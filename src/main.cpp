#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWiFiManager.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#define NUM_LEDS 156
#define DATA_PIN 4

//Object creation

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);
time_t getNtpTime();
CRGB leds[NUM_LEDS];
DynamicJsonDocument clockDoc(1024);
DynamicJsonDocument backgroundDoc(1024);
DynamicJsonDocument oldClockDoc(1024);
DynamicJsonDocument oldBackgroundDoc(1024);

String clockValue;
String backgroundValue;

byte es[] = {0, 2};
byte ist[] = {3, 3};
byte fuenf[] = {7, 4};
byte zehn[] = {18, 4};
byte zwanzig[] = {11, 7};
byte dreiviertel[] = {22, 11};
byte viertel[] = {26, 7};
byte vor[] = {41, 3};
byte nach[] = {33, 4};
byte halb[] = {44, 4};
byte elf[] = {49, 3};
byte fuenf2[] = {51, 4};
byte ein[] = {63, 3};
byte eins[] = {62, 4};
byte zwei[] = {55, 4};
byte drei[] = {66, 4};
byte vier[] = {73, 4};
byte sechs[] = {83, 5};
byte acht[] = {77, 4};
byte sieben[] = {88, 6};
byte zwoelf[] = {94, 5};
byte zehn2[] = {106, 4};
byte neun[] = {103, 4};
byte uhr[] = {99, 3};
byte extraArr[] = {103, 3};

bool clockRainbow = false;
bool backgroundRainbow = false;

byte ledList[NUM_LEDS];

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

bool isLastSundayOver(int weekday, int day){
  int daysToSunday = 7 - weekday;
  int daysLeft = 31 - day;
  if(daysLeft > 6){
    return false;
  }
  else if((day + daysToSunday) > 31){
    return true;
  }
  else if((day == 31) && (weekday == 6)){
    return true;
  }
  else{
    return false;
  }
}

int getTimeOffset()
{
  int dateMonth = month(timeClient.getEpochTime());
  int dateDay = day(timeClient.getEpochTime());
  int weekday = timeClient.getDay();
  if ((dateMonth > 3) && (dateMonth < 10))
  {
    return 1;
  }
  else if ((dateMonth == 3) && isLastSundayOver(weekday, dateDay))
  {
    return 1;
  }
  else if (dateMonth == 10 && isLastSundayOver(weekday, dateDay)){
    return 0;
  }
  else{
    return 0;
  }
}

