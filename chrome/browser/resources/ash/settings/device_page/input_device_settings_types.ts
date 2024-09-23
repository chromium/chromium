// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as AcceleratorActionTypes from '../mojom-webui/accelerator_actions.mojom-webui.js';
import * as AcceleratorKeysTypes from '../mojom-webui/accelerator_keys.mojom-webui.js';
import * as ExtendedFkeysModifierTypes from '../mojom-webui/extended_fkeys_modifier.mojom-webui.js';
import * as InputDeviceSettingsTypes from '../mojom-webui/input_device_settings.mojom-webui.js';
import * as InputDeviceSettingsProviderTypes from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import * as MetaKeyTypes from '../mojom-webui/meta_key.mojom-webui.js';
import * as ModifierKeyTypes from '../mojom-webui/modifier_key.mojom-webui.js';
import * as ShortcutInputProviderTypes from '../mojom-webui/shortcut_input_provider.mojom-webui.js';
import * as SimulateRightClickModifierTypes from '../mojom-webui/simulate_right_click_modifier.mojom-webui.js';
import * as SixPackShortcutModifierTypes from '../mojom-webui/six_pack_shortcut_modifier.mojom-webui.js';

/**
 * @fileoverview
 * Type alias for the add input device settings API.
 */

export type MetaKey = MetaKeyTypes.MetaKey;
export const MetaKey = MetaKeyTypes.MetaKey;
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

export type ExtendedFkeysModifier =
    ExtendedFkeysModifierTypes.ExtendedFkeysModifier;
export const ExtendedFkeysModifier =
    ExtendedFkeysModifierTypes.ExtendedFkeysModifier;

export type SixPackKeyInfo = InputDeviceSettingsTypes.SixPackKeyInfo;

export type TopRowActionKey = InputDeviceSettingsTypes.TopRowActionKey;
export const TopRowActionKey = InputDeviceSettingsTypes.TopRowActionKey;

export enum SixPackKey {
  DELETE = 'del',
  INSERT = 'insert',
  PAGE_UP = 'pageUp',
  PAGE_DOWN = 'pageDown',
  HOME = 'home',
  END = 'end',
}

export enum Fkey {
  F11 = 'f11',
  F12 = 'f12',
}

export type PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;
export const PolicyStatus = InputDeviceSettingsTypes.PolicyStatus;

export type KeyboardSettings = InputDeviceSettingsTypes.KeyboardSettings;
export type Keyboard = InputDeviceSettingsTypes.Keyboard;

export type Touchpad = InputDeviceSettingsTypes.Touchpad;
export type Mouse = InputDeviceSettingsTypes.Mouse;
export type PointingStick = InputDeviceSettingsTypes.PointingStick;
export type GraphicsTablet = InputDeviceSettingsTypes.GraphicsTablet;

export type BatteryInfo = InputDeviceSettingsTypes.BatteryInfo;
export type CompanionAppInfo = InputDeviceSettingsTypes.CompanionAppInfo;

export interface Stylus {
  // Unique per device based on this VID/PID pair as follows: "<vid>:<pid>"
  // where VID/PID are represented in lowercase hex
  deviceKey: string;
  id: number;
  name: string;
  // TODO(yyhyyh@): Add Stylus settings with buttonRemapping: ButtonRemapping[]
  // setting.
}

export interface GraphicsTabletSettings {
  tabletButtonRemappings: ButtonRemapping[];
  penButtonRemappings: ButtonRemapping[];
}

export type TouchpadSettings = InputDeviceSettingsTypes.TouchpadSettings;
export type MouseSettings = InputDeviceSettingsTypes.MouseSettings;
export type PointingStickSettings =
    InputDeviceSettingsTypes.PointingStickSettings;
export type DeviceSettings =
    KeyboardSettings|TouchpadSettings|MouseSettings|PointingStickSettings;
export type InputDeviceType =
    Keyboard|Touchpad|Mouse|PointingStick|GraphicsTablet;

export type InputDeviceSettingsPolicy =
    InputDeviceSettingsTypes.InputDeviceSettingsPolicy;

