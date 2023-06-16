// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as InputDeviceSettingsTypes from '../mojom-webui/input_device_settings.mojom-webui.js';
import * as InputDeviceSettingsProviderTypes from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import * as ModifierKeyTypes from '../mojom-webui/modifier_key.mojom-webui.js';
import * as SimulateRightClickModifierTypes from '../mojom-webui/simulate_right_click_modifier.mojom-webui.js';
import * as SixPackShortcutModifierTypes from '../mojom-webui/six_pack_shortcut_modifier.mojom-webui.js';

/**
 * @fileoverview
 * Type alias for the add input device settings API.
 */

export type MetaKey = InputDeviceSettingsTypes.MetaKey;
export const MetaKey = InputDeviceSettingsTypes.MetaKey;
export type ModifierKey = ModifierKeyTypes.ModifierKey;
export const ModifierKey = ModifierKeyTypes.ModifierKey;

export type SimulateRightClickModifier =
    SimulateRightClickModifierTypes.SimulateRightClickModifier;
export const SimulateRightClickModifier =
    SimulateRightClickModifierTypes.SimulateRightClickModifier;

export type SixPackShortcutModifier =
    SixPackShortcutModifierTypes.SixPackShortcutModifier;
export const SixPackShortcutModifier =
    SixPackShortcutModifierTypes.SixPackShortcutModifier;

export type SixPackKeyInfo = InputDeviceSettingsTypes.SixPackKeyInfo;

export type PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;
export const PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;

export type Keyboard = InputDeviceSettingsTypes.Keyboard;
export type Touchpad = InputDeviceSettingsTypes.Touchpad;
export type Mouse = InputDeviceSettingsTypes.Mouse;
export type PointingStick = InputDeviceSettingsTypes.PointingStick;

export interface Stylus {
  // Unique per device based on this VID/PID pair as follows: "<vid>:<pid>"
  // where VID/PID are represented in lowercase hex
  deviceKey: string;
  id: number;
  name: string;
  // TODO(yyhyyh@): Add Stylus settings with buttonRemapping: ButtonRemapping[]
  // setting.
}

export interface GraphicsTablet {
  // Unique per device based on this VID/PID pair as follows: "<vid>:<pid>"
  // where VID/PID are represented in lowercase hex
  deviceKey: string;
  id: number;
  name: string;
  // TODO(yyhyyh@): Add GraphicsTablet settings with
  // buttonRemapping: ButtonRemapping[] setting.
}

export type KeyboardSettings = InputDeviceSettingsTypes.KeyboardSettings;
export type TouchpadSettings = InputDeviceSettingsTypes.TouchpadSettings;
export type MouseSettings = InputDeviceSettingsTypes.MouseSettings;
export type PointingStickSettings =
    InputDeviceSettingsTypes.PointingStickSettings;
export type DeviceSettings =
    KeyboardSettings|TouchpadSettings|MouseSettings|PointingStickSettings;

export type InputDeviceSettingsPolicy =
    InputDeviceSettingsTypes.InputDeviceSettingsPolicy;
export type KeyboardPolicies = InputDeviceSettingsTypes.KeyboardPolicies;
export type MousePolicies = InputDeviceSettingsTypes.MousePolicies;

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

export interface StylusObserverInterface {
  // Fired when the stylus list is updated.
  onStylusListUpdated(styluses: Stylus[]): void;
}

export interface GraphicsTabletObserverInterface {
  // Fired when the graphics tablet list is updated.
  onGraphicsTabletListUpdated(graphicsTablet: GraphicsTablet[]): void;
}

interface FakeInputDeviceSettingsProviderInterface extends
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface {
  RestoreDefaultKeyboardRemappings(id: number): void;
  setKeyboardSettings(id: number, settings: KeyboardSettings): void;
  setMouseSettings(id: number, settings: MouseSettings): void;
  setTouchpadSettings(id: number, settings: TouchpadSettings): void;
  setPointingStickSettings(id: number, settings: PointingStickSettings): void;
}

// Type alias to enable use of in-progress InputDeviceSettingsProvider api.
export type InputDeviceSettingsProviderInterface = Required<
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface>&
    Partial<FakeInputDeviceSettingsProviderInterface>;