void notifyClients(int dataType)
{
  switch (dataType)
  {
  case 1:
    ws.textAll(clockValue);
    break;
  case 2:
    ws.textAll(backgroundValue);
    break;
  case 3:
    ws.textAll("clRa" + String(clockRainbow));
    break;
  case 4:
    ws.textAll("baRa" + String(backgroundRainbow));
    break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    String compStr = (char *)data;
    if (compStr.indexOf("clock") > -1)
    {
      clockValue = compStr;
      compStr.remove(0, 5);
      if (!clockRainbow)
      {
        deserializeJson(clockDoc, compStr);
      }
      notifyClients(1);
    }
    else if (compStr.indexOf("disconnect") > -1)
    {
      WiFi.disconnect();
      ESP.restart();
    }
    else if (compStr.indexOf("clRa") > -1)
    {
      if (clockRainbow)
      {
        clockRainbow = false;
      }
      else
      {
        clockRainbow = true;
      }
      notifyClients(3);
    }
    else if (compStr.indexOf("baRa") > -1)
    {
      if (backgroundRainbow)
      {
        backgroundRainbow = false;
      }
      else
      {
        backgroundRainbow = true;
      }
      notifyClients(4);
    }
    else
    {
      backgroundValue = compStr;
      compStr.remove(0, 10);
      if (!backgroundRainbow)
      {
        deserializeJson(backgroundDoc, compStr);
      }
      notifyClients(2);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String &var)
{
  if (var == "clockBrightness")
  {
    return clockValue;
  }
  else
  {
    return backgroundValue;
  }
}

void selectLeds(byte arr[])
{
  for (int i = 0; i < arr[1]; i++)
  {
    ledList[i + arr[0]] = 1;
  }
}

void unselectLeds(byte arr[])
{
  for (int i = 0; i < arr[1]; i++)
  {
    ledList[i + arr[0]] = 0;
  }
}

void enableLeds()
{
  if (clockRainbow)
  {
    clockDoc["s"] = 100;
    if (clockDoc["h"] < 360)
    {
      clockDoc["h"] = int(clockDoc["h"]) + 1;
    }
    else
    {
      clockDoc["h"] = 0;
    }
    CHSV clockColor = CHSV(map(clockDoc["h"], 0, 360, 0, 255), map(clockDoc["s"], 0, 100, 0, 255), map(clockDoc["v"], 0, 100, 0, 255));
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (ledList[i] == 1)
      {
        leds[i] = clockColor;
      }
      else if (ledList[i] == 0)
      {
        leds[i] = CRGB::Black;
      }
    }
    delay(25);
  }
  else
  {
    if (clockDoc != oldClockDoc)
    {
      float hueDiff = float(clockDoc["h"]) - float(oldClockDoc["h"]);
      float satDiff = float(clockDoc["s"]) - float(oldClockDoc["s"]);
      float valDiff = float(clockDoc["v"]) - float(oldClockDoc["v"]);
      for (int i = 1; i < 101; i++)
      {
        int hue = int(oldClockDoc["h"]) + (i * (hueDiff / 100));
        int sat = int(oldClockDoc["s"]) + (i * (satDiff / 100));
        int val = int(oldClockDoc["v"]) + (i * (valDiff / 100));
        CHSV values = CHSV(map(hue, 0, 360, 0, 255), map(sat, 0, 100, 0, 255), map(val, 0, 100, 0, 255));
        for (int i = 0; i < NUM_LEDS; i++)
        {
          if (ledList[i] == 1)
          {
            leds[i] = values;
          }
          else if (ledList[i] == 0)
          {
            leds[i] = CRGB::Black;
          }
        }
        FastLED.show();
        delay(5);
      }
      oldClockDoc = clockDoc;
    }
    else
    {
      CHSV clockColor = CHSV(map(clockDoc["h"], 0, 360, 0, 255), map(clockDoc["s"], 0, 100, 0, 255), map(clockDoc["v"], 0, 100, 0, 255));
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (ledList[i] == 1)
        {
          leds[i] = clockColor;
        }
        else if (ledList[i] == 0)
        {
          leds[i] = CRGB::Black;
        }
      }
    }
  }
}

void changeBackground()
{
  if (backgroundRainbow)
  {
    backgroundDoc["s"] = 100;
    if (backgroundDoc["h"] < 360)
    {
      backgroundDoc["h"] = int(backgroundDoc["h"]) + 1;
    }
    else
    {
      backgroundDoc["h"] = 0;
    }
    CHSV backgroundColor = CHSV(map(backgroundDoc["h"], 0, 360, 0, 255), map(backgroundDoc["s"], 0, 100, 0, 255), map(backgroundDoc["v"], 0, 100, 0, 100));
    for (int i = 110; i < 156; i++)
    {
      leds[i] = backgroundColor;
      delay(25);
    }
  }
  else
  {
    if (backgroundDoc != oldBackgroundDoc)
    {
      float hueDiff = float(backgroundDoc["h"]) - float(oldBackgroundDoc["h"]);
      float satDiff = float(backgroundDoc["s"]) - float(oldBackgroundDoc["s"]);
      float valDiff = float(backgroundDoc["v"]) - float(oldBackgroundDoc["v"]);
      for (float i = 1; i < 101; i++)
      {
        int hue = int(oldBackgroundDoc["h"]) + (i * (hueDiff / 100));
        int sat = int(oldBackgroundDoc["s"]) + (i * (satDiff / 100));
        int val = int(oldBackgroundDoc["v"]) + (i * (valDiff / 100));
        CHSV values = CHSV(map(hue, 0, 360, 0, 255), map(sat, 0, 100, 0, 255), map(val, 0, 100, 0, 255));
        for (int i = 110; i < 156; i++)
        {
          leds[i] = values;
        }
        FastLED.show();
        delay(5);
      }
      oldBackgroundDoc = backgroundDoc;
    }
    else
    {
      CHSV backgroundColor = CHSV(map(backgroundDoc["h"], 0, 360, 0, 255), map(backgroundDoc["s"], 0, 100, 0, 255), map(backgroundDoc["v"], 0, 100, 0, 255));
      for (int i = 110; i < 156; i++)
      {
        leds[i] = backgroundColor;
      }
    }
  }
}