export type InputDeviceSettingsFkeyPolicy =
    InputDeviceSettingsTypes.InputDeviceSettingsFkeyPolicy;

export type InputDeviceSettingsSixPackKeyPolicy =
    InputDeviceSettingsTypes.InputDeviceSettingsSixPackKeyPolicy;

export type KeyboardPolicies = InputDeviceSettingsTypes.KeyboardPolicies;
export type MousePolicies = InputDeviceSettingsTypes.MousePolicies;

export type ActionChoice = InputDeviceSettingsProviderTypes.ActionChoice;

/** Enumeration of accelerator types. */
export type Vkey = AcceleratorKeysTypes.VKey;
export const Vkey = AcceleratorKeysTypes.VKey;

/** Enumeration of accelerator actions. */
export type AcceleratorAction = AcceleratorActionTypes.AcceleratorAction;
export const AcceleratorAction = AcceleratorActionTypes.AcceleratorAction;

export interface FakeKeyEvent {
  vkey: AcceleratorKeysTypes.VKey;
  domCode: number;
  domKey: number;
  modifiers: number;
  keyDisplay: string;
}

export type Button = InputDeviceSettingsTypes.Button;
export type ButtonRemapping = InputDeviceSettingsTypes.ButtonRemapping;

export type RemappingAction = InputDeviceSettingsTypes.RemappingAction;

export type ChargeState = InputDeviceSettingsTypes.ChargeState;
export const ChargeState = InputDeviceSettingsTypes.ChargeState;

export type KeyEvent =
    Required<InputDeviceSettingsTypes.KeyEvent>&Partial<FakeKeyEvent>;

export type CustomizableButton = InputDeviceSettingsTypes.CustomizableButton;
export const CustomizableButton = InputDeviceSettingsTypes.CustomizableButton;

export type StaticShortcutAction =
    InputDeviceSettingsTypes.StaticShortcutAction;
export const StaticShortcutAction =
    InputDeviceSettingsTypes.StaticShortcutAction;

export type CustomizationRestriction =
    InputDeviceSettingsTypes.CustomizationRestriction;
export const CustomizationRestriction =
    InputDeviceSettingsTypes.CustomizationRestriction;

export type MouseButtonConfig = InputDeviceSettingsTypes.MouseButtonConfig;
export const MouseButtonConfig = InputDeviceSettingsTypes.MouseButtonConfig;

export type GraphicsTabletButtonConfig =
    InputDeviceSettingsTypes.GraphicsTabletButtonConfig;
export const GraphicsTabletButtonConfig =
    InputDeviceSettingsTypes.GraphicsTabletButtonConfig;

export type CompanionAppState = InputDeviceSettingsTypes.CompanionAppState;
export const CompanionAppState = InputDeviceSettingsTypes.CompanionAppState;

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

export interface KeyboardBrightnessObserverInterface {
  // Fired when the keyboard brightness is changed.
  onKeyboardBrightnessChanged(percent: number): void;
}

export interface KeyboardAmbientLightSensorObserverInterface {
  // Fired when the keyboard ambient light sensor is changed.
  onKeyboardAmbientLightSensorEnabledChanged(keyboardAmbientLightSensorEnabled:
                                                 boolean): void;
}

export interface LidStateObserverInterface {
  // Fired when the lid state is changed.
  onLidStateChanged(isLidOpen: boolean): void;
}

export type ButtonPressObserverInterface =
    InputDeviceSettingsProviderTypes.ButtonPressObserverInterface;

export type ButtonPressObserver =
    InputDeviceSettingsProviderTypes.ButtonPressObserver;

interface FakeInputDeviceSettingsProviderInterface extends
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface {}

// Type alias to enable use of in-progress InputDeviceSettingsProvider api.
export type InputDeviceSettingsProviderInterface = Required<
    InputDeviceSettingsProviderTypes.InputDeviceSettingsProviderInterface>&
    Partial<FakeInputDeviceSettingsProviderInterface>;

export type ShortcutInputProviderInterface =
    ShortcutInputProviderTypes.ShortcutInputProviderInterface;
