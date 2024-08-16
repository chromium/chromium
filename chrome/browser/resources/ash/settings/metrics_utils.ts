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

  // Bruschetta settings
  'bruschetta.mic_allowed': {
    setting: Setting.kCrostiniMicAccess,
    type: PrefType.BOOLEAN,
  },

  // Guest settings
  'guest_os.usb_notification_enabled': {
    setting: Setting.kGuestUsbNotification,
    type: PrefType.BOOLEAN,
  },

  'guest_os.usb_persistent_passthrough_enabled': {
    setting: Setting.kGuestUsbPersistentPassthrough,
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
  // os_a11y_page/audio_and_captions_page.ts
  'settings.a11y.flash_notifications_enabled': {
    setting: Setting.kFlashNotifications,
    type: PrefType.BOOLEAN,
  },
  'accessibility.captions.live_caption_enabled': {
    setting: Setting.kLiveCaption,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.mono_audio': {
    setting: Setting.kMonoAudio,
    type: PrefType.BOOLEAN,
  },
  // os_a11y_page/cursor_and_touchpad_page.ts
  'settings.a11y.autoclick': {
    setting: Setting.kAutoClickWhenCursorStops,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.cursor_highlight': {
    setting: Setting.kHighlightCursorWhileMoving,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.cursor_color_enabled': {
    setting: Setting.kEnableCursorColor,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.large_cursor_enabled': {
    setting: Setting.kLargeCursor,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.overscroll_history_navigation': {
    setting: Setting.kOverscrollEnabled,
    type: PrefType.BOOLEAN,
  },
  // os_a11y_page/display_and_magnification_subpage.ts
  'ash.docked_magnifier.enabled': {
    setting: Setting.kDockedMagnifier,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.color_filtering.enabled': {
    setting: Setting.kColorCorrectionEnabled,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.color_filtering.color_vision_correction_amount': {
    setting: Setting.kColorCorrectionFilterAmount,
    type: PrefType.NUMBER,
  },
  'settings.a11y.color_filtering.color_vision_deficiency_type': {
    setting: Setting.kColorCorrectionFilterType,
    type: PrefType.NUMBER,
  },
  'settings.a11y.high_contrast_enabled': {
    setting: Setting.kHighContrastMode,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.reduced_animations.enabled': {
    setting: Setting.kReducedAnimationsEnabled,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier': {
    setting: Setting.kFullscreenMagnifier,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_focus_following': {
    setting: Setting.kFullscreenMagnifierFocusFollowing,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_chromevox_focus_following': {
    setting: Setting.kMagnifierFollowsChromeVox,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_select_to_speak_focus_following': {
    setting: Setting.kAccessibilityMagnifierFollowsSts,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.screen_magnifier_mouse_following_mode': {
    setting: Setting.kFullscreenMagnifierMouseFollowingMode,
    type: PrefType.NUMBER,
  },
  // os_a11y_page/keyboard_and_text_input_page.ts
  'settings.a11y.caret.blink_interval': {
    setting: Setting.kCaretBlinkInterval,
    type: PrefType.NUMBER,
  },
  'settings.a11y.caretbrowsing.enabled': {
    setting: Setting.kCaretBrowsing,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.caret_highlight': {
    setting: Setting.kHighlightTextCaret,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.dictation': {
    setting: Setting.kDictation,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.focus_highlight': {
    setting: Setting.kHighlightKeyboardFocus,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.sticky_keys_enabled': {
    setting: Setting.kStickyKeys,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.switch_access.enabled': {
    setting: Setting.kEnableSwitchAccess,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.virtual_keyboard': {
    setting: Setting.kOnScreenKeyboard,
    type: PrefType.BOOLEAN,
  },
  // os_a11y_page/text_to_speech_subpage.ts
  'settings.a11y.enable_main_node_annotation': {
    setting: Setting.kMainNodeAnnotationsEnabled,
    type: PrefType.BOOLEAN,
  },
  'settings.a11y.select_to_speak': {
    setting: Setting.kSelectToSpeak,
    type: PrefType.BOOLEAN,
  },
  'settings.accessibility': {
    setting: Setting.kChromeVox,
    type: PrefType.BOOLEAN,
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
  // os_search_page/search_and_assistant_settings_card.ts
  'settings.magic_boost_enabled': {
    setting: Setting.kMagicBoostOnOff,
    type: PrefType.BOOLEAN,
  },
  'settings.mahi_enabled': {
    setting: Setting.kMahiOnOff,
    type: PrefType.BOOLEAN,
  },
  'assistive_input.orca_enabled': {
    setting: Setting.kShowOrca,
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
