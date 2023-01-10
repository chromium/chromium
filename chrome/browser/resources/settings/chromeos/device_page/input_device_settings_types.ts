// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type alias for the add input device settings API.
 */

/** Enumeration of MetaKey. */
export enum MetaKey {
  COMMAND,
  EXTERNAL_META,
  LAUNCHER,
  SEARCH,
}

/** Enumeration of ModifierKey. */
export enum ModifierKey {
  ALT,
  ASSISTANT,
  BACKSPACE,
  CAPS_LOCK,
  CONTROL,
  ESC,
  META,
  VOID,
}

export interface Keyboard {
  // Unique per device based on this VID/PID pair as follows: "<vid>:<pid>"
  // where VID/PID are represented in lowercase hex
  id: number;
  name: string;
  // This property represents whether or not the keyboard is an external device.
  isExternal: boolean;
  // Meta key (launcher, search, etc) for this device.
  metaKey: MetaKey;
  // List of modifier keys (caps lock, assistant, etc) present on this device.
  modifierKeys: ModifierKey[];
  settings: KeyboardSettings;
}

export interface Touchpad {
  id: number;
  name: string;
  // This property represents whether or not the touchpad is an external device.
  isExternal: boolean;
  // Some settings are only available on haptic touchpads.
  isHaptic: boolean;
}

export interface KeyboardSettings {
  // TODO: Populate KeyboardSettings interface.
}

export interface KeyboardObserverInterface {
  // Fired when the list of keyboards changes.
  onKeyboardConnected(keyboard: Keyboard): void;
  // Fired when a keyboard is removed from the list.
  onKeyboardDisconnected(id: number): void;
}

export interface TouchpadObserverInterface {
  // Fired when the list of touchpads changes.
  onTouchpadsConnected(touchpads: Touchpad[]): void;
  // Fired when a touchpad is removed from the list.
  onTouchpadsDisconnected(id: number): void;
}

export interface InputDeviceSettingsProviderInterface {
  observeKeyboardSettings(observer: KeyboardObserverInterface): void;
  stopObserveKeyboardSettings(observer: KeyboardObserverInterface): void;
  observeTouchpadSettings(observer: TouchpadObserverInterface): void;
  stopObserveTouchpadSettings(observer: TouchpadObserverInterface): void;
}
