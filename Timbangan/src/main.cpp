#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define WIFI_SSID "MUSRAM"
#define WIFI_PASSWORD "musram1234"

// #define MQTT_HOST "ec2-54-147-187-160.compute-1.amazonaws.com"
#define MQTT_HOST "broker.emqx.io"
#define MQTT_PORT 1883

#define CAL_FACTOR 440.00

#define DOUT D5
#define CLK D6

//notifications LEDs
#define _CON_DELAY 5000
#define _THRESHOLD_WEIGHT 0
#define _DETECTION_DELAY 2000

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

Ticker wifiReconnectTimer;

HX711 scale(DOUT, CLK);

int detection_time = 0;
double lastWeight;
double tempgram;
char publishValue[10];

double measured_weight;
boolean break_weight_loop = true;

IPAddress ip(192, 168, 0, 113);
IPAddress gateway_dns(192, 168, 0, 1);

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.config(ip, gateway_dns, gateway_dns);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach();
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}
void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void setup()
{
  scale.set_scale(CAL_FACTOR);
  scale.tare();
  scale.power_down();
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  lcd.begin();
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  connectToWifi();
}

void loop()
{
  scale.power_up();
  measured_weight = scale.get_units(10);
  if (measured_weight > _THRESHOLD_WEIGHT && ((millis() - detection_time) > _CON_DELAY))
  {
    break_weight_loop = true;
    if (measured_weight != tempgram)
    {
      lcd.clear();
    }
    
      int GRAM = measured_weight;
      lcd.setCursor(0, 0);
      lcd.print(String(GRAM) + String(" gram"));
      lcd.setCursor(0, 1);
      lcd.print(" Timbangan Slur ");
      Serial.println("Weight detected.");
      tempgram = measured_weight;

    detection_time = millis();
    while (break_weight_loop)
    {
      delay(40);
      lastWeight = scale.get_units();

      if (lastWeight < _THRESHOLD_WEIGHT)
      {
        break_weight_loop = false;
      }
      else
        measured_weight = lastWeight;
      if (((millis() - detection_time) > _DETECTION_DELAY))
      {
        break_weight_loop = false;
        sprintf(publishValue, "%.1f", measured_weight);
        yield();
        mqttClient.publish("uas/weight", 2, true, publishValue);
      }
    }
  }
  scale.power_down();
  delay(1000);
  if (((millis() - detection_time) > _CON_DELAY))
  {
    delay(500);
  }
}