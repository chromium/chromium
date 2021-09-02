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
const questionWifiSsid =
    '[WiFi] What is the WiFi SSID you were trying to connect to?';

/**
 * @type {string}
 */
const questionWifiPortalPage = '[WiFi] Do you expect to see a portal page ' +
    'before you get network access like a hotel?';

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
const questionCellularCarrier =
    '[Cellular] Which network/carrier are you trying to connect to?';

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
 * @type {string}
 */
const questionCellularToggle =
    '[Cellular] Does disabling/enabling mobile make the issue go away?';

/**
 * @type {string}
 */
const questionCellularOtherDevices =
    '[Cellular] If using the same SIM card, do other devices ' +
    '(non-Chrome OS devices, other Chromebooks) ' +
    'work with the same cellular network? Please specify the type of device.';


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
    questionGeneralTimestamp,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionWifiSsid,
    questionWifiPortalPage,
    questionWifiOtherDevices,
  ],
  'cellular': [
    questionGeneralTimestamp,
    questionGeneralRegression,
    questionGeneralReproducibility,
    questionCellularSim,
    questionCellularRoaming,
    questionCellularAPN,
    questionCellularToggle,
    questionCellularOtherDevices,
  ],
};
