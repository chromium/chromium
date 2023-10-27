// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Refers to the state of an 'input-key' item.
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
}

export const Modifiers: Modifier[] = [
  Modifier.SHIFT,
  Modifier.CONTROL,
  Modifier.ALT,
  Modifier.COMMAND,
];

export enum AllowedModifierKeyCodes {
  SHIFT = 16,
  CTRL = 17,
  ALT = 18,
  META_LEFT = 91,
  META_RIGHT = 92,
}

export const ModifierKeyCodes: AllowedModifierKeyCodes[] = [
  AllowedModifierKeyCodes.SHIFT,
  AllowedModifierKeyCodes.ALT,
  AllowedModifierKeyCodes.CTRL,
  AllowedModifierKeyCodes.META_LEFT,
  AllowedModifierKeyCodes.META_RIGHT,
];

export const getSortedModifiers = (modifierStrings: string[]): string[] => {
  const sortOrder = ['meta', 'ctrl', 'alt', 'shift'];
  if (modifierStrings.length <= 1) {
    return modifierStrings;
  }
  return modifierStrings.sort(
      (a, b) => sortOrder.indexOf(a) - sortOrder.indexOf(b));
};

// The keys in this map are pulled from the file:
// ui/events/keycodes/dom/dom_code_data.inc
export const KeyToIconNameMap: {[key: string]: string|undefined} = {
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
