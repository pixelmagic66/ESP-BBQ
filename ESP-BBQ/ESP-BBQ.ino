// Original code and PCB design by KMelsen - https://github.com/KMelsen/ESP-BBQ
// 
// Adapted by PixelMagic 2022
//
// I used the FireBeetle32 as board
//
// Please define your WiFi and MQTT credentials
// MQTT topic will be bbqtemp/probeX
//
// JSON output is available at http://yourip/json 
//
// Holding down the GPIO/BOOT button for 5 seconds will signal a Offline message

// Include libraries
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// OLED information
#define I2C_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Number of ADC samples for temperature calculation
const int samples = 50;

// Define input channels
#define CH0_INPUT 33 // probes
#define CH1_INPUT 32
#define CH2_INPUT 35
#define CH3_INPUT 34
#define CH4_INPUT 39
#define CH5_INPUT 36 // probes
#define BTN_STOP 0

// WiFi information
const char* ssid = "Wifiname";
const char* password = "WifiPassword;

// MQTT Broker
const char *mqtt_broker = "broker_address";
const char *mqtt_username = "MQTT-username";
const char *mqtt_password = "MQTT-password";
const int mqtt_port = 1883;

// deep sleep
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  0        /* Time ESP32 will go to sleep (in seconds) */

// Define resistor and steinhart values
float RESISTOR = 99700;
float STEINHART_A = 0.5508197510e-3;
float STEINHART_B = 2.381337093e-4;
float STEINHART_C = 0.3546777196e-7;

// String temperature variables
String cal_temp_ch0_F = "";
String cal_temp_ch0_C = "";
String cal_temp_ch1_F = "";
String cal_temp_ch2_F = "";
String cal_temp_ch3_F = "";
String cal_temp_ch4_F = "";
String cal_temp_ch5_F = "";

// Splash screen logo bitmap, 128x64px
const unsigned char splash_bitmap [] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x07, 0xc0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0xe0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x70, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x38, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xf8, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xf8, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x60, 0x1b, 0x01, 0xfd, 0x81, 0xfc, 0xfe, 0x3f, 0x80, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x60, 0x18, 0x01, 0x80, 0x01, 0x86, 0xc3, 0x60, 0xc0, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x33, 0x30, 0x01, 0x80, 0x01, 0x86, 0xc3, 0x60, 0xc0, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x33, 0x33, 0x01, 0x81, 0x81, 0x86, 0xc3, 0x60, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x33, 0x33, 0x01, 0xf9, 0x81, 0xfc, 0xfe, 0x60, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x1f, 0xe3, 0x3d, 0x81, 0x81, 0x86, 0xc3, 0x60, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x1f, 0xe3, 0x01, 0x81, 0x81, 0x86, 0xc3, 0x63, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x0c, 0xc3, 0x01, 0x81, 0x81, 0x86, 0xc3, 0x61, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x0c, 0xc3, 0x01, 0x81, 0x81, 0xfc, 0xfe, 0x3f, 0x80, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x7f, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01, 0x98, 0x00, 
0x00, 0x0c, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x0c, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0xcf, 0xff, 0x98, 0x00, 
0x00, 0x0c, 0x3f, 0x1e, 0x7b, 0xfe, 0x3c, 0xff, 0x8f, 0x39, 0xe7, 0x80, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x0c, 0x39, 0xb3, 0x63, 0x33, 0x66, 0xcc, 0xd9, 0xb3, 0x36, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x0c, 0x31, 0xbf, 0x63, 0x33, 0x66, 0xcc, 0xdf, 0xb3, 0xf6, 0x00, 0xff, 0xff, 0xf8, 0x00, 
0x00, 0x0c, 0x31, 0xb0, 0x63, 0x33, 0x66, 0xcc, 0xd8, 0x33, 0x06, 0x00, 0xff, 0xff, 0xf8, 0x00, 
0x00, 0x0c, 0x31, 0xb3, 0x63, 0x33, 0x66, 0xcc, 0xd9, 0xb3, 0x36, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x0c, 0x31, 0x9e, 0x63, 0x33, 0x3c, 0xcc, 0xcf, 0x19, 0xe6, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x38, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x70, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0xe0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x07, 0xc0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xc0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0xf8, 0x60, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x60, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x60, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x60, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x30, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x30, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x1c, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x0c, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x04, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


