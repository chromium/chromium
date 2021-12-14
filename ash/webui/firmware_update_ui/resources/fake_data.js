// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdate, InstallationProgress, UpdatePriority} from './firmware_update_types.js';
import {stringToMojoString16} from './mojo_utils.js';

/** @type {!Array<!Array<!FirmwareUpdate>>} */
export const fakeFirmwareUpdates = [[
  {
    deviceId: '1',
    deviceName: stringToMojoString16('HP dock'),
    deviceVersion: '5.4.3',
    deviceDescription: stringToMojoString16(
        `Update the firmware to the latest to enhance the security of your HP
         dock device`),
    priority: UpdatePriority.kCritical,
  },
  {
    deviceId: '2',
    deviceName: stringToMojoString16('ColorHugALS'),
    deviceVersion: '3.0.2',
    deviceDescription: stringToMojoString16(
        `Updating your ColorHugALS device firmware improves performance and
         adds new features`),
    priority: UpdatePriority.kMedium,
  },
  {
    deviceId: '3',
    deviceName: stringToMojoString16('Logitech keyboard'),
    deviceVersion: '2.1.12',
    deviceDescription: stringToMojoString16(
        'Update firmware for Logitech keyboard to improve performance'),
    priority: UpdatePriority.kLow,
  },
]];

/** @type {!Array<!InstallationProgress>} */
export const fakeInstallationProgress = [
  {
    status: '',
    percentage: 33,
  },
  {
    status: '',
    percentage: 66,
  },
  {
    status: '',
    percentage: 100,
  },
];

/** @type {!FirmwareUpdate} */
export const fakeFirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kLow,
};

/** @type {!FirmwareUpdate} */
export const fakeCriticalFirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kCritical,
};
