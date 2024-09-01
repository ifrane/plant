#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
WiFiMulti WiFiMulti;
#include <Wire.h>
#include "Adafruit_seesaw.h"
#include <NTPClient.h>
#include <PubSubClient.h>
#include "Adafruit_SHT31.h" //https://github.com/adafruit/Adafruit_SHT31/archive/master.zip
Adafruit_SHT31 sht31(&Wire1);
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

const char* ssid = "redacted"; //Your wifi network's SSID
const char* password = "redacted"; //Your wifi network's password
const char* mqtt_server = "192.168.0.22";  //Your network's MQTT server (usually same IP address as Home Assistant server)
// URL of the JSON file on GitHub
const char* json_url = "https://raw.githubusercontent.com/ifrane/plant/main/plant_configs.json";


Adafruit_seesaw ss1, ss2, ss3, ss4, ss21(&Wire1), ss22(&Wire1);
const int pump36 = 16;
const int pump37 = 17;
const int pump38 = 19;
const int pump39 = 25;
const int pump36_2 = 26;
const int pump37_2 = 27;

int sensor36_thres = 700;
int sensor37_thres = 700;
int sensor38_thres = 530;
int sensor39_thres = 530;
int sensor36_2_thres = 700;
int sensor37_2_thres = 530;

int pump36_ontime = 4000;
int pump37_ontime = 4000;
int pump38_ontime = 5000;
int pump39_ontime = 5000;
int pump36_2_ontime = 8000;
int pump37_2_ontime = 5000;

int pump36_enable = 1;
int pump37_enable = 1;
int pump38_enable = 0;
int pump39_enable = 0;
int pump36_2_enable = 1;
int pump37_2_enable = 0;

int pump36_waittime = 2;
int pump37_waittime = 2;
int pump38_waittime = 2;
int pump39_waittime = 2;
int pump36_2_waittime = 2;
int pump37_2_waittime = 2;

unsigned long  pump36_previousMils;
unsigned long  pump37_previousMils;
unsigned long  pump38_previousMils;
unsigned long  pump39_previousMils;
unsigned long  pump36_2_previousMils;
unsigned long  pump37_2_previousMils;

int overflow = 0;
int currenthour;
int daytime;
unsigned long timeDiff;
unsigned long mils;

// Min and max sensor values
const int sensorMin = 340;
const int sensorMax = 900;

// Arrays to store last three readings of each sensor
uint16_t capread36_history[3] = {sensor36_thres, sensor36_thres, sensor36_thres};
uint16_t capread37_history[3] = {sensor37_thres, sensor37_thres, sensor37_thres};
uint16_t capread38_history[3] = {sensor38_thres, sensor38_thres, sensor38_thres};
uint16_t capread39_history[3] = {sensor39_thres, sensor39_thres, sensor39_thres};
uint16_t capread36_2_history[3] = {sensor36_2_thres, sensor36_2_thres, sensor36_2_thres};
uint16_t capread37_2_history[3] = {sensor37_2_thres, sensor37_2_thres, sensor37_2_thres};


// Variables for NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // Your time zone's offset in seconds
const int daylightOffset_sec = 0; // Daylight offset in seconds

// Static IP configuration
IPAddress staticIP(192, 168, 0, 30); // ESP32 static IP
IPAddress gateway(192, 168, 0, 1);    // IP Address of your network gateway (router)
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8); // Primary DNS (optional)
IPAddress secondaryDNS(8, 8, 4, 4);   // Secondary DNS (optional)

WiFiClient espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

PubSubClient client(espClient);
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

#define PAYLOAD_BUFFER_SIZE  (50)
char temphum_payload[PAYLOAD_BUFFER_SIZE];

// Function to calculate the average of an array
uint16_t calculateAverage(uint16_t array[], int length) {
  uint32_t sum = 0;
  for (int i = 0; i < length; i++) {
    sum += array[i];
  }
  return sum / length;
}

// Function to convert sensor reading to percentage
float sensorReadingToPercentage(uint16_t reading) {
  if (reading <= sensorMin) return 0.0;
  if (reading >= sensorMax) return 100.0;
  return (float)(reading - sensorMin) / (sensorMax - sensorMin) * 100.0;
}