AsyncWebServer server(80);

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Functions
double analogReadAverage(int channel_pin) {
  int i;
  int maxValue = 0;
  int minValue = 5000;
  double sumValue = 0;
  double result;
  int reading;
  for (i = 0; i < 8; i++) {
    reading = analogRead(channel_pin);
    sumValue += (double)reading;
    if (reading > maxValue) maxValue = reading;
    if (reading < minValue) minValue = reading;
  }
  result = (sumValue - (double)maxValue - (double)minValue) / 6;
  return result;
}

double calculateResistance(int channel_pin) {
  double resistanceNTC = 0;
  int x;
  for (x = 0; x < samples; x++) {
    resistanceNTC += analogReadAverage(channel_pin);
  }
  resistanceNTC /= samples;
  resistanceNTC = (float)(RESISTOR * resistanceNTC) / (4095 - resistanceNTC);
  return resistanceNTC;
}

String calTempF(int channel_pin) {
  float R = calculateResistance(channel_pin);
  float temp_K = 1.0 / (STEINHART_A + (STEINHART_B * log(R)) + (STEINHART_C * pow(log(R), 3.0)));
  float temp_C = round(temp_K - 273.15);
  int cal_temp_C = round(4.95152 + 0.7959 * temp_C);
  if (cal_temp_C < -100) {
    String cal_temp_F = "No probe connected";
    return String(cal_temp_F);
  } else {
    String cal_temp_F = String(round(cal_temp_C * 9 / 5 + 32));
    return String(cal_temp_C);
  }
}

// html code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 15px auto;
     text-align: center;
    }
  </style>
  <title>ESP32 Boretti Kamado temperatuur</title>
</head>
<body>
  <h1>ESP32 Boretti Kamado temperatuur</h1>
  <table style="width: 80.1867; border-collapse: collapse; margin-left: auto; margin-right: auto;" border="0">
  <tbody>
    <tr>
    <td><strong>Channel 0&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch0tempf" style="text-align:right;">%CH0TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
    <tr>
    <td><strong>Channel 1&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch1tempf" style="text-align:right;">%CH1TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
    <tr>
    <td><strong>Channel 2&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch2tempf" style="text-align:right;">%CH2TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
    <tr>
    <td><strong>Channel 3&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch3tempf" style="text-align:right;">%CH3TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
    <tr>
    <td><strong>Channel 4&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch4tempf" style="text-align:right;">%CH4TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
    <tr>
    <td><strong>Channel 5&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong></td>
    <td id="ch5tempf" style="text-align:right;">%CH5TEMPF%</td>
    <td>&nbsp;&nbsp;&deg;C</td>
    </tr>
  </tbody>
  </table>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch0tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch0tempf", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch1tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch1tempf", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch2tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch2tempf", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch3tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch3tempf", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch4tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch4tempf", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ch5tempf").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/ch5tempf", true);
  xhttp.send();
}, 10000) ;
</script>
</html>)rawliteral";

