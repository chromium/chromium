// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as AcceleratorActionTypes from '../mojom-webui/accelerator_actions.mojom-webui.js';
import * as AcceleratorKeysTypes from '../mojom-webui/accelerator_keys.mojom-webui.js';
import * as ExtendedFkeysModifierTypes from '../mojom-webui/extended_fkeys_modifier.mojom-webui.js';
import * as InputDeviceSettingsTypes from '../mojom-webui/input_device_settings.mojom-webui.js';
import * as InputDeviceSettingsProviderTypes from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import * as ModifierKeyTypes from '../mojom-webui/modifier_key.mojom-webui.js';
import * as ShortcutInputProviderTypes from '../mojom-webui/shortcut_input_provider.mojom-webui.js';
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
export type Mouse = Omit<InputDeviceSettingsTypes.Mouse, 'settings'>&{
  settings: MouseSettings,
};
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
  settings: GraphicsTabletSettings;
}

export interface GraphicsTabletSettings {
  tabletButtonRemappings: ButtonRemapping[];
  penButtonRemappings: ButtonRemapping[];
}

export type TouchpadSettings = InputDeviceSettingsTypes.TouchpadSettings;
export type MouseSettings =
    Omit<InputDeviceSettingsTypes.MouseSettings, 'buttonRemappings'>&{
      buttonRemappings: ButtonRemapping[],
    };
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
export type ButtonRemapping =
    Omit<InputDeviceSettingsTypes.ButtonRemapping, 'remappingAction'>&{
      remappingAction?: RemappingAction,
    };

export type RemappingAction =
    Omit<InputDeviceSettingsTypes.RemappingAction, 'keyEvent'>&{
      keyEvent?: KeyEvent,
    };

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