void setup()
{
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.begin(115200);
  if (!LittleFS.begin())
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  DNSServer dns;
  AsyncWiFiManager wifiManager(&server, &dns);
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0), IPAddress(8,8,8,8), IPAddress(8,8,4,4));
  wifiManager.autoConnect("Wortuhr");
  Serial.println("WIFI-Connected");
  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false, processor);
  });
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.begin();
  timeClient.begin();
  for (int i = 0; i < NUM_LEDS; i++)
  {
    ledList[i] = 2;
  }
}

void loop()
{
  timeClient.update();
  int hour = timeClient.getHours() + getTimeOffset();
  int minute = timeClient.getMinutes();

  changeBackground();
  selectLeds(es);
  selectLeds(ist);

  if ((minute >= 5) && (minute < 10))
  { //fuenf nach
    selectLeds(fuenf);
    selectLeds(nach);
    unselectLeds(uhr);
  }

  if ((minute >= 10) && (minute < 15))
  { //zehn nach
    selectLeds(zehn);
    selectLeds(nach);
    unselectLeds(fuenf);
  }

  if ((minute >= 15) && (minute < 20))
  { //viertel nach
    selectLeds(viertel);
    selectLeds(nach);
    unselectLeds(zehn);
  }

  if ((minute >= 20) && (minute < 25))
  { //zwanzig nach
    selectLeds(zwanzig);
    selectLeds(nach);
    unselectLeds(viertel);
  }

  if ((minute >= 25) && (minute < 30))
  { //5 vor halb
    selectLeds(fuenf);
    selectLeds(vor);
    selectLeds(halb);
    unselectLeds(zwanzig);
    unselectLeds(nach);
  }

  if ((minute >= 30) && (minute < 35))
  { //halb
    selectLeds(halb);
    unselectLeds(fuenf);
    unselectLeds(vor);
  }

  if ((minute >= 35) && (minute < 40))
  { //fuenf nach halb
    selectLeds(halb);
    selectLeds(fuenf);
    selectLeds(nach);
  }

  if ((minute >= 40) && (minute < 45))
  { //zwanzig vor
    selectLeds(zwanzig);
    selectLeds(vor);
    unselectLeds(fuenf);
    unselectLeds(halb);
    unselectLeds(nach);
  }

  if ((minute >= 45) && (minute < 50))
  { //dreiviertel
    selectLeds(dreiviertel);
    unselectLeds(zwanzig);
    unselectLeds(vor);
  }

  if ((minute >= 50) && (minute < 55))
  { //zehn vor
    selectLeds(zehn);
    selectLeds(vor);
    unselectLeds(dreiviertel);
  }

  if (minute >= 55)
  { //fuenf vor
    selectLeds(fuenf);
    selectLeds(vor);
    unselectLeds(zehn);
  }

  if ((minute >= 0) && (minute < 5))
  { //volle Stunde
    selectLeds(uhr);
    unselectLeds(fuenf);
    unselectLeds(vor);
  }

  if (minute >= 25)
  {
    hour = hour + 1;
  }

  switch (hour)
  {
  case 1:
  case 13:
  {
    unselectLeds(zwoelf);
    if (minute < 5)
      selectLeds(ein);
    else
      selectLeds(eins);
    break;
  }
  case 2:
  case 14:
    unselectLeds(eins);
    selectLeds(zwei);
    break;

  case 3:
  case 15:
    unselectLeds(zwei);
    selectLeds(drei);
    break;
  case 4:
  case 16:
    unselectLeds(drei);
    selectLeds(vier);
    break;
  case 5:
  case 17:
    unselectLeds(vier);
    selectLeds(fuenf2);
    break;
  case 6:
  case 18:
    unselectLeds(fuenf2);
    selectLeds(sechs);
    break;
  case 7:
  case 19:
    unselectLeds(sechs);
    selectLeds(sieben);
    break;
  case 8:
  case 20:
    unselectLeds(sieben);
    selectLeds(acht);
    break;
  case 9:
  case 21:
    unselectLeds(acht);
    selectLeds(neun);
    break;
  case 10:
  case 22:
    unselectLeds(extraArr);
    selectLeds(zehn2);
    break;
  case 11:
  case 23:
    unselectLeds(zehn2);
    selectLeds(elf);
    break;
  case 12:
  case 24:
  case 0:
    unselectLeds(elf);
    selectLeds(zwoelf);
    break;
  }

  ws.cleanupClients();
  enableLeds();
  FastLED.show();
}