// Replace html placeholders
String processor(const String& var){
  //Serial.println(var);
  if (var == "CH0TEMPF"){
    return cal_temp_ch0_F;
  }
  else if (var == "CH1TEMPF"){
    return cal_temp_ch1_F;
  }
  else if (var == "CH2TEMPF") {
    return cal_temp_ch2_F;
  }
  else if (var == "CH3TEMPF") {
    return cal_temp_ch3_F;
  }
  else if (var == "CH4TEMPF") {
    return cal_temp_ch4_F;
  }
  else if (var == "CH5TEMPF") {
    return cal_temp_ch5_F;
  }
  return String();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // setup display
  analogSetAttenuation(ADC_11db);
  display.begin(I2C_ADDRESS, true);
  delay(1000);
  display.clearDisplay();
  display.drawBitmap(0, 0, splash_bitmap, 128, 64, SH110X_WHITE);
  display.display();
  delay(3000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  // setup WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  display.println("Connecting to WiFi");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }
  Serial.println();
  display.clearDisplay();
  display.setCursor(0, 0);
  Serial.println(WiFi.localIP());
  display.println("Available at");
  display.println();
  display.println(WiFi.localIP());
  display.display();
  delay(3000);
  // setup MQTT
  mqtt.setServer(mqtt_broker, mqtt_port);
  while (!mqtt.connected()) {
    String mqtt_id = "esp32-client-";
    mqtt_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", mqtt_id.c_str());
    if (mqtt.connect(mqtt_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("Public mqtt broker connected");
        display.println("MQTT Connected");
        display.display();
        mqtt.publish("bbqtemp/status", "Online");
    } else {
        Serial.print("failed with state ");
        Serial.print(mqtt.state());
        delay(2000);
    }
    
  } 

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/ch0tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH0_INPUT).c_str());
  });
  server.on("/ch1tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH1_INPUT).c_str());
  });
  server.on("/ch2tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH2_INPUT).c_str());
  });
  server.on("/ch3tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH3_INPUT).c_str());
  });
  server.on("/ch4tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH4_INPUT).c_str());
  });
  server.on("/ch5tempf", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", calTempF(CH5_INPUT).c_str());
  });
  // json output
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
        json += "{";
        json +=  "\"probe0\":\""+calTempF(CH0_INPUT)+"\"";
        json += ",\"probe1\":\""+calTempF(CH1_INPUT)+"\"";
        json += ",\"probe2\":\""+calTempF(CH2_INPUT)+"\"";
        json += ",\"probe3\":\""+calTempF(CH3_INPUT)+"\"";
        json += ",\"probe4\":\""+calTempF(CH4_INPUT)+"\"";
        json += ",\"probe5\":\""+calTempF(CH5_INPUT)+"\"";
        json += "}";     
    json += "]";
    request->send(200, "application/json", json);
    json = String();
  });
  pinMode(BTN_STOP,INPUT_PULLUP);
  // Start server
  server.begin();
}

