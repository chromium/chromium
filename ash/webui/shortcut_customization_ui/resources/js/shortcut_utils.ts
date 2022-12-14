// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorInfo, AcceleratorState, AcceleratorSubcategory, AcceleratorType, DefaultAcceleratorInfo, MojoAcceleratorInfo, TextAcceleratorInfo} from './shortcut_types.js';

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

export const isTextAcceleratorInfo =
    (accelInfo: AcceleratorInfo|MojoAcceleratorInfo):
        accelInfo is TextAcceleratorInfo => {
          return !!(accelInfo as TextAcceleratorInfo)
                       .layoutProperties.textAccelerator;
        };

export const isDefaultAcceleratorInfo =
    (accelInfo: AcceleratorInfo|MojoAcceleratorInfo):
        accelInfo is DefaultAcceleratorInfo => {
          return !!(accelInfo as DefaultAcceleratorInfo)
                       .layoutProperties.defaultAccelerator;
        };

export const createEmptyAccelInfoFromAccel =
    (accel: Accelerator): DefaultAcceleratorInfo => {
      return {
        layoutProperties:
            {defaultAccelerator: {accelerator: accel, keyDisplay: ''}},
        locked: false,
        state: AcceleratorState.kEnabled,
        type: AcceleratorType.kUser,
      };
    };

export const createEmptyAcceleratorInfo = (): DefaultAcceleratorInfo => {
  return createEmptyAccelInfoFromAccel({modifiers: 0, keyCode: 0});
};

export const getAcceleratorId =
    (source: string|number, actionId: string|number): AcceleratorId => {
      return `${source}-${actionId}`;
    };

const categoryPrefix = 'category';
export const getCategoryNameStringId =
    (category: AcceleratorCategory): string => {
      switch (category) {
        case AcceleratorCategory.kTabsAndWindows:
          return `${categoryPrefix}TabsAndWindows`;
        case AcceleratorCategory.kPageAndWebBrowser:
          return `${categoryPrefix}PageAndWebBrowser`;
        case AcceleratorCategory.kSystemAndDisplaySettings:
          return `${categoryPrefix}SystemAndDisplaySettings`;
        case AcceleratorCategory.kTextEditing:
          return `${categoryPrefix}TextEditing`;
        case AcceleratorCategory.kAccessibility:
          return `${categoryPrefix}Accessibility`;
        case AcceleratorCategory.kDebug:
          return `${categoryPrefix}Debug`;
        case AcceleratorCategory.kDeveloper:
          return `${categoryPrefix}Developer`;
        default: {
          // If this case is reached, then an invalid category was passed in.
          assertNotReached();
        }
      }
    };

const subcategoryPrefix = 'subcategory';
export const getSubcategoryNameStringId =
    (subcategory: AcceleratorSubcategory): string => {
      switch (subcategory) {
        case AcceleratorSubcategory.kGeneral:
          return `${subcategoryPrefix}General`;
        case AcceleratorSubcategory.kSystemApps:
          return `${subcategoryPrefix}SystemApps`;
        case AcceleratorSubcategory.kSystemControls:
          return `${subcategoryPrefix}SystemControls`;
        default: {
          // If this case is reached, then an invalid category was passed in.
          assertNotReached();
        }
      }
    };

export const getAccelerator =
    (acceleratorInfo: DefaultAcceleratorInfo): Accelerator => {
      return acceleratorInfo.layoutProperties.defaultAccelerator.accelerator;
    };
