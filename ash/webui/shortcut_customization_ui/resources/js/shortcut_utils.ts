// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {Accelerator, AcceleratorInfo, AcceleratorState, AcceleratorType} from './shortcut_types.js';

// Returns true if shortcut customization is disabled via the feature flag.
export const isCustomizationDisabled = (): boolean => {
  return !loadTimeData.getBoolean('isCustomizationEnabled');
};

export const areAcceleratorsEqual =
    (accelA: Accelerator, accelB: Accelerator): boolean => {
      return JSON.stringify(accelA) === JSON.stringify(accelB);
    };

export const createEmptyAccelInfoFromAccel =
    (accel: Accelerator): AcceleratorInfo => {
      return {
        accelerator: accel,
        hasKeyEvent: true,
        keyDisplay: '',
        locked: false,
        state: AcceleratorState.kEnabled,
        type: AcceleratorType.kUser,
      };
    };

export const createEmptyAcceleratorInfo = (): AcceleratorInfo => {
  return createEmptyAccelInfoFromAccel({modifiers: 0, keyCode: 0});
};
