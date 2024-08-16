// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';

import {StandardAcceleratorProperties} from './accelerator_info.mojom-webui.js';
import * as MetaKeyTypes from './meta_key.mojom-webui.js';
import {ShortcutInputKeyElement} from './shortcut_input_key.js';

export interface ShortcutLabelProperties extends StandardAcceleratorProperties {
  shortcutLabelText: TrustedHTML;
  metaKey: MetaKey;
}

/**
 * Refers to the state of an 'shortcut-input-key' item.
 */
export enum KeyInputState {
  NOT_SELECTED = 'not-selected',
  MODIFIER_SELECTED = 'modifier-selected',
  ALPHANUMERIC_SELECTED = 'alpha-numeric-selected',
}

export enum Modifier {
  NONE = 0,
  SHIFT = 1 << 1,
  CONTROL = 1 << 2,
  ALT = 1 << 3,
  COMMAND = 1 << 4,
  FN_KEY = 1 << 5,
}

export const Modifiers: Modifier[] = [
  Modifier.SHIFT,
  Modifier.CONTROL,
  Modifier.ALT,
  Modifier.COMMAND,
  Modifier.FN_KEY,
];

export enum AllowedModifierKeyCodes {
  SHIFT = 16,
  CTRL = 17,
  ALT = 18,
  META_LEFT = 91,
  META_RIGHT = 92,
  FN_KEY = 255,
}

export const ModifierKeyCodes: AllowedModifierKeyCodes[] = [
  AllowedModifierKeyCodes.SHIFT,
  AllowedModifierKeyCodes.ALT,
  AllowedModifierKeyCodes.CTRL,
  AllowedModifierKeyCodes.META_LEFT,
  AllowedModifierKeyCodes.META_RIGHT,
  AllowedModifierKeyCodes.FN_KEY,
];

/**
 * Enumeration of meta key denoting all the possible options deducable from
 * the users keyboard. Used to show the correct key to the user in the settings
 * UI.
 */
export type MetaKey = MetaKeyTypes.MetaKey;
export const MetaKey = MetaKeyTypes.MetaKey;

export const getSortedModifiers = (modifierStrings: string[]): string[] => {
  const sortOrder = ['meta', 'ctrl', 'alt', 'shift', 'fn'];
  if (modifierStrings.length <= 1) {
    return modifierStrings;
  }
  return modifierStrings.sort(
      (a, b) => sortOrder.indexOf(a) - sortOrder.indexOf(b));
};

// The keys in this map are pulled from the file:
// ui/events/keycodes/dom/dom_code_data.inc
export const KeyToIconNameMap: {[key: string]: string|undefined} = {
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
  'Settings': 'settings-icon',
  'Standby': 'lock',
  'ZoomToggle': 'fullscreen',
};

// <if expr="_google_chrome" >
export const KeyToInternalIconNameMap: {[key: string]: string|undefined} = {
  'RightAlt': 'right-alt',
};

export const KeyToInternalIconNameRefreshOnlyMap:
    {[key: string]: string|undefined} = {
      'LaunchApplication1': 'overview-refresh',
      'BrightnessUp': 'brightness-up-refresh',
    };
// </if>

/**
 * Map the modifier keys to the bit value. Currently the modifiers only
 * contains the following four.
 */
export const modifierBitMaskToString = new Map<number, string>([
  [Modifier.CONTROL, 'ctrl'],
  [Modifier.SHIFT, 'shift'],
  [Modifier.ALT, 'alt'],
  [Modifier.COMMAND, 'command'],
]);

export function createInputKeyParts(
    shortcutLabelProperties: ShortcutLabelProperties,
    useNarrowLayout: boolean = false): ShortcutInputKeyElement[] {
  const inputKeys: ShortcutInputKeyElement[] = [];
  const pressedModifiers: string[] = [];
  for (const [bitValue, modifierName] of modifierBitMaskToString) {
    if ((shortcutLabelProperties.accelerator.modifiers & bitValue) !== 0) {
      const key: ShortcutInputKeyElement =
          document.createElement('shortcut-input-key');
      key.keyState = KeyInputState.MODIFIER_SELECTED;
      // Current use cases outside keyboard page or shortcut page only consider
      // 'meta' instead of 'command'.
      key.key = modifierName === 'command' ? 'meta' : modifierName;
      key.metaKey = shortcutLabelProperties.metaKey;
      key.narrow = useNarrowLayout;
      inputKeys.push(key);
      pressedModifiers.push(modifierName);
    }
  }

  const keyDisplay = mojoString16ToString(shortcutLabelProperties.keyDisplay);
  if (!pressedModifiers.includes(keyDisplay.toLowerCase())) {
    const key = document.createElement('shortcut-input-key');
    key.keyState = KeyInputState.ALPHANUMERIC_SELECTED;
    key.key = keyDisplay;
    key.narrow = useNarrowLayout;
    inputKeys.push(key);
  }

  return inputKeys;
}

// TODO(b/340609992): Encapsulate this as a new element too.
export function createShortcutAppendedKeyLabel(
    shortcutLabelProperties: ShortcutLabelProperties,
    useNarrowLayout: boolean = false): HTMLDivElement {
  const reminder = document.createElement('div');
  reminder.innerHTML = shortcutLabelProperties.shortcutLabelText;

  // TODO(b/340609992): Move this out of the helper function as a new element.
  const keyCodes = document.createElement('span');
  keyCodes.append(
      ...createInputKeyParts(shortcutLabelProperties, useNarrowLayout));
  reminder.firstElementChild!.replaceWith(keyCodes);
  reminder.classList.add('reminder-label');
  return reminder;
}
