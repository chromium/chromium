/* Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Utility functions for settings metrics
 */

import {assert} from 'chrome://resources/js/assert_ts.js';

import {Setting} from './mojom-webui/setting.mojom-webui.js';
import {SettingChangeValue} from './mojom-webui/user_action_recorder.mojom-webui.js';

interface SettingMetric {
  setting: Setting;
  value: SettingChangeValue;
}

interface SettingAndType {
  setting: Setting;
  type: chrome.settingsPrivate.PrefType;
}

// Sorted and grouped by page alphabetically.
const PREF_TO_SETTING_MAP: Record<string, SettingAndType> = {
  // device_page/audio.ts
  'ash.low_battery_sound.enabled': {
    setting: Setting.kLowBatterySound,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
  'ash.charging_sounds.enabled': {
    setting: Setting.kChargingSounds,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
  // device_page/keyboard.ts
  'settings.language.send_function_keys': {
    setting: Setting.kKeyboardFunctionKeys,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
  // device_page/pointers.ts
  'settings.touchpad.sensitivity2': {
    setting: Setting.kTouchpadSpeed,
    type: chrome.settingsPrivate.PrefType.NUMBER,
  },
  // os_a11y_page/display_and_magnification_subpage.ts
  'settings.a11y.screen_magnifier_focus_following': {
    setting: Setting.kFullscreenMagnifierFocusFollowing,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_mouse_following_mode': {
    setting: Setting.kFullscreenMagnifierMouseFollowingMode,
    type: chrome.settingsPrivate.PrefType.NUMBER,
  },
  'settings.a11y.color_filtering.enabled': {
    setting: Setting.kColorCorrectionEnabled,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
  'settings.a11y.color_filtering.color_vision_deficiency_type': {
    setting: Setting.kColorCorrectionFilterType,
    type: chrome.settingsPrivate.PrefType.NUMBER,
  },
  'settings.a11y.color_filtering.color_vision_correction_amount': {
    setting: Setting.kColorCorrectionFilterAmount,
    type: chrome.settingsPrivate.PrefType.NUMBER,
  },
  // os_privacy_page/os_privacy_page.js
  'cros.device.peripheral_data_access_enabled': {
    setting: Setting.kPeripheralDataAccessProtection,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
  },
};

// Converts a given settings pref to a pair of setting ID and setting change
// value. Used to record metrics about changes to pref-based settings.
export function convertPrefToSettingMetric(
    prefKey: string, prefValue: unknown): SettingMetric|null {
  const settingAndType = PREF_TO_SETTING_MAP[prefKey];
  if (!settingAndType) {
    // Pref to setting metric not implemented.
    return null;
  }

  const {type, setting} = settingAndType;
  switch (type) {
    case chrome.settingsPrivate.PrefType.BOOLEAN:
      assert(typeof prefValue === 'boolean');
      return {setting, value: {boolValue: prefValue}};

    case chrome.settingsPrivate.PrefType.NUMBER:
      assert(typeof prefValue === 'number');
      return {setting, value: {intValue: prefValue}};

    // pref to setting metric not implemented.
    default:
      return null;
  }
}
