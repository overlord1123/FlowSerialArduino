
#include "Arduino.h"
#include "flowSerialArduino.h"

//#define _DEBUG_FLOWSERIALARDUINO_

FlowSerial::FlowSerial(long baudrate, uint8_t* iflowReg, size_t regSize):
	flowReg(iflowReg),
	sizeOfFlowReg(regSize)
{
	Serial.begin(baudrate);
	Serial.flush(); 
}

char FlowSerial::update(){
	while(Serial.available() != 0){
		uint8_t byteIn = Serial.read();
		#ifdef _DEBUG_FLOWSERIALARDUINO_
		Serial.print("Received ");
		Serial.println(byteIn);
		#endif
		switch(process){
			case idle:
				if(byteIn == 0xAA){
					checksumInbox = 0xAA;
					timeoutTime = millis();
					#ifdef _DEBUG_FLOWSERIALARDUINO_
					Serial.println("startbyte recieved");
					#endif
					process = startRecieved;
				}
				break;
			case startRecieved:
				instruction = byteIn;
				checksumInbox += byteIn;
				argumentBufferAt = 0;
				process = instructionRecieved;
				#ifdef _DEBUG_FLOWSERIALARDUINO_
				Serial.println("instruction recieved");
				#endif
				break;
			case instructionRecieved:
				checksumInbox += byteIn;
				switch(instruction){
					case readRequest:
						argumentBuffer[argumentBufferAt] = byteIn;
						argumentBufferAt++;
						if(argumentBufferAt >= 2){
							process = argumentsRecieved;
							#ifdef _DEBUG_FLOWSERIALARDUINO_
							Serial.println("arguments recieved");
							#endif
						}
						break;
					case writeInstruction:
						argumentBuffer[argumentBufferAt] = byteIn;
						argumentBufferAt++;
						if(argumentBufferAt >= 2 && argumentBufferAt >= argumentBuffer[1] + 2){
							process = argumentsRecieved;
							#ifdef _DEBUG_FLOWSERIALARDUINO_
							Serial.println("arguments recieved");
							#endif
						}
						break;
					case dataReturn:
						argumentBuffer[argumentBufferAt] = byteIn;
						argumentBufferAt++;
						if(argumentBufferAt >= 1 && argumentBufferAt >= argumentBuffer[0] + 2){
							process = argumentsRecieved;
							#ifdef _DEBUG_FLOWSERIALARDUINO_
							Serial.println("arguments recieved");
							#endif
						}
						break;
				}
				break;
			case argumentsRecieved:
				checksumIn = byteIn << 8;
				process = MSBchecksumRecieved;
				#ifdef _DEBUG_FLOWSERIALARDUINO_
				Serial.print("MSB recieved = ");
				Serial.println(byteIn);
				#endif
				break;
			case MSBchecksumRecieved:
				checksumIn = byteIn & 0xFF;
				#ifdef _DEBUG_FLOWSERIALARDUINO_
				Serial.print("LSB recieved = ");
				Serial.println(byteIn);
				#endif
				if(checksumIn == checksumInbox){
					#ifdef _DEBUG_FLOWSERIALARDUINO_
					Serial.println("checksum ok. exucute instruction");
					#endif
				}
				else{
					process = idle;
					#ifdef _DEBUG_FLOWSERIALARDUINO_
					Serial.print("checksum failed. recieved = ");
					Serial.println(checksumIn);
					Serial.print("counted                   = ");
					Serial.println(checksumInbox);
					#endif
					return -1;
				}
				switch(instruction){
					case readRequest:
						readCommand();
						break;
					case writeInstruction:
						writeCommand();
						break;
					case dataReturn:
						recieveData();
						break;
				}
				process = idle;
				break;
			default:
				process = idle;
		}
	}
	if(millis() - timeoutTime > 150){
		process = idle;
		return -1;
	}
	return 0;
}

void FlowSerial::readCommand(){
	#ifdef _DEBUG_FLOWSERIALARDUINO_
	Serial.println("Executing read command");
	#endif
	Serial.write(0xAA);
	Serial.write(dataReturn);
	Serial.write(argumentBuffer[1]);
	int checksumOut = 0xAA + dataReturn + argumentBuffer[1];
	for(int i = 0;i < argumentBuffer[1]; i++){
		Serial.write(flowReg[i + argumentBuffer[0]]);
		checksumOut += flowReg[i + argumentBuffer[0]];
	}
	Serial.write(char(checksumOut));
	Serial.write(char(checksumOut >> 8));
}

void FlowSerial::writeCommand(){
	#ifdef _DEBUG_FLOWSERIALARDUINO_
	Serial.println("Executing write command");
	#endif
	if(argumentBuffer[1] > sizeOfFlowReg){
		argumentBuffer[1] = sizeOfFlowReg;
		#ifdef _DEBUG_FLOWSERIALARDUINO_
		Serial.println("Tried writing outside of buffer");
		#endif
	}
	#ifdef _DEBUG_FLOWSERIALARDUINO_
	Serial.print("Writing from ");
	Serial.print(argumentBuffer[0]);
	Serial.print(" to ");
	Serial.println(argumentBuffer[1] + argumentBuffer[0]);
	#endif
	for(int i = 0;i < argumentBuffer[1]; i++){
		flowReg[i + argumentBuffer[0]] = argumentBuffer[i + 2];
	}
}

void FlowSerial::recieveData(){
	#ifdef _DEBUG_FLOWSERIALARDUINO_
	Serial.println("Executing recieveData command");
	#endif
	for(int i = 0; i < argumentBuffer[1]; i++){
		inboxBuffer[i + inboxRegisterAt] = argumentBuffer[i + 2];
		inboxAvailable++;
		inboxRegisterAt++;
	}
}

char FlowSerial::read(){
	char charOut = inboxBuffer[inboxRegisterAt - inboxAvailable];
	inboxAvailable--;
	return charOut;
}

int FlowSerial::available(){
	return inboxAvailable;
}

void FlowSerial::write(uint8_t address, uint8_t out[], int quantity){
	Serial.write(0xAA);
	Serial.write(writeInstruction);
	Serial.write(address);
	Serial.write(quantity);
	int serialSum = 0xAA + writeInstruction + address + quantity;
	for(int i = 0; i < quantity; i++){
		Serial.write(out[i]);
		serialSum += out[i];
	}
	Serial.write(char(serialSum));
	Serial.write(char(serialSum >> 8));
}

void FlowSerial::write(uint8_t address, uint8_t out){
	write(address, &out, 1);
}
