// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';

import {FirmwareUpdate, InstallationProgress, UpdatePriority, UpdateState} from './firmware_update.mojom-webui.js';

export const fakeFirmwareUpdates: FirmwareUpdate[][] = [[
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

export const fakeInstallationProgress: InstallationProgress[] = [
  {percentage: 33, state: UpdateState.kUpdating},
  {percentage: 66, state: UpdateState.kUpdating},
  {percentage: 100, state: UpdateState.kSuccess},
];

export const fakeInstallationProgressFailure: InstallationProgress[] = [
  {percentage: 33, state: UpdateState.kUpdating},
  {percentage: 66, state: UpdateState.kUpdating},
  {percentage: 100, state: UpdateState.kRestarting},
  {percentage: 100, state: UpdateState.kFailed},
];

export const fakeFirmwareUpdate: FirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kLow,
  filepath: {'path': '1.cab'},
  checksum: '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f',
};

export const fakeCriticalFirmwareUpdate: FirmwareUpdate = {
  deviceId: '1',
  deviceName: stringToMojoString16('Logitech keyboard'),
  deviceVersion: '2.1.12',
  deviceDescription: stringToMojoString16(
      'Update firmware for Logitech keyboard to improve performance'),
  priority: UpdatePriority.kCritical,
  filepath: {'path': '2.cab'},
  checksum: '3fab34cfa1ef97238fb24c5e40a979bc544bb2b0967b863e43e7d58e0d9a923f',
};
