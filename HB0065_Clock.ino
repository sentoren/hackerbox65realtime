// HackerBox 0065 Clock Demo for 64x32 LED Array
// Adapted from SmartMatrix example file
//

#include "RTClib.h"
#include <MatrixHardware_ESP32_V0.h>    
#include <SmartMatrix.h>
#include <WiFi.h>
#include "time.h"
#include <OpenWeatherOneCall.h>

const char* ssid       = "YOUR_INTERNET";
const char* password   = "YOUR_PASSWORD";
#define ONECALLKEY "YOUR_API_KEY"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -28800;
const int   daylightOffset_sec = 0;

int yr = 0;
int mt = 0;
int dy = 0;
int hr = 0;
int mi = 0;
int se = 0;

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer1, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer2, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer3, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer4, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer5, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

RTC_DS1307 rtc;
const int defaultBrightness = (50*255)/100;     // dim: 35% brightness
char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char monthsOfTheYr[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

OpenWeatherOneCall OWOC;
unsigned long previousMillis = 0;
const unsigned long interval = 1UL*60UL*60UL*1000UL; //1hour
float tempString;

// WiFi Connect function **********************************
void connectWifi() {
  WiFi.begin(ssid, password);
  printf("Connecting.");
  int TryNum = 0;
  while (WiFi.status() != WL_CONNECTED) {
    printf(".");
    delay(200);
    TryNum++;
    if (TryNum > 20) {
      printf("\nUnable to connect to WiFi. Please check your parameters.\n");
      for (;;);
    }
  }
  printf("Connected to: % s\n\n", ssid);
} //================== END WIFI CONNECT =======================

void setup() {
  Serial.begin(57600);
  delay(1000);
  Serial.println("DS1307RTC Test");
  Wire.begin(14, 13);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // WiFi Connection required *********************
  while (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  } //<----------------End WiFi Connection Check
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // gets time from NTP Server and syncs RTC clock
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  yr = timeinfo.tm_year + 1900;
  mt = timeinfo.tm_mon + 1;
  dy = timeinfo.tm_mday;
  hr = timeinfo.tm_hour;
  mi = timeinfo.tm_min;
  se = timeinfo.tm_sec;

  rtc.adjust(DateTime(yr, mt, dy, hr, mi, se));

  //Weather call setup
  OWOC.setOpenWeatherKey(ONECALLKEY);
  OWOC.setExcl(EXCL_M+EXCL_D+EXCL_H+EXCL_A);
  OWOC.setLatLon();
  OWOC.parseWeather();

  printf("\nLocation: % s, % s % s\n", OWOC.location.CITY, OWOC.location.STATE, OWOC.location.COUNTRY);
  //Verify all other values exist before using
    if (OWOC.current)
    {
        tempString = OWOC.current->temperature;
        printf("Current Temp : % .0f\n", tempString);
        printf("Current Humidity : % .0f\n", OWOC.current->humidity);
    }

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // setup matrix
  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&indexedLayer1); 
  matrix.addLayer(&indexedLayer2);
  matrix.addLayer(&indexedLayer3);
  matrix.addLayer(&indexedLayer4);
  matrix.addLayer(&indexedLayer5);
  matrix.begin();

  matrix.setBrightness(defaultBrightness);
}

void loop() {
  char txtBuffer[12];
  char tempBuffer[4];
  DateTime now = rtc.now();

  // clear screen before writing new text
  backgroundLayer.fillScreen({0,0,0});
  indexedLayer1.fillScreen(0);
  indexedLayer2.fillScreen(0);
  indexedLayer3.fillScreen(0);
  indexedLayer4.fillScreen(0);
  indexedLayer5.fillScreen(0);

  backgroundLayer.fillCircle(32, 17, 4, {0xff, 0xff, 0x00}, {0x05, 0xfd, 0xe8});
  backgroundLayer.drawRectangle(0,0,63,31,{0xff, 0xff, 0x00});
  backgroundLayer.swapBuffers();

  sprintf(txtBuffer, "%02d:%02d:%02d", (now.hour()+1), now.minute(), now.second());
  indexedLayer1.setFont(font8x13);
  indexedLayer1.setIndexedColor(1,{0x05, 0xfd, 0xe8});
  indexedLayer1.drawString(0, 0, 1, txtBuffer);
  indexedLayer1.swapBuffers();
  indexedLayer2.setFont(font8x13);
  indexedLayer2.setIndexedColor(1,{0xff, 0xff, 0x00});
  indexedLayer2.drawString(4, 11, 1, daysOfTheWeek[now.dayOfTheWeek()]);
  indexedLayer2.swapBuffers();
  sprintf(txtBuffer, "%02d %s %04d", now.day(), monthsOfTheYr[(now.month()-1)], now.year());
  indexedLayer3.setFont(font5x7);
  indexedLayer3.setIndexedColor(1,{0xbf, 0x6b, 0x41});
  indexedLayer3.drawString(5, 25, 1, txtBuffer); 
  indexedLayer3.swapBuffers();
  indexedLayer4.setFont(font8x13);
  indexedLayer4.setIndexedColor(1,{0xff, 0xff, 0x00});
  sprintf(tempBuffer, "%d", (int)tempString, (int)(tempString*100)%100);
  indexedLayer4.drawString(40, 11, 1, tempBuffer);
  indexedLayer4.swapBuffers();
  indexedLayer5.setFont(font3x5);
  indexedLayer5.setIndexedColor(1,{0xff, 0xff, 0x00});
  indexedLayer5.drawString(55, 11, 1, "o");
  indexedLayer5.swapBuffers();

  delay(500);

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the time you should have toggled the LED
    previousMillis += interval;

    while (WiFi.status() != WL_CONNECTED) {
      connectWifi();
  
      OWOC.parseWeather();
  
      printf("\nLocation: % s, % s % s\n", OWOC.location.CITY, OWOC.location.STATE, OWOC.location.COUNTRY);
      //Verify all other values exist before using
      if (OWOC.current)
      {
        tempString = OWOC.current->temperature;
        printf("Current Temp : % .0f\n", tempString);
        printf("Current Humidity : % .0f\n", OWOC.current->humidity);
      }
    }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  }
}
