// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';

import {Accelerator, AcceleratorCategory, AcceleratorConfigResult, AcceleratorId, AcceleratorInfo, AcceleratorKeyState, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, Modifier, MojoAcceleratorInfo, MojoSearchResult, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from './shortcut_types.js';

// TODO(jimmyxgong): ChromeOS currently supports up to F24 but can be updated to
// F32. Update here when F32 is available.
const kF11 = 112;  // Keycode for F11.
const kF24 = 135;  // Keycode for F24.

const kMeta = 91;  // Keycode for Meta.

const modifiers: Modifier[] = [
  Modifier.SHIFT,
  Modifier.CONTROL,
  Modifier.ALT,
  Modifier.COMMAND,
];

export const keyCodeToModifier: {[keyCode: number]: number} = {
  16: Modifier.SHIFT,
  17: Modifier.CONTROL,
  18: Modifier.ALT,
  91: Modifier.COMMAND,
  92: Modifier.COMMAND,
};

export const unidentifiedKeyCodeToKey: {[keyCode: number]: string} = {
  159: 'MicrophoneMuteToggle',
  192: '`',  // Backquote key.
  218: 'KeyboardBrightnessUp',
  232: 'KeyboardBrightnessDown',
  237: 'EmojiPicker',
  238: 'EnableOrToggleDictation',
  239: 'ViewAllApps',
};

// The keys in this map are pulled from the file:
// ui/events/keycodes/dom/dom_code_data.inc
export const keyToIconNameMap: {[key: string]: string|undefined} = {
  'ArrowDown': 'arrow-down',
  'ArrowLeft': 'arrow-left',
  'ArrowRight': 'arrow-right',
  'ArrowUp': 'arrow-up',
  'AudioVolumeDown': 'volume-down',
  'AudioVolumeMute': 'volume-mute',
  'AudioVolumeUp': 'volume-up',
  'BrightnessDown': 'display-brightness-down',
  'BrightnessUp': 'display-brightness-up',
  'BrowserBack': 'back',
  'BrowserForward': 'forward',
  'BrowserHome': 'browser-home',
  'BrowserRefresh': 'refresh',
  'BrowserSearch': 'browser-search',
  'ContextMenu': 'menu',
  'EmojiPicker': 'emoji-picker',
  'EnableOrToggleDictation': 'dictation-toggle',
  'KeyboardBacklightToggle': 'keyboard-brightness-toggle',
  'KeyboardBrightnessUp': 'keyboard-brightness-up',
  'KeyboardBrightnessDown': 'keyboard-brightness-down',
  'LaunchApplication1': 'overview',
  'LaunchApplication2': 'calculator',
  'LaunchAssistant': 'assistant',
  'LaunchMail': 'launch-mail',
  'MediaFastForward': 'fast-forward',
  'MediaPause': 'pause',
  'MediaPlay': 'play',
  'MediaPlayPause': 'play-pause',
  'MediaTrackNext': 'next-track',
  'MediaTrackPrevious': 'last-track',
  'MicrophoneMuteToggle': 'microphone-mute',
  'ModeChange': 'globe',
  'ViewAllApps': 'view-all-apps',
  'Power': 'power',
  'PrintScreen': 'screenshot',
  'PrivacyScreenToggle': 'electronic-privacy-screen',
  'Settings': 'settings',
  'Standby': 'lock',
  'ZoomToggle': 'fullscreen',
};

// Return true if shortcut customization is allowed.
export const isCustomizationAllowed = (): boolean => {
  return loadTimeData.getBoolean('isCustomizationAllowed');
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
  return createEmptyAccelInfoFromAccel(
      {modifiers: 0, keyCode: 0, keyState: AcceleratorKeyState.PRESSED});
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

export const areAcceleratorsEqual =
    (first: Accelerator, second: Accelerator): boolean => {
      return first.keyCode === second.keyCode &&
          first.modifiers === second.modifiers &&
          first.keyState === second.keyState;
    };

/**
 * Checks if a retry can bypass the last error. Returns true for
 * kConflictCanOverride or kNonSearchAcceleratorWarning results.
 */
export const canBypassErrorWithRetry =
    (result: AcceleratorConfigResult): boolean => {
      return result === AcceleratorConfigResult.kConflictCanOverride ||
          result === AcceleratorConfigResult.kNonSearchAcceleratorWarning;
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

function isSearchOnlyAccelerator(accelerator: Accelerator): boolean {
  return accelerator.keyCode === kMeta &&
      accelerator.modifiers === Modifier.NONE;
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

  // Search/meta as the activation key should always be the highest priority.
  if (isSearchOnlyAccelerator(
          first.layoutProperties.standardAccelerator.accelerator)) {
    return -1;
  }

  if (isSearchOnlyAccelerator(
          second.layoutProperties.standardAccelerator.accelerator)) {
    return 1;
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
export const META_KEY = 'meta';
export const LWIN_KEY = 'Meta';

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

export const getSourceAndActionFromAcceleratorId =
    (uuid: AcceleratorId): {source: number, action: number} => {
      // Split '{source}-{action}` into [source][action].
      const uuidSplit = uuid.split('-');
      const source: AcceleratorSource = parseInt(uuidSplit[0], 10);
      const action = parseInt(uuidSplit[1], 10);

      return {source, action};
    };

/**
 *
 * @param keyOrIcon the text for an individual accelerator key.
 * @returns the associated icon name for the given `keyOrIcon` text if it
 *     exists, otherwise returns `keyOrIcon` itself.
 */
export const getKeyDisplay = (keyOrIcon: string): string => {
  const iconName = keyToIconNameMap[keyOrIcon];
  return iconName ? iconName : keyOrIcon;
};

/**
 * @returns the Aria label for the standard accelerators.
 */
export const getAriaLabelForStandardAccelerators =
    (acceleratorInfos: StandardAcceleratorInfo[], dividerString: string):
        string => {
          return acceleratorInfos
              .map(
                  (acceleratorInfo: StandardAcceleratorInfo) =>
                      getAriaLabelForStandardAcceleratorInfo(acceleratorInfo))
              .join(` ${dividerString} `);
        };

/**
 * @returns the Aria label for the text accelerators.
 */
export const getAriaLabelForTextAccelerators =
    (acceleratorInfos: TextAcceleratorInfo[]): string => {
      return getTextAcceleratorParts(acceleratorInfos as TextAcceleratorInfo[])
          .map(part => getKeyDisplay(mojoString16ToString(part.text)))
          .join('');
    };

/**
 * @returns the Aria label for the given StandardAcceleratorInfo.
 */
export const getAriaLabelForStandardAcceleratorInfo =
    (acceleratorInfo: StandardAcceleratorInfo): string => {
      const keyOrIcon =
          acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
      return getModifiersForAcceleratorInfo(acceleratorInfo)
          .join(' ')
          .concat(` ${getKeyDisplay(keyOrIcon)}`);
    };

/**
 * @returns the text accelerator parts for the given TextAcceleratorInfo.
 */
export const getTextAcceleratorParts =
    (infos: TextAcceleratorInfo[]): TextAcceleratorPart[] => {
      // For text based layout accelerators, we always expect this to be an
      // array with a single element.
      assert(infos.length === 1);
      const textAcceleratorInfo = infos[0];

      assert(isTextAcceleratorInfo(textAcceleratorInfo));
      return textAcceleratorInfo.layoutProperties.textAccelerator.parts;
    };
