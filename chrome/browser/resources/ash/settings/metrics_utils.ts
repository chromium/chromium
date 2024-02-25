/* Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Utility functions for settings metrics
 */

import {assert} from 'chrome://resources/js/assert.js';

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

const PrefType = chrome.settingsPrivate.PrefType;

// Sorted and grouped by page alphabetically.
const PREF_TO_SETTING_MAP: Record<string, SettingAndType> = {
  // Crostini settings
  'crostini.mic_allowed': {
    setting: Setting.kCrostiniMicAccess,
    type: PrefType.BOOLEAN,
  },

  // Date and time settings
  'settings.clock.use_24hour_clock': {
    setting: Setting.k24HourClock,
    type: PrefType.BOOLEAN,
  },
  'generated.resolve_timezone_by_geolocation_on_off': {
    setting: Setting.kChangeTimeZone,
    type: PrefType.BOOLEAN,
  },

  // Language and input settings
  'browser.enable_spellchecking': {
    setting: Setting.kSpellCheckOnOff,
    type: PrefType.BOOLEAN,
  },
  'translate.enabled': {
    setting: Setting.kOfferTranslation,
    type: PrefType.BOOLEAN,
  },

  // Multitasking settings
  'ash.snap_window_suggestions.enabled': {
    setting: Setting.kSnapWindowSuggestions,
    type: PrefType.BOOLEAN,
  },

  // Startup settings
  'settings.restore_apps_and_pages': {
    setting: Setting.kRestoreAppsAndPages,
    type: PrefType.NUMBER,
  },

  // device_page/audio.ts
  'ash.low_battery_sound.enabled': {
    setting: Setting.kLowBatterySound,
    type: PrefType.BOOLEAN,
  },
  'ash.charging_sounds.enabled': {
    setting: Setting.kChargingSounds,
    type: PrefType.BOOLEAN,
  },
  // device_page/keyboard.ts
  'settings.language.send_function_keys': {
    setting: Setting.kKeyboardFunctionKeys,
    type: PrefType.BOOLEAN,
  },
  // device_page/pointers.ts
  'settings.touchpad.sensitivity2': {
    setting: Setting.kTouchpadSpeed,
    type: PrefType.NUMBER,
  },
  // os_a11y_page/display_and_magnification_subpage.ts
  'settings.a11y.screen_magnifier_focus_following': {
    setting: Setting.kFullscreenMagnifierFocusFollowing,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_mouse_following_mode': {
    setting: Setting.kFullscreenMagnifierMouseFollowingMode,
    type: PrefType.NUMBER,
  },
  'settings.a11y.color_filtering.enabled': {
    setting: Setting.kColorCorrectionEnabled,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.color_filtering.color_vision_deficiency_type': {
    setting: Setting.kColorCorrectionFilterType,
    type: PrefType.NUMBER,
  },
  'settings.a11y.color_filtering.color_vision_correction_amount': {
    setting: Setting.kColorCorrectionFilterAmount,
    type: PrefType.NUMBER,
  },
  // os_privacy_page/os_privacy_page.js
  'cros.device.peripheral_data_access_enabled': {
    setting: Setting.kPeripheralDataAccessProtection,
    type: PrefType.BOOLEAN,
  },
  'cros.reven.enable_hw_data_usage': {
    setting: Setting.kRevenEnableHwDataUsage,
    type: PrefType.BOOLEAN,
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
    case PrefType.BOOLEAN:
      assert(typeof prefValue === 'boolean');
      return {setting, value: {boolValue: prefValue}};

    case PrefType.NUMBER:
      assert(typeof prefValue === 'number');
      return {setting, value: {intValue: prefValue}};

    // pref to setting metric not implemented.
    default:
      return null;
  }
}
