// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const questionnaireBegin: string =
    '(Google Internal) To help us diagnose and ' +
    'fix the issue, please answer the following questions:';

export const questionnaireNotification: string =
    'Some questions have been added' +
    ' to the description box, please answer them before submitting.';

const questionGeneralTimestamp: string =
    '[General] When did this happen? Please mention the exact time ' +
    '(for example: 2:45pm). ';

const questionGeneralRegression: string =
    '[General] Do you know if this issue is a regression? ' +
    'If so, in which Chrome OS version did this issue start appearing? ';

const questionGeneralReproducibility: string =
    '[General] Does this always happen during a particular activity? ' +
    'Does it happen randomly? If so, how often? ' +
    'Please describe the steps to reproduce this problem. ';

const questionBluetoothPeripheral: string =
    '[Bluetooth] What is the brand and model of your Bluetooth peripheral ' +
    '(such as headset or mouse) you had an issue with? ';

const questionBluetoothOtherDevices: string =
    '[Bluetooth] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'work well with this Bluetooth peripheral (such as headset or mouse)? ';

const questionWifiTypeOfIssue: string = '[WiFi] What kind of issue is this? ' +
    'Please select one or more from the below: \n' +
    '   * Failure to connect to Wi-Fi \n' +
    '   * Internet connectivity \n' +
    '   * Intermittently disconnects \n' +
    '   * Constantly slow Wi-Fi \n' +
    '   * Intermittently slow Wi-Fi';

const questionWifiConnectedButNotInternet: string =
    '[WiFi] Does your computer show that Wi-Fi is connected? ' +
    'If so, is your internet still not working? ';

const questionWifiNetworkWorkingBefore: string =
    '[WiFi] Has this device been able to connect ' +
    'to this Wi-Fi network before? ';

const questionWifiOtherDevices: string = '[WiFi] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'have the same issue using the same Wi-Fi network? ' +
    'If so, please specify the kind of device. ';

const questionCellularSim: string =
    '[Cellular] Who is your SIM card carrier? ' +
    'For example: Verizon, T-Mobile, AT&T. ';

const questionCellularLastSuccess: string =
    '[Cellular] Have you been able to use this SIM card previously ' +
    'on this Chrome OS device? ';

const questionCellularRoaming: string =
    '[Cellular] Are you currently roaming internationally?';

const questionCellularAPN: string =
    '[Cellular] Did you manually configure the Access Point Name (APN)?';

export const domainQuestions: {
  bluetooth: string[],
  wifi: string[],
  cellular: string[],
} = {
  'bluetooth': [
    questionGeneralTimestamp,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionBluetoothPeripheral,
    questionBluetoothOtherDevices,
  ],
  'wifi': [
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
};
