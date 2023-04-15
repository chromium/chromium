// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as InputDeviceSettingsTypes from '../mojom-webui/input_device_settings.mojom-webui.js';
import * as InputDeviceSettingsProviderTypes from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import * as ModifierKeyTypes from '../mojom-webui/modifier_key.mojom-webui.js';

/**
 * @fileoverview
 * Type alias for the add input device settings API.
 */

export type MetaKey = InputDeviceSettingsTypes.MetaKey;
export const MetaKey = InputDeviceSettingsTypes.MetaKey;
export type ModifierKey = ModifierKeyTypes.ModifierKey;
export const ModifierKey = ModifierKeyTypes.ModifierKey;

export type PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;
export const PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;

export type Keyboard = InputDeviceSettingsTypes.Keyboard;
export type Touchpad = InputDeviceSettingsTypes.Touchpad&
                       Partial<{isExternal: boolean, isHaptic: boolean}>;
export type Mouse =
    InputDeviceSettingsTypes.Mouse&Partial<{isExternal: boolean}>;
export type PointingStick =
    InputDeviceSettingsTypes.PointingStick&Partial<{isExternal: boolean}>;

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

interface FakeInputDeviceSettingsProviderInterface extends
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface {
  setKeyboardSettings(id: number, settings: KeyboardSettings): void;
  setMouseSettings(id: number, settings: MouseSettings): void;
  setTouchpadSettings(id: number, settings: TouchpadSettings): void;
  setPointingStickSettings(id: number, settings: PointingStickSettings): void;
}

// Type alias to enable use of in-progress InputDeviceSettingsProvider api.
export type InputDeviceSettingsProviderInterface = Required<
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface>&
    Partial<FakeInputDeviceSettingsProviderInterface>;
