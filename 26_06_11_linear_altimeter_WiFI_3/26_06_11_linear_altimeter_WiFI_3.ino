// 6/11/26 notes
//still doesn't work right - eventually settle down a little
//try shortening times from 300 cycles to 100
//better, but too short for calibration - try 150, add delay between calibration 
//and display - better, still too short on 500 ft, try 200
// present times 0 feet 200, 500 feet 300, altitude 100 pretty good
//next try linMin to 820 from 900 - OK
//now try 500 ft 350 - OK
//now try linMax to 2200 us -OK
//try 2250 -OK, try 375 to 2150
//now try LinPW to 820
//everything works now!


//6/10/26 notes
//this is the program that Copilot modified

// 6/9/26 notes
//use this for linear only
//doesn't presently work
//plan - remove all dial related, check function - no
// dial works, try using that method to calculate PW

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* apiKey = API_KEY;

const char* city = "Albany,OR,US";
const char* weatherHost = "api.openweathermap.org";

#define LIN 10 //PWM for linear slider

// Calibration values
int LinMin = 820;   //pulse width for zero feet on slider
int LinMax = 2150;  //pulse width for 500 feet on slider

float barometricPressure = 30.02;  //will be updated from API (in-Hg)

int LinPW = 820;
int i = 0;            //for counting the servo pulses
int c = 1;            //for counting the slider calibration cycles

Adafruit_BMP280 bmp;  // I2C
int HEIGHT;

bool calibrationDone = false;  // Track if calibration has run

unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_UPDATE_INTERVAL = 60000;  // 10 minutes - 60 sec now


//******************************************************************** setup
void setup() {
  pinMode(LIN,OUTPUT); //linear slider output
    digitalWrite(LIN, LOW); //added 2/11/21 - not sure this is needed

  Serial.begin(9600);

  delay(2000);
  Serial.println(F("\n\nBMP280 Altimeter with WiFi Barometric Pressure"));

  //Initialize BMP280
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1)
      ;
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  pinMode(LED_BUILTIN, OUTPUT);

  connectToWiFi();
}




//*********************************************************************** main loop
void loop() {

  //Fetch updated barometric pressure every 1 minutes
  if (millis() - lastWeatherUpdate > WEATHER_UPDATE_INTERVAL) {
    fetchWeatherData();
    lastWeatherUpdate = millis();
  }

  Serial.print(F("Temperature = "));  // added 6/6/26
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");

  Serial.print(F("Pressure = "));
  Serial.print(bmp.readPressure());
  Serial.println(" Pa");

  Serial.print(F("Barometric Pressure (from API) = "));
  Serial.print(barometricPressure);
  Serial.println(" in-Hg");

  Serial.print(F("Approx altitude = "));
  Serial.print(bmp.readAltitude(barometricPressure * 33.86));
  Serial.println(" m");
  Serial.print(bmp.readAltitude(barometricPressure * 33.86) * 3.2808);
  Serial.println(" feet");

  Serial.println();

  HEIGHT = (bmp.readAltitude(barometricPressure * 33.86)) * 3.2808;  //height in feet

  // Run calibration sequence only once at startup
  if (!calibrationDone) {
    Serial.println(F("\n=== CALIBRATION SEQUENCE ===\n"));
    
    runPulseSequence(LinMin, 200, "zero feet");
    delay(1000);

    runPulseSequence(LinMax, 375, "500 feet");
    delay(1000);

    calibrationDone = true;
    Serial.println(F("=== CALIBRATION COMPLETE ===\n"));
  }

delay(2000);
  // Continuously update altitude position
  Serial.println(F(" to altitude"));
  LinPW = LinMin + ((LinMax - LinMin) * HEIGHT / 500.0);  // Linear interpolation
  if (LinPW > LinMax) {LinPW = LinMax;}
  if (LinPW < LinMin) {LinPW = LinMin;}
  
  Serial.print(F("Pulse Width: "));
  Serial.println(LinPW);
  Serial.println("");

  runPulseSequence(LinPW, 100, "altitude");
  
  delay(5000);

}   

//******************************************************************* functions

void runPulseSequence(int pw, int pulseCount, const char* label) {
  Serial.print(F(" to "));
  Serial.println(label);
  for (i = 0; i < pulseCount; i++) {
    digitalWrite(LIN, HIGH);
    delayMicroseconds(pw);
    digitalWrite(LIN, LOW);
    delay(20);
  }
}

void connectToWiFi() {
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi connected!"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    Serial.println(F("Failed to connect to WiFi"));
  }
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi not connected, skipping weather update"));
    return;
  }

  Serial.println(F("Fetching weather data from OpenWeatherMap..."));

  WiFiClient client;

  if (!client.connect(weatherHost, 80)) {
    Serial.println(F("Failed to connect to weather server"));
    return;
  }

  //Build the request URL
  String url = "/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey) + "&units=metric";

  //Send HTTP request
  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: " + String(weatherHost) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  //Read response
  String response = "";
  unsigned long timeout = millis() + 5000;  // 5 second timeout

  while (client.connected() || client.available()) {
    if (client.available()) {
      char c = client.read();
      response += c;
    }
    if (millis() > timeout) break;
  }

  client.stop();

  //Extract JSON from response (skip HTTP headers)
  int jsonStart = response.indexOf('{');
  if (jsonStart != -1) {
    String jsonData = response.substring(jsonStart);
    parseWeatherResponse(jsonData);
  } else {
    Serial.println(F("No JSON found in response"));
  }
}


void parseWeatherResponse(String response) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print(F("JSON parse error: "));
    Serial.println(error.f_str());
    return;
  }

  //Extract barometric pressure (main.pressure is in hPa, convert to in-Hg)
  if (doc.containsKey("main") && doc["main"].containsKey("pressure")) {
    float pressureHpa = doc["main"]["pressure"];
    barometricPressure = pressureHpa * 0.02953;  // Convert hPa to in-Hg

    Serial.print(F("Updated barometric pressure: "));
    Serial.print(barometricPressure);
    Serial.println(F(" in-Hg"));
  } else {
    Serial.println(F("Pressure data not found in response"));
  }
}
