// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const questionnaireBegin = '(Google Internal) To help us diagnose and ' +
    'fix the issue, please answer the following questions:';

const questionGeneralTimestamp =
    '[General] When did this happen? Please mention the exact time ' +
    '(for example: 2:45pm). ';

const questionGeneralCurrentProblem =
    '[General] Are you experiencing this problem now? ';

const questionGeneralRegression =
    '[General] Do you know if this issue is a regression? ' +
    'If so, in which Chrome OS version did this issue start appearing? ';

const questionGeneralReproducibility =
    '[General] Does this always happen during a particular activity? ' +
    'Does it happen randomly? If so, how often? ' +
    'Please describe the steps to reproduce this problem. ';

const questionBluetoothPeripheral =
    '[Bluetooth] What is the brand and model of your Bluetooth peripheral ' +
    '(such as headset or mouse) you had an issue with? ';

const questionBluetoothOtherDevices = '[Bluetooth] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'work well with this Bluetooth peripheral (such as headset or mouse)? ';

const questionWifiTypeOfIssue = '[WiFi] What kind of issue is this? ' +
    'Please select one or more from the below: \n' +
    '   * Failure to connect to Wi-Fi \n' +
    '   * Internet connectivity \n' +
    '   * Intermittently disconnects \n' +
    '   * Constantly slow Wi-Fi \n' +
    '   * Intermittently slow Wi-Fi';

const questionWifiConnectedButNotInternet =
    '[WiFi] Does your computer show that Wi-Fi is connected? ' +
    'If so, is your internet still not working? ';

const questionWifiNetworkWorkingBefore =
    '[WiFi] Has this device been able to connect ' +
    'to this Wi-Fi network before? ';

const questionWifiOtherDevices = '[WiFi] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'have the same issue using the same Wi-Fi network? ' +
    'If so, please specify the kind of device. ';

const questionCellularSim = '[Cellular] Who is your SIM card carrier? ' +
    'For example: Verizon, T-Mobile, AT&T. ';

const questionCellularLastSuccess =
    '[Cellular] Have you been able to use this SIM card previously ' +
    'on this Chrome OS device? ';

const questionCellularRoaming =
    '[Cellular] Are you currently roaming internationally?';

const questionCellularAPN =
    '[Cellular] Did you manually configure the Access Point Name (APN)?';

const questionDisplayConnectionConnected =
    '[Display] How many external displays are connected to your device? ';

const questionDisplayConnectionNumberConnected =
    '[Display] What make/model are any connected external displays? ';

const questionDisplayConnectionCableType =
    '[Display] What cable (DisplayPort/HDMI) are connected displays using? ';

const questionDisplayConnectionDock =
    '[Display] Are your displays connected via dock? If so, what make/model? ';

const questionDisplayProblemScope =
    '[Display] If you have multiple displays connected, is the problem ' +
    'affecting all displays? ';

const questionDisplayProblem =
    '[Display] What kind of issue is this? Please select one from below:\n' +
    '   * Entire screen is consistently black/off \n' +
    '   * Entire screen is intermittently black/off \n' +
    '   * Part of screen is consistently black/off \n' +
    '   * Part of screen is intermittently black/off \n' +
    '   * Picture on screen is corrupted \n';

const questionUSBDevice = '[USB] What is the make/model of the affected USB ' +
    'device?';

const questionUSBCable = '[USB] Tell us about your cable. What kind of plug ' +
    'does it have at each end (USB-A, USB-C, Micro USB, etc.). Does it have ' +
    'any logos or labeling?';

const questionUSBTopology = '[USB] How is the device connected to your ' +
    'Chromebook? Which port is the device connected to? Is it directly ' +
    'connected to your Chromebook, or connected through hubs? If it is ' +
    'connected through hubs, please tell us their make and model.';

const questionThunderboltDevice = '[Thunderbolt] What is the make/model of ' +
    'the affected Thunderbolt device?';

const questionThunderboltCable = '[Thunderbolt] Tell us about your cable. Is ' +
    'it labeled as a Thunderbolt cable? If not, what kind of plug does it ' +
    'have at each end (USB-A, USB-C, Micro USB, etc.). Does it have any ' +
    'other logos or labeling?';

const questionThunderboltDisplays = '[Thunderbolt] If you are having trouble ' +
    'connecting external displays to your Chromebook with a Thunderbolt ' +
    'dock, what are the make and model of the displays? Which ports on the ' +
    'dock are they connected to?';

export const domainQuestions = {
  'bluetooth': [
    questionGeneralTimestamp,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionBluetoothPeripheral,
    questionBluetoothOtherDevices,
  ],
  'wifi': [
    questionGeneralTimestamp,
    questionWifiTypeOfIssue,
    questionWifiConnectedButNotInternet,
    questionWifiNetworkWorkingBefore,
    questionWifiOtherDevices,
  ],
  'cellular': [
    questionCellularSim,
    questionCellularLastSuccess,
    questionCellularRoaming,
    questionCellularAPN,
  ],
  'display': [
    questionGeneralTimestamp,
    questionGeneralCurrentProblem,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionDisplayConnectionConnected,
    questionDisplayConnectionNumberConnected,
    questionDisplayConnectionCableType,
    questionDisplayConnectionDock,
    questionDisplayProblemScope,
    questionDisplayProblem,
  ],
  'usb': [
    questionGeneralTimestamp,
    questionGeneralCurrentProblem,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionUSBDevice,
    questionUSBCable,
    questionUSBTopology,
  ],
  'thunderbolt': [
    questionGeneralTimestamp,
    questionGeneralCurrentProblem,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionThunderboltDevice,
    questionThunderboltCable,
    questionThunderboltDisplays,
  ],
};