void setup_wifi() {

  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");

    // Configuring static IP
  if(!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure Static IP");
  } else {
    Serial.println("Static IP configured!");
  }
  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  Wire.begin();
  Wire1.begin(32, 33);  // Replace 21 and 22 with your desired SDA and SCL pins
  while (!Serial); // Wait for the serial connection to be ready
  delay(5000);
  Serial.println("bears farm!");

  // initialize the LED pin as an output
  pinMode(pump36, OUTPUT);
  digitalWrite(pump36, HIGH);

  pinMode(pump37, OUTPUT);
  digitalWrite(pump37, HIGH);

  pinMode(pump38, OUTPUT);
  digitalWrite(pump38, HIGH);

  pinMode(pump39, OUTPUT);
  digitalWrite(pump39, HIGH);

  pinMode(pump36_2, OUTPUT);
  digitalWrite(pump36_2, HIGH);

  pinMode(pump37_2, OUTPUT);
  digitalWrite(pump37_2, HIGH);


  if (!ss1.begin(0x36)) {
    Serial.println("ERROR! Seesaw 1 not found");
    pump36_enable = 0;
  } else {
    Serial.print("Seesaw 1 started! Version: ");
    Serial.println(ss1.getVersion(), HEX);
  }

  if (!ss2.begin(0x37)) {
    Serial.println("ERROR! Seesaw 2 not found");
    pump37_enable = 0;
  } else {
    Serial.print("Seesaw 2 started! Version: ");
    Serial.println(ss2.getVersion(), HEX);
  }

  if (!ss3.begin(0x38)) {
    Serial.println("ERROR! Seesaw 3 not found");
    pump38_enable = 0;
  } else {
    Serial.print("Seesaw 3 started! Version: ");
    Serial.println(ss3.getVersion(), HEX);
  }

  if (!ss4.begin(0x39)) {
    Serial.println("ERROR! Seesaw 4 not found");
    pump39_enable = 0;
  } else {
    Serial.print("Seesaw 4 started! Version: ");
    Serial.println(ss4.getVersion(), HEX);
  }

  if (!ss21.begin(0x36)) {
    Serial.println("ERROR! Seesaw 1 bus 2 not found");
    pump36_2_enable = 0;
  } else {
    Serial.print("Seesaw 1 bus 2 started! Version: ");
    Serial.println(ss21.getVersion(), HEX);
  }

  if (!ss22.begin(0x37)) {
    Serial.println("ERROR! Seesaw 2 bus 2 not found");
    pump37_2_enable = 0;
  } else {
    Serial.print("Seesaw 2 bus 2 started! Version: ");
    Serial.println(ss22.getVersion(), HEX);
  }

  if (!sht31.begin(0x44)) {
    Serial.println("ERROR! SHT31 not found");
  } else {
    Serial.print("SHT31 bus 2 started!");
  }

  // Connect to WiFi
  //connectWiFi();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(gmtOffset_sec);

  while(!timeClient.update()) {
  timeClient.forceUpdate();
  }

  pump36_previousMils = millis();
  pump37_previousMils = millis();
  pump38_previousMils = millis();
  pump39_previousMils = millis();
  pump36_2_previousMils = millis();
  pump37_2_previousMils = millis();



}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void printTimeDiff(unsigned long milliseconds, const char* pumpname) {
  unsigned long totalMinutes = milliseconds / 60000; // 60,000 milliseconds in a minute
  unsigned int hours = totalMinutes / 60;
  unsigned int minutes = totalMinutes % 60;

  char time_elapsed[10]; // Buffer to store the formatted string

  if (hours > 0) {
    //Serial.print(hours);
    //Serial.print(" hours and ");
  }
  //Serial.print(minutes);
  //Serial.println(" minutes");
  sprintf(time_elapsed, "%d:%02d", hours, minutes); // Format with hours and minutes
  //sprintf(time_elapsed, "0:%02d", minutes); // Format with minutes only

  //msg variable contains JSON string to send to MQTT server
  snprintf(msg, MSG_BUFFER_SIZE, "{\"time\": \"%s\"}", time_elapsed);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish(pumpname, msg);
}

void reconnect() {
  int maxRetries = 5;
  int retryCount = 0;

  // Loop until we're reconnected or maximum retries reached
  while (!client.connected() && retryCount < maxRetries) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), "mosquito", "mosquito")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      retryCount++;
    }
  }
}

