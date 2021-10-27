// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdate, UpdatePriority} from './firmware_update_types.js';

/** @type {!Array<!Array<!FirmwareUpdate>>} */
export const fakeFirmwareUpdates = [[
  {
    deviceId: '1',
    deviceName: 'HP dock',
    version: '5.4.3',
    description:
        `Update the firmware to the latest to enhance the security of your HP
         dock device`,
    priority: UpdatePriority.kCritical,
    updateModeInstructions: 'Do a backflip before updating.',
    screenshotUrl: '',
  },
  {
    deviceId: '2',
    deviceName: 'ColorHugALS',
    version: '3.0.2',
    description:
        `Updating your ColorHugALS device firmware improves performance and
         adds new features`,
    priority: UpdatePriority.kMedium,
    updateModeInstructions: '',
    screenshotUrl: '',
  },
  {
    deviceId: '3',
    deviceName: 'Logitech keyboard',
    version: '2.1.12',
    description: 'Update firmware for Logitech keyboard to improve performance',
    priority: UpdatePriority.kLow,
    updateModeInstructions: 'Do a cartwheel before updating.',
    screenshotUrl: '',
  },
]];
