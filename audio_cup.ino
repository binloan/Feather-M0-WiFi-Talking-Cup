#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <AudioZero.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <WiFi101.h>
#include <FlashStorage.h>

// Chip select for the uSD card reader
#define CS 19
// Pin for the touch button
#define PLAY_BUTTON 15
#define AMP_SHUTDOWN 16
// The BNO055 object for reading absolute orientation data
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
// Make this volatile as it's accessed through the ISR
volatile int tValue = 0;
// Root directory for file list
File root;
// Filenames used for the SD card files
// Audio files MUST be 88.2kHz & 8-bit PCM!!
int leftFile = 0;
int rightFile = 0;
#define FILE_LIST_LENGTH 255
String files[FILE_LIST_LENGTH];
int fileCount = 0;
// WiFi Network Details
char ssid[] = "TALKING_CUP";        // your network SSID (name)
// WiFi Server Status
const int serverOnlineTimeMs = 1000 * 30;
bool isWifiActive = true;
// This is a horrible hack...I'm just a lazy pseudo Korean, OK?
bool ignoreInital = true;
int activeConnections = 0;
WiFiServer server(80);
int status = WL_IDLE_STATUS;
// Reserve a portion of flash memory to store an "int" variable
FlashStorage(left_sound, int);
FlashStorage(right_sound, int);

void setup() {
  // Initially Serial port is initialized, only for DEBUG pruposes
  Serial.begin(115200);
  // Activate the INPUT for the touch button in the cup
  Serial.print("Initializing I/O ...");
  pinMode(PLAY_BUTTON, INPUT);
  pinMode(AMP_SHUTDOWN, OUTPUT);
  digitalWrite(AMP_SHUTDOWN, LOW);
  attachInterrupt(digitalPinToInterrupt(PLAY_BUTTON), touchChange, CHANGE);
  Serial.println("DONE!");
  // Start mounting the SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.println("Failed!");
    // If failed get stuck...
    while (1);
  }
  Serial.println("DONE!");
  // Initialize the BNO055 orientation sensor
  Serial.print("Initializing BNO055...");
  if (!bno.begin())
  {
    // If failed get stuck...
    Serial.println("Failed!");
    while (1);
  }
  Serial.println("DONE!");
  // Read all files in SD card root
  Serial.print("Reading files on SD card...");
  root = SD.open("/");
  readDirectory(root, 0);
  root.close();
  // Make a bit more fancy init message
  for(int i = 0; i < FILE_LIST_LENGTH; i++){
    if(files[i] == ""){
      // Skip empty items
      continue;
    }
   fileCount += 1;
  }
  Serial.println(fileCount);
  // If there are no files, we just stop here.
  if(fileCount <= 0){
    Serial.println("ERROR: No files present!");
    while(true);
  }
  leftFile = left_sound.read();
  rightFile = right_sound.read();
  if(files[leftFile] == ""){
    leftFile = 0;
  }
  if(files[rightFile] == ""){
    rightFile = 0;
  }
  // Catch this edge case where we only have one audio file present
  if(fileCount == 1){
    // We use the same for both sides
    leftFile = 0;
    rightFile = 0;
  }
  // OK the Feather we use as the shield soldered, but maybe at some point Wi-Fi fails. Then we know!
  Serial.print("Initializing WiFi...");
  WiFi.setPins(8,7,4,2);
  if (WiFi.status() == WL_NO_SHIELD) {
    // If failed get stuck...
    Serial.println("NO WiFi!");
    while (1);
  }
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("NO AP!");
    // don't continue
    while (1);
  }
  IPAddress ip = WiFi.localIP();
  server.begin();
  Serial.println(ip);
}

void touchChange() {
  // Set the touch ISR value so it can be accessed through the main loop
  tValue = digitalRead(PLAY_BUTTON);
}

void loop() {
  computeSound();
  wifiStatus();
  if(isWifiActive){
    computeWebServer();
  }
}

void wifiStatus(){
  // Check the WiFi AP status
  if (status != WiFi.status()) {
    // Read the new WiFi status
    status = WiFi.status();
    // If client connected...
    if (status == WL_AP_CONNECTED) {
      byte remoteMac[6];
      // ...print the clients MAC
      Serial.print("New Wifi client: ");
      WiFi.APClientMacAddress(remoteMac);
      printMacAddress(remoteMac);
      activeConnections += 1;
    }else{
      activeConnections -= 1;
      if(activeConnections < 0){
        activeConnections = 0;
      }
    }
  }
  // Turn of WiFi after given time to combat background hiss
  if(millis() > serverOnlineTimeMs && activeConnections <= 0){
    Serial.print("Disabling WiFi...");
    server.flush();
    WiFi.end();
    Serial.println("DONE!");
  }
}