void loop() {
  // Read from each Seesaw device
  uint16_t capread36 = ss1.touchRead(0);
  uint16_t capread37 = ss2.touchRead(0);
  uint16_t capread38 = ss3.touchRead(0);
  uint16_t capread39 = ss4.touchRead(0);
  uint16_t capread36_2 = ss21.touchRead(0);
  uint16_t capread37_2 = ss22.touchRead(0);

  // Fetch and parse JSON
  if (fetchAndParseJSON()) {
    Serial.println("JSON parsed successfully");
    // Print the variables to verify
    Serial.println(sensor36_thres);
    Serial.println(sensor37_thres);
    // ... print other variables similarly
  } else {
    Serial.println("Failed to parse JSON");
  }

  Serial.println(pump36_enable);

  // Update NTP time
  timeClient.update();

  String currentTime = timeClient.getFormattedTime();
  Serial.println(currentTime);

  //Serial.println(currentTime);
  // Extract hour, minute, and second components
  int currentHour = currentTime.substring(0, 2).toInt();
  int currentMinute = currentTime.substring(3, 5).toInt();
  int currentSecond = currentTime.substring(6, 8).toInt();
  //Serial.println(currentHour);

  // Adjust for local time zone difference
  currentHour = (currentHour + 18) % 24;
  //Serial.println(currentHour);

  daytime = (currentHour >= 6 && currentHour < 18) ? 1 : 0; // Daytime if between 6 AM and 6 PM
  
  mils = millis();

  // Shift values in the history arrays to make room for the new readings
  for (int i = 2; i > 0; i--) {
    capread36_history[i] = capread36_history[i - 1];
    capread37_history[i] = capread37_history[i - 1];
    capread38_history[i] = capread38_history[i - 1];
    capread39_history[i] = capread39_history[i - 1];
    capread36_2_history[i] = capread36_2_history[i - 1];
    capread37_2_history[i] = capread37_2_history[i - 1];
  }
  // Store the new readings in the first position of the history arrays
  capread36_history[0] = capread36;
  capread37_history[0] = capread37;
  capread38_history[0] = capread38;
  capread39_history[0] = capread39;
  capread36_2_history[0] = capread36_2;
  capread37_2_history[0] = capread37_2;

  // Calculate the average of the last three readings for each sensor
  uint16_t average_capread36 = calculateAverage(capread36_history, 3);
  uint16_t average_capread37 = calculateAverage(capread37_history, 3);
  uint16_t average_capread38 = calculateAverage(capread38_history, 3);
  uint16_t average_capread39 = calculateAverage(capread39_history, 3);
  uint16_t average_capread36_2 = calculateAverage(capread36_2_history, 3);
  uint16_t average_capread37_2 = calculateAverage(capread37_2_history, 3);

  if (!client.connected()) {
    reconnect();
  }

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  
  if (! isnan(t)) {  // check if 'is not a number'
  Serial.print("Temp *C = "); Serial.println(t);
  }
  else {    
  t=0.0;
  Serial.println("Failed to read temperature");
  }
  
  if (! isnan(h)) {  // check if 'is not a number'
  Serial.print("Hum. % = "); Serial.println(h);
  }
  else { 
  h=0.0;
  Serial.println("Failed to read humidity");    
  }

  // Format the message with temperature and humidity values
  snprintf(temphum_payload, PAYLOAD_BUFFER_SIZE, "{\"temperature\": %5.2f, \"humidity\": %5.2f}", t, h);

  // Print the message to the serial monitor
  Serial.print("Publish message: ");
  Serial.println(temphum_payload);

  // Publish the message to the MQTT topic
  client.publish("SensorData", temphum_payload);

  // Check if average readings exceed thresholds
  if (average_capread36 > 2000) {
    pump36_enable = 0;
    float percentage36 = 0;
  } else {
    float percentage36 = sensorReadingToPercentage(average_capread36);
    Serial.print("Average of last three readings for Sensor 0x36: ");
    Serial.println(average_capread36);
    Serial.print(" -> ");
    Serial.print(percentage36);
    Serial.println("%");
    //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage36);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture36", msg);
    
  } 
  
  if (average_capread37 > 2000) {
    pump37_enable = 0;
    float percentage37 = 0;
  } else {
    float percentage37 = sensorReadingToPercentage(average_capread37);
    Serial.print("Average of last three readings for Sensor 0x37: ");
    Serial.println(average_capread37);
    Serial.print(" -> ");
    Serial.print(percentage37);
    Serial.println("%");
    //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage37);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture37", msg);
  }

  if (average_capread38 > 2000) {
    pump38_enable = 0;
    float percentage38 = 0;
  } else {
    float percentage38 = sensorReadingToPercentage(average_capread38);
    Serial.print("Average of last three readings for Sensor 0x38: ");
    Serial.println(average_capread38);
    Serial.print(" -> ");
    Serial.print(percentage38);
    Serial.println("%");
    //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage38);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture38", msg);
  }

  if (average_capread39 > 2000) {
    pump39_enable = 0;
    float percentage39 = 0;
  } else {
    float percentage39 = sensorReadingToPercentage(average_capread39);
    Serial.print("Average of last three readings for Sensor 0x39: ");
    Serial.println(average_capread39);
    Serial.print(" -> ");
    Serial.print(percentage39);
    Serial.println("%");
     //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage39);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture39", msg);
  }

  if (average_capread36_2 > 2000) {
    pump36_2_enable = 0;
    float percentage36_2 = 0;
  } else {
    float percentage36_2 = sensorReadingToPercentage(average_capread36_2);
    Serial.print("Average of last three readings for Sensor 0x36 bus 2: ");
    Serial.println(average_capread36_2);
    Serial.print(" -> ");
    Serial.print(percentage36_2);
    Serial.println("%");
    //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage36_2);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture36_2", msg);
  }

  if (average_capread37_2 > 2000) {
    pump37_2_enable = 0;
    float percentage37_2 = 0;
  } else {
    float percentage37_2 = sensorReadingToPercentage(average_capread37_2);
    Serial.print("Average of last three readings for Sensor 0x37 bus 2: ");
    Serial.println(average_capread37_2);
    Serial.print(" -> ");
    Serial.print(percentage37_2);
    Serial.println("%");
    //msg variable contains JSON string to send to MQTT server
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"moisture\": %5.1f\}", percentage37_2);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("SoilMoisture37_2", msg);
  }


  timeDiff = mils - pump36_previousMils;
  // Call the function to print the time difference
  //Serial.println("pump36 time since last watering");
  printTimeDiff(timeDiff, "Pump36LastActive");

  if (average_capread36 < sensor36_thres) {
    Serial.println("Threshold exceeded for Sensor 0x36");
  
    if ((overflow == 0) && (pump36_enable == 1) && (daytime == 1) && (timeDiff > (pump36_waittime*3600000))) {
        
      Serial.print("pump36 triggered");
      digitalWrite(pump36, LOW); //turn on relay
      delay((pump36_ontime/2));
      digitalWrite(pump36, HIGH);
      delay(10000);
      digitalWrite(pump36, LOW); //turn on relay
      delay((pump36_ontime/2));
      digitalWrite(pump36, HIGH);

      pump36_previousMils = millis();
    }
  }


  timeDiff = mils - pump37_previousMils;
  // Call the function to print the time difference
  //Serial.println("pump37 time since last watering");
  printTimeDiff(timeDiff, "Pump37LastActive");

  if (average_capread37 < sensor37_thres) {
    Serial.println("Threshold exceeded for Sensor 0x37");
  
    if ((overflow == 0) && (pump37_enable == 1) && (daytime == 1) && (timeDiff > (pump37_waittime*3600000))) {
        
      Serial.print("pump37 triggered");
      digitalWrite(pump37, LOW); //turn on relay
      delay((pump37_ontime/2));
      digitalWrite(pump37, HIGH);
      delay(10000);
      digitalWrite(pump37, LOW); //turn on relay
      delay((pump37_ontime/2));
      digitalWrite(pump37, HIGH);

      pump37_previousMils = millis();
    }
  }


  timeDiff = mils - pump38_previousMils;
  // Call the function to print the time difference
  //Serial.println("pump38 time since last watering");
  printTimeDiff(timeDiff, "Pump38LastActive");

  if (average_capread38 < sensor38_thres) {
    Serial.println("Threshold exceeded for Sensor 0x38");
  
    if ((overflow == 0) && (pump38_enable == 1) && (daytime == 1) && (timeDiff > (pump38_waittime*3600000))) {
        
      Serial.print("pump38 triggered");
      digitalWrite(pump38, LOW); //turn on relay
      delay((pump38_ontime/2));
      digitalWrite(pump38, HIGH);
      delay(10000);
      digitalWrite(pump38, LOW); //turn on relay
      delay((pump38_ontime/2));
      digitalWrite(pump38, HIGH);

      pump38_previousMils = millis();
    }
  }


  timeDiff = mils - pump39_previousMils;
  // Call the function to print the time difference
  //Serial.println("pump39 time since last watering");
  printTimeDiff(timeDiff, "Pump39LastActive");

  if (average_capread39 < sensor39_thres) {
    Serial.println("Threshold exceeded for Sensor 0x39");
  
    if ((overflow == 0) && (pump39_enable == 1) && (daytime == 1) && (timeDiff > (pump39_waittime*3600000))) {
        
      Serial.print("pump39 triggered");
      digitalWrite(pump39, LOW); //turn on relay
      delay((pump39_ontime/2));
      digitalWrite(pump39, HIGH);
      delay(10000);
      digitalWrite(pump39, LOW); //turn on relay
      delay((pump39_ontime/2));
      digitalWrite(pump39, HIGH);

      pump39_previousMils = millis();
    }
  }


  timeDiff = mils - pump36_2_previousMils;
  // Call the function to print the time difference
  //Serial.println("pump36_2 time since last watering");
  printTimeDiff(timeDiff, "Pump36_2LastActive");

  if (average_capread36_2 < sensor36_2_thres) {
    Serial.println("Threshold exceeded for Sensor 0x36_2");
  
    if ((overflow == 0) && (pump36_2_enable == 1) && (daytime == 1) && (timeDiff > (pump36_2_waittime*3600000))) {
        
      Serial.print("pump36_2 triggered");
      digitalWrite(pump36_2, LOW); //turn on relay
      delay((pump36_2_ontime/2));
      digitalWrite(pump36_2, HIGH);
      delay(10000);
      digitalWrite(pump36_2, LOW); //turn on relay
      delay((pump36_2_ontime/2));
      digitalWrite(pump36_2, HIGH);

      pump36_2_previousMils = millis();
    }
  }


  timeDiff = mils - pump37_2_previousMils;
  // Call the function to print the time difference
 // Serial.println("pump37_2 time since last watering");
  printTimeDiff(timeDiff, "Pump37_2LastActive");

  if (average_capread37_2 < sensor37_2_thres) {
    Serial.println("Threshold exceeded for Sensor 0x37_2");
  
    if ((overflow == 0) && (pump37_2_enable == 1) && (daytime == 1) && (timeDiff > (pump37_2_waittime*3600000))) {
        
      Serial.print("pump37_2 triggered");
      digitalWrite(pump37_2, LOW); //turn on relay
      delay((pump37_2_ontime/2));
      digitalWrite(pump37_2, HIGH);
      delay(10000);
      digitalWrite(pump37_2, LOW); //turn on relay
      delay((pump37_2_ontime/2));
      digitalWrite(pump37_2, HIGH);

      pump37_2_previousMils = millis();
    }
  }

  delay(600000);
}


