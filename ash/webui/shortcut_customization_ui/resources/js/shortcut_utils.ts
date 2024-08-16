// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {VKey as ash_mojom_VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {ModifierKeyCodes} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
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
  Modifier.FN_KEY,
];

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
  'Accessibility': 'accessibility',
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
        acceleratorLocked: false,
        locked: false,
        state: AcceleratorState.kEnabled,
        type: AcceleratorType.kUser,
      };
    };

export const createEmptyAcceleratorInfo = (): StandardAcceleratorInfo => {
  return createEmptyAccelInfoFromAccel(
      {modifiers: 0, keyCode: 0, keyState: AcceleratorKeyState.PRESSED});
};

export const resetKeyEvent = (): KeyEvent => {
  return {
    vkey: ash_mojom_VKey.MIN_VALUE,
    domCode: 0,
    domKey: 0,
    modifiers: 0,
    keyDisplay: '',
  };
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
        case AcceleratorSubcategory.kMouseKeys:
          return `${subcategoryPrefix}MouseKeys`;
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
  const sortOrder = ['meta', 'ctrl', 'alt', 'shift', 'fn'];
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
    case Modifier.FN_KEY:
      return 'fn';
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

export const isModifierKey = (keycode: number): boolean => {
  return ModifierKeyCodes.includes(keycode);
};

export const isValidAccelerator = (accelerator: Accelerator): boolean => {
  // A valid default accelerator is one that has modifier(s) and a key or
  // is function key.
  return (accelerator.modifiers > 0 && accelerator.keyCode > 0) ||
      isFunctionKey(accelerator.keyCode);
};

export const containsAccelerator =
    (accelerators: Accelerator[], accelerator: Accelerator): boolean => {
      return accelerators.some(
          accel => areAcceleratorsEqual(accel, accelerator));
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
 * @returns the associated icon label for the given `keyOrIcon` text if it
 *     exists, otherwise returns `keyOrIcon` itself.
 */
export const getKeyDisplay = (keyOrIcon: string): string => {
  const iconName = keyToIconNameMap[keyOrIcon];
  return iconName ? loadTimeData.getString(`iconLabel${keyOrIcon}`) : keyOrIcon;
};

/**
 * Translate a numpadKey code to a display string.
 */
export const getNumpadKeyDisplay = (code: string): string => {
  // For "NumpadEnter", it is the same as "enter" key.
  if (code === 'NumpadEnter') {
    return 'enter';
  }
  // Map of special numpad key codes to their display symbols.
  const numpadKeyMap: {[code: string]: string} = {
    'NumpadAdd': '+',
    'NumpadDecimal': '.',
    'NumpadDivide': '/',
    'NumpadMultiply': '*',
    'NumpadSubtract': '-',
  };

  // Return the formatted string, using the map for special keys,
  // or stripping 'Numpad' for numeric keys.
  const numpadKey = numpadKeyMap[code] || code.replace('Numpad', '');
  return `numpad ${numpadKey}`.toLowerCase();
};

/**
 * Translate an unidentified key to a display string.
 */
export const getUnidentifiedKeyDisplay = (e: KeyboardEvent): string => {
  if (e.code === 'Backquote') {
    // Backquote `key` will become 'unidentified' when ctrl
    // is pressed.
    if (e.ctrlKey) {
      return unidentifiedKeyCodeToKey[e.keyCode];
    }
    return e.key;
  }
  if (e.code === '') {
    // If there is no `code`, check the `key`. If the `key` is
    // `unidentified`, we need to manually lookup the key.
    return unidentifiedKeyCodeToKey[e.keyCode] || e.key;
  }

  return `Key ${e.keyCode}`;
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

export const getModifiersFromKeyboardEvent = (e: KeyboardEvent): Modifier => {
  let modifiers = 0;
  if (e.metaKey) {
    modifiers |= Modifier.COMMAND;
  }
  if (e.ctrlKey) {
    modifiers |= Modifier.CONTROL;
  }
  if (e.altKey) {
    modifiers |= Modifier.ALT;
  }
  if (e.key == 'Shift' || e.shiftKey) {
    modifiers |= Modifier.SHIFT;
  }
  return modifiers;
};

export const getKeyDisplayFromKeyboardEvent = (e: KeyboardEvent): string => {
  // Handle numpad keys:
  if (e.code.startsWith('Numpad')) {
    return getNumpadKeyDisplay(e.code);
  }
  // Handle unidentified keys:
  if (e.key === 'Unidentified' || e.code === '') {
    return getUnidentifiedKeyDisplay(e);
  }

  switch (e.code) {
    case 'Space':  // Space key: e.key: ' ', e.code: 'Space', set keyDisplay
      // to be 'space' text.
      return 'space';
    case 'ShowAllWindows':  // Overview key: e.key: 'F4', e.code:
      // 'ShowAllWindows', set keyDisplay to be
      // 'LaunchApplication1' and will display as
      // 'overview' icon.
      return 'LaunchApplication1';
    default:  // All other keys: Use the original e.key as keyDisplay.
      return e.key;
  }
};

export const keyEventToAccelerator = (keyEvent: KeyEvent): Accelerator => {
  const output: Accelerator = {
    modifiers: 0,
    keyCode: 0,
    keyState: AcceleratorKeyState.PRESSED,
  };
  output.modifiers = keyEvent.modifiers;
  if (!isModifierKey(keyEvent.vkey) || isFunctionKey(keyEvent.vkey)) {
    output.keyCode = keyEvent.vkey;
  }

  return output;
};
