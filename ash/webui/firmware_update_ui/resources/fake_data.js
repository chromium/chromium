// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdate, InstallationProgress, UpdatePriority, UpdateState} from './firmware_update_types.js';
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
    filepath: {'path': '1.cab'},
    checksum:
        '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f',
  },
  {
    deviceId: '2',
    deviceName: stringToMojoString16('ColorHugALS'),
    deviceVersion: '3.0.2',
    deviceDescription: stringToMojoString16(
        `Updating your ColorHugALS device firmware improves performance and
         adds new features`),
    priority: UpdatePriority.kMedium,
    filepath: {'path': '2.cab'},
    checksum:
        '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a9231',
  },
  {
    deviceId: '3',
    deviceName: stringToMojoString16('Logitech keyboard'),
    deviceVersion: '2.1.12',
    deviceDescription: stringToMojoString16(
        'Update firmware for Logitech keyboard to improve performance'),
    priority: UpdatePriority.kLow,
    filepath: {'path': '3.cab'},
    checksum:
        '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a9232',
  },
]];

/** @type {!Array<!InstallationProgress>} */
export const fakeInstallationProgress = [
  {percentage: 33, state: UpdateState.kUpdating},
  {percentage: 66, state: UpdateState.kUpdating},
  {percentage: 100, state: UpdateState.kSuccess},
];

/** @type {!Array<!InstallationProgress>} */
export const fakeInstallationProgressFailure = [
  {percentage: 33, state: UpdateState.kUpdating},
  {percentage: 66, state: UpdateState.kUpdating},
  {percentage: 100, state: UpdateState.kRestarting},
  {percentage: 100, state: UpdateState.kFailed},
];

/** @type {!FirmwareUpdate} */
export const fakeFirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kLow,
  filepath: {'path': '1.cab'},
  checksum: '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f',
};

/** @type {!FirmwareUpdate} */
export const fakeCriticalFirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kCritical,
  filepath: {'path': '2.cab'},
  checksum: '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f',
};