void loop() {
  // check if button is pressed
  
     if(!digitalRead(BTN_STOP)){
        Serial.println("Offline signaled");
        mqtt.publish("bbqtemp/status", "Offline" );
        display.clearDisplay();   
        display.setCursor(0, 1);   
        display.println("Shutdown completed");
        display.display();
        delay(1000);
        esp_deep_sleep_start();
    }   
    
  // Clear display and set cursor for printing data
  display.clearDisplay();
  display.setCursor(0, 1);

  // Calculate thermistor resistances.
  float r_ch_0 = calculateResistance(CH0_INPUT);
  float r_ch_1 = calculateResistance(CH1_INPUT);
  float r_ch_2 = calculateResistance(CH2_INPUT);
  float r_ch_3 = calculateResistance(CH3_INPUT);
  float r_ch_4 = calculateResistance(CH4_INPUT);
  float r_ch_5 = calculateResistance(CH5_INPUT);
  
  // Calculate temperatures in K using Steinhart-Hart
  float temp_k_ch_0 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_0)) + (STEINHART_C * pow(log(r_ch_0), 3.0)));
  float temp_k_ch_1 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_1)) + (STEINHART_C * pow(log(r_ch_1), 3.0)));
  float temp_k_ch_2 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_2)) + (STEINHART_C * pow(log(r_ch_2), 3.0)));
  float temp_k_ch_3 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_3)) + (STEINHART_C * pow(log(r_ch_3), 3.0)));
  float temp_k_ch_4 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_4)) + (STEINHART_C * pow(log(r_ch_4), 3.0)));
  float temp_k_ch_5 = 1.0 / (STEINHART_A + (STEINHART_B * log(r_ch_5)) + (STEINHART_C * pow(log(r_ch_5), 3.0)));

  // Convert to C and correct using calibration curve
  float temp_ch_0 = round(temp_k_ch_0 - 273.15);
  float temp_ch_1 = round(temp_k_ch_1 - 273.15);
  float temp_ch_2 = round(temp_k_ch_2 - 273.15);
  float temp_ch_3 = round(temp_k_ch_3 - 273.15);
  float temp_ch_4 = round(temp_k_ch_4 - 273.15);
  float temp_ch_5 = round(temp_k_ch_5 - 273.15);
  int cal_temp_ch_0 = round(4.95152 + 0.7959 * temp_ch_0); 
  int cal_temp_ch_1 = round(4.95152 + 0.7959 * temp_ch_1); 
  int cal_temp_ch_2 = round(4.95152 + 0.7959 * temp_ch_2);
  int cal_temp_ch_3 = round(4.95152 + 0.7959 * temp_ch_3);  
  int cal_temp_ch_4 = round(4.95152 + 0.7959 * temp_ch_4); 
  int cal_temp_ch_5 = round(4.95152 + 0.7959 * temp_ch_5);
  int cal_temp_ch_0_F_OLED = round(cal_temp_ch_0 * 9 / 5 + 32);
  int cal_temp_ch_1_F_OLED = round(cal_temp_ch_1 * 9 / 5 + 32);
  int cal_temp_ch_2_F_OLED = round(cal_temp_ch_2 * 9 / 5 + 32);
  int cal_temp_ch_3_F_OLED = round(cal_temp_ch_3 * 9 / 5 + 32);
  int cal_temp_ch_4_F_OLED = round(cal_temp_ch_4 * 9 / 5 + 32);
  int cal_temp_ch_5_F_OLED = round(cal_temp_ch_5 * 9 / 5 + 32);

  String cal_temp_ch_0_F = calTempF(CH0_INPUT);
  String cal_temp_ch_1_F = calTempF(CH1_INPUT);
  String cal_temp_ch_2_F = calTempF(CH2_INPUT);
  String cal_temp_ch_3_F = calTempF(CH3_INPUT);
  String cal_temp_ch_4_F = calTempF(CH4_INPUT);
  String cal_temp_ch_5_F = calTempF(CH5_INPUT);
  
  // Print temperature on OLED display and publish to MQTT
  if (cal_temp_ch_0 < -200) {
    display.println("Ch 0: No probe");
    mqtt.publish("bbqtemp/probe0", "No Probe" ); 
  } else {
    display.println((String) "Ch 0: " + cal_temp_ch_0 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_0, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe0", charTemperature );        
  }
  display.setCursor(0, 12);
  if (cal_temp_ch_1 < -200) {
    display.println("Ch 1: No probe");
    mqtt.publish("bbqtemp/probe1", "No Probe" ); 
  } else {
    display.println((String) "Ch 1: " + cal_temp_ch_1 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_1, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe1", charTemperature );      
  }
  display.setCursor(0, 23);
  if (cal_temp_ch_2 < -200) {
    display.println("Ch 2: No probe");
    mqtt.publish("bbqtemp/probe2", "No Probe" ); 
  } else {
    display.println((String) "Ch 2: " + cal_temp_ch_2 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_2, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe2", charTemperature );      
  }
  display.setCursor(0, 34);
  if (cal_temp_ch_3 < -200) {
    display.println("Ch 3: No probe");
    mqtt.publish("bbqtemp/probe3", "No Probe" ); 
  } else {
    display.println((String) "Ch 3: " + cal_temp_ch_3 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_3, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe3", charTemperature );      
  }
  display.setCursor(0, 45);
  if (cal_temp_ch_4 < -200) {
    display.println("Ch 4: No probe");
    mqtt.publish("bbqtemp/probe4", "No Probe" ); 
  } else {
    display.println((String) "Ch 4: " + cal_temp_ch_4 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_4, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe4", charTemperature );      
  }
  display.setCursor(0, 56);
  if (cal_temp_ch_5 < -200) {
    display.println("Ch 5: No probe");
    mqtt.publish("bbqtemp/probe5", "No Probe" );     
  } else {
    display.println((String) "Ch 5: " + cal_temp_ch_5 + " C  ");
    char charTemperature[] = "00.0";
    dtostrf(cal_temp_ch_5, 4, 1, charTemperature);
    mqtt.publish("bbqtemp/probe5", charTemperature );      
  }
  display.display();
  // wait a little bit
  delay(5000);
}
