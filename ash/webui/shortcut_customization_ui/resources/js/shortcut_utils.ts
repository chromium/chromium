// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorInfo, AcceleratorState, AcceleratorSubcategory, AcceleratorType, MojoAcceleratorInfo, StandardAcceleratorInfo, TextAcceleratorInfo} from './shortcut_types.js';

// Returns true if shortcut customization is disabled via the feature flag.
export const isCustomizationDisabled = (): boolean => {
  return !loadTimeData.getBoolean('isCustomizationEnabled');
};

// Returns true if search is enabled via the feature flag.
export const isSearchEnabled = (): boolean => {
  return loadTimeData.getBoolean('isSearchEnabled');
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

export const isStandardAcceleratorInfo =
    (accelInfo: AcceleratorInfo|MojoAcceleratorInfo):
        accelInfo is StandardAcceleratorInfo => {
          return !!(accelInfo as StandardAcceleratorInfo)
                       .layoutProperties.standardAccelerator;
        };

export const createEmptyAccelInfoFromAccel =
    (accel: Accelerator): StandardAcceleratorInfo => {
      return {
        layoutProperties:
            {standardAccelerator: {accelerator: accel, keyDisplay: ''}},
        locked: false,
        state: AcceleratorState.kEnabled,
        type: AcceleratorType.kUser,
      };
    };

export const createEmptyAcceleratorInfo = (): StandardAcceleratorInfo => {
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
        case AcceleratorCategory.kGeneral:
          return `${categoryPrefix}General`;
        case AcceleratorCategory.kDevice:
          return `${categoryPrefix}Device`;
        case AcceleratorCategory.kBrowser:
          return `${categoryPrefix}Browser`;
        case AcceleratorCategory.kText:
          return `${categoryPrefix}Text`;
        case AcceleratorCategory.kWindowsAndDesks:
          return `${categoryPrefix}WindowsAndDesks`;
        case AcceleratorCategory.kDebug:
          return `${categoryPrefix}Debug`;
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
        case AcceleratorSubcategory.kGeneralControls:
          return `${subcategoryPrefix}GeneralControls`;
        case AcceleratorSubcategory.kApps:
          return `${subcategoryPrefix}Apps`;
        case AcceleratorSubcategory.kMedia:
          return `${subcategoryPrefix}Media`;
        case AcceleratorSubcategory.kInputs:
          return `${subcategoryPrefix}Inputs`;
        case AcceleratorSubcategory.kDisplay:
          return `${subcategoryPrefix}Display`;
        case AcceleratorSubcategory.kGeneral:
          return `${subcategoryPrefix}General`;
        case AcceleratorSubcategory.kBrowserNavigation:
          return `${subcategoryPrefix}BrowserNavigation`;
        case AcceleratorSubcategory.kPages:
          return `${subcategoryPrefix}Pages`;
        case AcceleratorSubcategory.kTabs:
          return `${subcategoryPrefix}Tabs`;
        case AcceleratorSubcategory.kBookmarks:
          return `${subcategoryPrefix}Bookmarks`;
        case AcceleratorSubcategory.kDeveloperTools:
          return `${subcategoryPrefix}DeveloperTools`;
        case AcceleratorSubcategory.kTextNavigation:
          return `${subcategoryPrefix}TextNavigation`;
        case AcceleratorSubcategory.kTextEditing:
          return `${subcategoryPrefix}TextEditing`;
        case AcceleratorSubcategory.kWindows:
          return `${subcategoryPrefix}Windows`;
        case AcceleratorSubcategory.kDesks:
          return `${subcategoryPrefix}Desks`;
        case AcceleratorSubcategory.kChromeVox:
          return `${subcategoryPrefix}ChromeVox`;
        case AcceleratorSubcategory.kVisibility:
          return `${subcategoryPrefix}Visibility`;
        case AcceleratorSubcategory.kAccessibilityNavigation:
          return `${subcategoryPrefix}AccessibilityNavigation`;
        default: {
          // If this case is reached, then an invalid category was passed in.
          assertNotReached();
        }
      }
    };

export const getAccelerator =
    (acceleratorInfo: StandardAcceleratorInfo): Accelerator => {
      return acceleratorInfo.layoutProperties.standardAccelerator.accelerator;
    };
