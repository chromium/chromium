// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {string}
 */
export const questionnaireBegin = 'To help us diagnose and fix the issue, ' +
    'please answer the following questions:';

/**
 * @type {string}
 */
const questionGeneralTimestamp =
    '[General] What is the timestamp of the issue? ' +
    '(e.g. "2:45 pm" or "2 minutes ago")';

/**
 * @type {string}
 */
const questionGeneralRegression =
    '[General] Do you know if the issue is a regression? ' +
    'If so, in which Chrome OS version did the issue start appearing?';

/**
 * @type {string}
 */
const questionGeneralReproducibility =
    '[General] What is the reproducibility of the problem? ' +
    '(e.g. always happens at a particular activity? randomly once a day? ' +
    'Please describe repro steps if there is any)';

/**
 * @type {string}
 */
const questionBluetoothPeripheral =
    '[Bluetooth] What is the brand/model of the Bluetooth peripheral ' +
    'you had issue with?';

/**
 * @type {string}
 */
const questionBluetoothOtherDevices =
    '[Bluetooth] Do other devices (non-Chrome OS devices, other Chromebooks) ' +
    'work with no issue with the same Bluetooth peripheral?';

/**
 * @type {string}
 */
const questionWifiTypeOfIssue = '[WiFi] What is the type of the issue? ' +
    '(failure to connect WiFi, Internet connectivity, ' +
    'intermittent disconnects, slow WiFi, intermittent lags)';

/**
 * @type {string}
 */
const questionWifiConnectedButNotInternet =
    '[WiFi] Did WiFi connect but the Internet is not working?';

/**
 * @type {string}
 */
const questionWifiNetworkWorkingBefore =
    '[WiFi] Has this device successfully used this network before?';

/**
 * @type {string}
 */
const questionWifiOtherDevices =
    '[WiFi] Do other devices (non-Chrome OS devices, other Chromebooks) ' +
    'work with the same WiFi network? Please specify the type of device.';

/**
 * @type {string}
 */
const questionCellularSim = '[Cellular] Which carrier SIM do you use?';

/**
 * @type {string}
 */
const questionCellularLastSuccess =
    '[Cellular] When was the last time you connected to the network ' +
    'successfully with this SIM card?';

/**
 * @type {string}
 */
const questionCellularRoaming = '[Cellular] Are you roaming internationally?';

/**
 * @type {string}
 */
const questionCellularAPN = '[Cellular] Did you configure the APN manually? ' +
    'If yes, which configuration did you use?';


/**
 * @type {Object<string, Array<string>>}
 */
export const domainQuestions = {
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
