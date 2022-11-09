// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {Accelerator, AcceleratorId, AcceleratorInfo, AcceleratorState, AcceleratorType} from './shortcut_types.js';

// Returns true if shortcut customization is disabled via the feature flag.
export const isCustomizationDisabled = (): boolean => {
  return !loadTimeData.getBoolean('isCustomizationEnabled');
};

export const areAcceleratorsEqual =
    (accelA: Accelerator, accelB: Accelerator): boolean => {
      // This picking of types is necessary because Accelerators are a subset
      // of MojoAccelerators, and MojoAccelerators have properties that error
      // when they're stringified. Due to TypeScript's structural typing, we
      // can't prevent MojoAccelerators from being passed to this function.
      const accelAComparable:
          Accelerator = {keyCode: accelA.keyCode, modifiers: accelA.modifiers};
      const accelBComparable:
          Accelerator = {keyCode: accelB.keyCode, modifiers: accelB.modifiers};
      return JSON.stringify(accelAComparable) ===
          JSON.stringify(accelBComparable);
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

export const getAcceleratorId =
    (source: string|number, actionId: string|number): AcceleratorId => {
      return `${source}-${actionId}`;
    };