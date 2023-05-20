// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorInfo, AcceleratorState, AcceleratorSubcategory, AcceleratorType, Modifier, MojoAcceleratorInfo, MojoSearchResult, StandardAcceleratorInfo, TextAcceleratorInfo} from './shortcut_types.js';

// TODO(jimmyxgong): ChromeOS currently supports up to F24 but can be updated to
// F32. Update here when F32 is available.
const kF11 = 112;  // Keycode for F11.
const kF24 = 135;  // Keycode for F24.

const modifiers: Modifier[] = [
  Modifier.SHIFT,
  Modifier.CONTROL,
  Modifier.ALT,
  Modifier.COMMAND,
];

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

/**
 * Sort the modifiers in the order of ctrl, alt, shift, meta.
 */
export const getSortedModifiers = (modifierStrings: string[]): string[] => {
  const sortOrder = ['meta', 'ctrl', 'alt', 'shift'];
  if (modifierStrings.length <= 1) {
    return modifierStrings;
  }
  return modifierStrings.sort(
      (a, b) => sortOrder.indexOf(a) - sortOrder.indexOf(b));
};

function getModifierCount(accelerator: Accelerator): number {
  let count = 0;

  for (const modifier of modifiers) {
    if (accelerator.modifiers & modifier) {
      ++count;
    }
  }
  return count;
}

// Comparison function that checks the number of modifiers in an accelerator.
// Lower number of modifiers get higher priority.
// @returns a negative number if the first accelerator info should be higher in
// the list, a positive number if it should be lower, 0 if it should have the
// same position
export function compareAcceleratorInfos(
    first: AcceleratorInfo, second: AcceleratorInfo): number {
  // Ignore non-standard accelerator infos as they only have one entry and is
  // a no-opt.
  if (!isStandardAcceleratorInfo(first) || !isStandardAcceleratorInfo(second)) {
    return 0;
  }

  const firstModifierCount =
      getModifierCount(first.layoutProperties.standardAccelerator.accelerator);
  const secondModifierCount =
      getModifierCount(second.layoutProperties.standardAccelerator.accelerator);
  return firstModifierCount - secondModifierCount;
}

/**
 * Returns the converted modifier flag as a readable string.
 * TODO(jimmyxgong): Localize, replace with icon, or update strings.
 */
export function getModifierString(modifier: Modifier): string {
  switch (modifier) {
    case Modifier.SHIFT:
      return 'shift';
    case Modifier.CONTROL:
      return 'ctrl';
    case Modifier.ALT:
      return 'alt';
    case Modifier.COMMAND:
      return 'meta';
    default:
      assertNotReached();
  }
}

/**
 * @returns the list of modifier keys for the given AcceleratorInfo.
 */
export function getModifiersForAcceleratorInfo(
    acceleratorInfo: StandardAcceleratorInfo): string[] {
  const modifierStrings: string[] = [];
  for (const modifier of modifiers) {
    if ((getAccelerator(acceleratorInfo)).modifiers & modifier) {
      modifierStrings.push(getModifierString(modifier));
    }
  }
  return getSortedModifiers(modifierStrings);
}

export const SHORTCUTS_APP_URL = 'chrome://shortcut-customization';

export const getURLForSearchResult = (searchResult: MojoSearchResult): URL => {
  const url = new URL(SHORTCUTS_APP_URL);
  const {action, category} = searchResult.acceleratorLayoutInfo;
  url.searchParams.append('action', action.toString());
  url.searchParams.append('category', category.toString());
  return url;
};

export const isFunctionKey = (keycode: number): boolean => {
  return keycode >= kF11 && keycode <= kF24;
};

// TODO(longbowei): Update to dynamically check if all shortcuts within a
// category are locked instead of hardcoding specific categories.
export const isCategoryLocked = (category: AcceleratorCategory): boolean => {
  return (
      category === AcceleratorCategory.kBrowser ||
      category === AcceleratorCategory.kText ||
      category === AcceleratorCategory.kAccessibility);
};