bool fetchAndParseJSON() {
  // Use WiFiClientSecure for HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // This is not recommended for production use

  HTTPClient http;
  http.begin(client, json_url);
  http.setTimeout(5000); // Increase timeout to 5000ms

  int httpCode = http.GET();
 
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println(payload);

    // Use ArduinoJson to parse the JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    // Set the variables from the parsed JSON
    sensor36_thres = doc["thresholds"]["sensor36_thres"];
    sensor37_thres = doc["thresholds"]["sensor37_thres"];
    sensor38_thres = doc["thresholds"]["sensor38_thres"];
    sensor39_thres = doc["thresholds"]["sensor39_thres"];
    sensor36_2_thres = doc["thresholds"]["sensor36_2_thres"];
    sensor37_2_thres = doc["thresholds"]["sensor37_2_thres"];

    pump36_ontime = doc["ontime"]["pump36_ontime"];
    pump37_ontime = doc["ontime"]["pump37_ontime"];
    pump38_ontime = doc["ontime"]["pump38_ontime"];
    pump39_ontime = doc["ontime"]["pump39_ontime"];
    pump36_2_ontime = doc["ontime"]["pump36_2_ontime"];
    pump37_2_ontime = doc["ontime"]["pump37_2_ontime"];

    pump36_enable = doc["enabled"]["pump36_enable"];
    pump37_enable = doc["enabled"]["pump37_enable"];
    pump38_enable = doc["enabled"]["pump38_enable"];
    pump39_enable = doc["enabled"]["pump39_enable"];
    pump36_2_enable = doc["enabled"]["pump36_2_enable"];
    pump37_2_enable = doc["enabled"]["pump37_2_enable"];

    pump36_waittime = doc["waittime"]["pump36_waittime"];
    pump37_waittime = doc["waittime"]["pump37_waittime"];
    pump38_waittime = doc["waittime"]["pump38_waittime"];
    pump39_waittime = doc["waittime"]["pump39_waittime"];
    pump36_2_waittime = doc["waittime"]["pump36_2_waittime"];
    pump37_2_waittime = doc["waittime"]["pump37_2_waittime"];

    http.end();
    return true;
  } else {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    if (httpCode == -1) {
      Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return false;
  }
}