void computeWebServer(){
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    // make a String to hold incoming data from the client
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<html><body><h2>&#128483; Talking Cup</h2>");
            client.print("<p>Please select the sound you want to play on either left or right side actions. To select click the respective side on the sound file.</p>");
            client.print("<table style=\"border: 1px solid black;\">");
            client.print("<tr><th style=\"border: 1px solid black;\">&#128072;</th><th style=\"border: 1px solid black;\">&#128073;</th></tr>");
            client.print("<tr><td style=\"border: 1px solid black;\">");
            client.print(files[leftFile]);
            client.print("</td><td style=\"border: 1px solid black;\">");
            client.print(files[rightFile]);
            client.print("</td><tr></table>");
            client.print("<br/>");
            client.print("<table style=\"width:100%; border: 1px solid black;\">");
            for(int i = 0; i < fileCount; i++){
              client.print("<tr><th style=\"text-align:left; border: 1px solid black;\">");
              client.print(files[i]);
              client.print("</th><td style=\"text-align:center; border: 1px solid black;\"><a href=\"/");
              client.print("L_");
              client.print(files[i]);
              client.print("\">&#9194;</a></td><td style=\"text-align:center; border: 1px solid black;\"><a href=\"/");
              client.print("R_");
              client.print(files[i]);
              client.print("\">&#9193;</a></td>");
              client.print("<td style=\"text-align:center; border: 1px solid black;\"><a href=\"/");
              client.print("P_");
              client.print(files[i]);
              client.print("\">&#9654;</a></td>");
            }
            client.print("</table></body></html>");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Read the HTTP response from the Client and scan for an assignment
        // TODO: Only receive one full request not two
        if (currentLine.startsWith("GET /L_") && currentLine.endsWith("HTTP/1.1")) {
          if(!ignoreInital){
            // Get the index of the HTTP request
            int httpIndex = currentLine.indexOf("HTTP/1.1");
            // Create file substring
            String file = currentLine.substring(7, httpIndex - 1);
            // Search for the file in the list and assign it
            for(int i = 0; i < fileCount; i++){
              if(files[i].equalsIgnoreCase(file)){
                ignoreInital = true;
                leftFile = i;
                Serial.print("New assignment for left: ");Serial.println(file);
                left_sound.write(leftFile);
              }
            }
          }else{
            ignoreInital = false;
          }
        }else if(currentLine.startsWith("GET /R_") && currentLine.endsWith("HTTP/1.1")){
          if(!ignoreInital){
            // Get the index of the HTTP request
            int httpIndex = currentLine.indexOf("HTTP/1.1");
            // Create file substring
            String file = currentLine.substring(7, httpIndex - 1);
            // Search for the file in the list and assign it
            for(int i = 0; i < fileCount; i++){
              if(files[i].equalsIgnoreCase(file)){
                ignoreInital = true;
                rightFile = i;
                Serial.print("New assignment for right: ");Serial.println(file);
                right_sound.write(rightFile);
              }
            }
          }else{
            ignoreInital = false;
          }
        }else if(currentLine.startsWith("GET /P_") && currentLine.endsWith("HTTP/1.1")){
          if(!ignoreInital){
            // Get the index of the HTTP request
            int httpIndex = currentLine.indexOf("HTTP/1.1");
            // Create file substring
            String file = currentLine.substring(7, httpIndex - 1);
            // Search for the file in the list and assign it
            for(int i = 0; i < fileCount; i++){
              if(files[i].equalsIgnoreCase(file)){
                ignoreInital = true;
                rightFile = i;
                Serial.print("Previewing file: ");Serial.println(file);
                playSound(files[i]);
              }
            }
          }else{
            ignoreInital = false;
          } 
        }
      }
    }
    // close the connection:
    client.stop();
  }
}

void computeSound(){
  // Check if there's a touch event
  if(tValue == 1){
    tValue = 2;
    // This is the case, let's read the BNO055's orientation
    sensors_event_t orientationData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    // Allocate initial values to spot an issue easily
    double x = orientationData.orientation.x;
    Serial.print("Measured absolute orienation: ");Serial.println(x);
    // Based on the measured angle play a given sound
    if(x <= 180){
      playSound(files[leftFile]);
    }else{
      playSound(files[rightFile]);
    }
  }
}

void playSound(String file){
  // Load the audio file from the SD card
  File audioFile = SD.open(file);
  digitalWrite(AMP_SHUTDOWN, HIGH);
  // Start the audio engine
  AudioZero.begin(2*44100);
  // Check audio file was properly loaded
  if (!audioFile) {
    // If the file didn't open, print an error and stop
    Serial.print("ERROR opening ");Serial.print(file);Serial.println("!");
    return;
  }
  Serial.print("Playing ");Serial.print(file);Serial.print("...");
  // Play sopund file. This action is blocking the main loop!
  AudioZero.play(audioFile);
  // This function is called after playback
  AudioZero.end();
  digitalWrite(AMP_SHUTDOWN, LOW);
  Serial.println("DONE!");
}

void readDirectory(File dir, int numTabs) {
  // The array position for the file name
  int position = 0;
  // Loop through the specified folder
  while (true) {
    // Open the next file of the iterator
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    // Convert the name to String, just to make things easier
    String name = String(entry.name());
    // Make it lowercase
    name.toLowerCase();
    // Then test for it being a .wav file
    if (name.endsWith(".wav")){
      // Then append to the list
      files[position] = entry.name();
      position += 1;
    }
    // Close the file
    entry.close();
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}