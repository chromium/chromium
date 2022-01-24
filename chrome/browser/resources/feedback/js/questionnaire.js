// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {string}
 */
const questionnaireBegin = '(Google Internal) To help us diagnose and ' +
    'fix the issue, please answer the following questions:';

/**
 * @type {string}
 */
const questionnaireNotification = 'Some questions have been added' +
    ' to the description box, please answer them before submitting.';

/**
 * @type {string}
 */
const questionGeneralTimestamp =
    '[General] When did this happen? Please mention the exact time ' +
    '(for example: 2:45pm). ';

/**
 * @type {string}
 */
const questionGeneralRegression =
    '[General] Do you know if this issue is a regression? ' +
    'If so, in which Chrome OS version did this issue start appearing? ';

/**
 * @type {string}
 */
const questionGeneralReproducibility =
    '[General] Does this always happen during a particular activity? ' +
    'Does it happen randomly? If so, how often? ' +
    'Please describe the steps to reproduce this problem. ';

/**
 * @type {string}
 */
const questionBluetoothPeripheral =
    '[Bluetooth] What is the brand and model of your Bluetooth peripheral ' +
    '(such as headset or mouse) you had an issue with? ';

/**
 * @type {string}
 */
const questionBluetoothOtherDevices = '[Bluetooth] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'work well with this Bluetooth peripheral (such as headset or mouse)? ';

/**
 * @type {string}
 */
const questionWifiTypeOfIssue = '[WiFi] What type of issue is this? ' +
    'Please select one or more from the below: \n' +
    '   * Failure to connect to Wi-Fi \n' +
    '   * Internet connectivity \n' +
    '   * Intermittently disconnects \n' +
    '   * Constantly slow Wi-Fi \n' +
    '   * Intermittently slow Wi-Fi';

/**
 * @type {string}
 */
const questionWifiConnectedButNotInternet =
    '[WiFi] Does your computer show that Wi-Fi is connected? ' +
    'If so, is your internet still not working? ';

/**
 * @type {string}
 */
const questionWifiNetworkWorkingBefore =
    '[WiFi] Has this device been able to connect ' +
    'to this Wi-Fi network before? ';

/**
 * @type {string}
 */
const questionWifiOtherDevices = '[WiFi] Do other computer devices ' +
    '(such as non-Chrome OS devices or other Chromebooks) ' +
    'have the same issue using the same Wi-Fi network? ' +
    'If so, please specify the device type. ';

/**
 * @type {string}
 */
const questionCellularSim = '[Cellular] Who is your SIM card carrier? ' +
    'For example: Verizon, T-Mobile, AT&T. ';

/**
 * @type {string}
 */
const questionCellularLastSuccess =
    '[Cellular] Have you been able to use this SIM card previously ' +
    'on this Chrome OS device? ';

/**
 * @type {string}
 */
const questionCellularRoaming =
    '[Cellular] Are you currently roaming internationally?';

/**
 * @type {string}
 */
const questionCellularAPN =
    '[Cellular] Did you manually configure the Access Point Name (APN)?';


/**
 * @type {Object<string, Array<string>>}
 */
const domainQuestions = {
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
