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
  settings: TouchpadSettings;
}

export interface Mouse {
  id: number;
  name: string;
  // This property represents whether or not the mouse is an external device.
  isExternal: boolean;
  settings: MouseSettings;
}

export interface PointingStick {
  id: number;
  name: string;
  // This property represents whether or not the pointing stick is an
  // external device.
  isExternal: boolean;
  settings: PointingStickSettings;
}

export interface KeyboardSettings {
  modifierRemappings: Map<ModifierKey, ModifierKey>;
  topRowAreFKeys: boolean;
  suppressMetaFKeyRewrites: boolean;
  autoRepeatEnabled: boolean;
  autoRepeatDelay: number;
  autoRepeatInterval: number;
}

export interface TouchpadSettings {
  sensitivity: number;
  reverseScrolling: boolean;
  accelerationEnabled: boolean;
  tapToClickEnabled: boolean;
  threeFingerClickEnabled: boolean;
  tapDraggingEnabled: boolean;
  scrollSensitivity: number;
  scrollAcceleration: boolean;
  hapticSensitivity: number;
  hapticEnabled: boolean;
}

export interface MouseSettings {
  swapRight: boolean;
  sensitivity: number;
  reverseScrolling: boolean;
  accelerationEnabled: boolean;
  scrollSensitivity: number;
  scrollAcceleration: boolean;
}

export interface PointingStickSettings {
  swapRight: boolean;
  sensitivity: number;
  accelerationEnabled: boolean;
}

export interface KeyboardObserverInterface {
  // Fired when the keyboard list is updated.
  onKeyboardListUpdated(keyboards: Keyboard[]): void;
}

export interface TouchpadObserverInterface {
  // Fired when the touchpad list is updated.
  onTouchpadListUpdated(touchpads: Touchpad[]): void;
}

export interface MouseObserverInterface {
  // Fired when the mouse list updated.
  onMouseListUpdated(mice: Mouse[]): void;
}


export interface PointingStickObserverInterface {
  // Fired when the pointing stick list is updated.
  onPointingStickListUpdated(pointingSticks: PointingStick[]): void;
}

export interface InputDeviceSettingsProviderInterface {
  observeKeyboardSettings(observer: KeyboardObserverInterface): void;
  getConnectedKeyboardSettings(): Promise<Keyboard[]>;
  observeTouchpadSettings(observer: TouchpadObserverInterface): void;
  getConnectedTouchpadSettings(): Promise<Touchpad[]>;
  observeMouseSettings(observer: MouseObserverInterface): void;
  getConnectedMouseSettings(): Promise<Mouse[]>;
  observePointingStickSettings(observer: PointingStickObserverInterface): void;
  getConnectedPointingStickSettings(): Promise<PointingStick[]>;
  setKeyboardSettings(id: number, settings: KeyboardSettings): void;
  setMouseSettings(id: number, settings: MouseSettings): void;
  setTouchpadSettings(id: number, settings: TouchpadSettings): void;
  setPointingStickSettings(id: number, settings: PointingStickSettings): void;
}
