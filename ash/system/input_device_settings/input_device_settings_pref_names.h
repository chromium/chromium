// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_

namespace ash::prefs {
// Prefs which contain lists of observed devices for a few milestones before
// per-device settings are enabled.
inline constexpr char kKeyboardObservedDevicesPref[] =
    "settings.keyboard.observed_devices";
inline constexpr char kMouseObservedDevicesPref[] =
    "settings.mouse.observed_devices";
inline constexpr char kPointingStickObservedDevicesPref[] =
    "settings.pointing_stick.observed_devices";
inline constexpr char kTouchpadObservedDevicesPref[] =
    "settings.touchpad.observed_devices";

// Prefs which contain login screen settings for connected internal devices.
inline constexpr char kKeyboardLoginScreenInternalSettingsPref[] =
    "settings.keyboard.internal";
inline constexpr char kMouseLoginScreenInternalSettingsPref[] =
    "settings.mouse.internal";
inline constexpr char kPointingStickLoginScreenInternalSettingsPref[] =
    "settings.pointing_stick.internal";
inline constexpr char kTouchpadLoginScreenInternalSettingsPref[] =
    "settings.touchpad.internal";

// Prefs which contain seen peripheral devices for notification use.
inline constexpr char kPeripheralNotificationMiceSeen[] =
    "settings.mouse.peripheral_notification_seen";
inline constexpr char kPeripheralNotificationGraphicsTabletsSeen[] =
    "settings.graphics_tablet.peripheral_notification_seen";

// Prefs which contain peripheral devices that have seen the enhanced
// "Welcome Experience" notification.
inline constexpr char kWelcomeExperienceNotificationSeen[] =
    "settings.device.welcome_experience_notification_seen";

// Prefs which contain login screen settings for connected external devices.
inline constexpr char kKeyboardLoginScreenExternalSettingsPref[] =
    "settings.keyboard.external";
inline constexpr char kMouseLoginScreenExternalSettingsPref[] =
    "settings.mouse.external";
inline constexpr char kPointingStickLoginScreenExternalSettingsPref[] =
    "settings.pointing_stick.external";
inline constexpr char kTouchpadLoginScreenExternalSettingsPref[] =
    "settings.touchpad.external";

// Prefs which contain login screen button remapping list for connected graphics
// tablet devices.
inline constexpr char
    kGraphicsTabletLoginScreenTabletButtonRemappingListPref[] =
        "settings.graphics_tablet.tablet_button_remappings";
inline constexpr char kGraphicsTabletLoginScreenPenButtonRemappingListPref[] =
    "settings.graphics_tablet.pen_button_remappings";

// Prefs which contain login screen button remapping list for connected external
// mice devices.
inline constexpr char kMouseLoginScreenButtonRemappingListPref[] =
    "settings.mouse.external.button_remappings";

// Prefs which contain dictionaries of settings for each connected device.
inline constexpr char kKeyboardDeviceSettingsDictPref[] =
    "settings.keyboard.devices";
inline constexpr char kMouseDeviceSettingsDictPref[] = "settings.mouse.devices";
inline constexpr char kPointingStickDeviceSettingsDictPref[] =
    "settings.pointing_stick.devices";
inline constexpr char kTouchpadDeviceSettingsDictPref[] =
    "settings.touchpad.devices";

// Dictionary pref containing the internal keyboard's settings.
inline constexpr char kKeyboardInternalSettings[] =
    "settings.keyboard.internal";

// Pref which contains a list of previously seen imposter keyboards that we know
// to be valid (ie false positives).
inline constexpr char kKeyboardDeviceImpostersListPref[] =
    "settings.keyboard.imposter_false_positives";
// Pref which contains a list of previously seen imposter mice that we know to
// be valid (ie false positives).
inline constexpr char kMouseDeviceImpostersListPref[] =
    "settings.mouse.imposter_false_positives";

// Prefs which contain dictionaries of button remappings for each connected
// device.
inline constexpr char kGraphicsTabletTabletButtonRemappingsDictPref[] =
    "settings.graphics_tablet.tablet_button_remappings";
inline constexpr char kGraphicsTabletPenButtonRemappingsDictPref[] =
    "settings.graphics_tablet.pen_button_remappings";
inline constexpr char kMouseButtonRemappingsDictPref[] =
    "settings.mouse.button_remappings";

// Keyboard settings dictionary keys.
inline constexpr char kKeyboardSettingAutoRepeatDelay[] = "auto_repeat_delay";
inline constexpr char kKeyboardSettingAutoRepeatEnabled[] =
    "auto_repeat_enabled";
inline constexpr char kKeyboardSettingAutoRepeatInterval[] =
    "auto_repeat_interval";
inline constexpr char kKeyboardSettingModifierRemappings[] =
    "modifier_remappings";
inline constexpr char kKeyboardSettingSuppressMetaFKeyRewrites[] =
    "suppress_meta_fkey_rewrites";
inline constexpr char kKeyboardSettingTopRowAreFKeys[] = "top_row_are_fkeys";
inline constexpr char kKeyboardSettingSixPackKeyRemappings[] =
    "six_pack_key_remappings";
inline constexpr char kSixPackKeyPageUp[] = "page_up";
inline constexpr char kSixPackKeyPageDown[] = "page_down";
inline constexpr char kSixPackKeyHome[] = "home";
inline constexpr char kSixPackKeyEnd[] = "end";
inline constexpr char kSixPackKeyDelete[] = "delete";
inline constexpr char kSixPackKeyInsert[] = "insert";
inline constexpr char kKeyboardSettingF11[] = "f11";
inline constexpr char kKeyboardSettingF12[] = "f12";

inline constexpr char kKeyboardUpdateSettingsMetricInfo[] =
    "settings.keyboard.update_settings_info";
inline constexpr char kMouseUpdateSettingsMetricInfo[] =
    "settings.mouse.update_settings_info";
inline constexpr char kTouchpadUpdateSettingsMetricInfo[] =
    "settings.touchpad.update_settings_info";
inline constexpr char kPointingStickUpdateSettingsMetricInfo[] =
    "settings.pointing_stick.update_settings_info";
inline constexpr char kTopRowRemappingNudgeShownCount[] =
    "settings.keyboard.top_row_key_remapping_nudge_shown_count";
inline constexpr char kPageUpRemappingNudgeShownCount[] =
    "settings.keyboard.page_up_key_remapping_nudge_shown_count";
inline constexpr char kPageDownRemappingNudgeShownCount[] =
    "settings.keyboard.page_down_key_remapping_nudge_shown_count";
inline constexpr char kHomeRemappingNudgeShownCount[] =
    "settings.keyboard.home_key_remapping_nudge_shown_count";
inline constexpr char kEndRemappingNudgeShownCount[] =
    "settings.keyboard.end_key_remapping_nudge_shown_count";
inline constexpr char kDeleteRemappingNudgeShownCount[] =
    "settings.keyboard.delete_key_remapping_nudge_shown_count";
inline constexpr char kInsertRemappingNudgeShownCount[] =
    "settings.keyboard.insert_key_remapping_nudge_shown_count";
inline constexpr char kCapsLockRemappingNudgeShownCount[] =
    "settings.keyboard.caps_lock_remapping_nudge_shown_count";
inline constexpr char kTopRowRemappingNudgeLastShown[] =
    "settings.keyboard.top_row_key_remapping_nudge_last_shown";
inline constexpr char kPageUpRemappingNudgeLastShown[] =
    "settings.keyboard.page_up_key_remapping_nudge_last_shown";
inline constexpr char kPageDownRemappingNudgeLastShown[] =
    "settings.keyboard.page_down_key_remapping_nudge_last_shown";
inline constexpr char kHomeRemappingNudgeLastShown[] =
    "settings.keyboard.home_key_remapping_nudge_last_shown";
inline constexpr char kEndRemappingNudgeLastShown[] =
    "settings.keyboard.end_key_remapping_nudge_last_shown";
inline constexpr char kDeleteRemappingNudgeLastShown[] =
    "settings.keyboard.delete_key_remapping_nudge_last_shown";
inline constexpr char kInsertRemappingNudgeLastShown[] =
    "settings.keyboard.insert_key_remapping_nudge_last_shown";
inline constexpr char kCapsLockRemappingNudgeLastShown[] =
    "settings.keyboard.caps_lock_remapping_nudge_last_shown";

// Mouse settings dictionary keys.
inline constexpr char kMouseSettingSwapRight[] = "swap_right";
inline constexpr char kMouseSettingSensitivity[] = "sensitivity";
inline constexpr char kMouseSettingReverseScrolling[] = "reverse_scrolling";
inline constexpr char kMouseSettingAccelerationEnabled[] =
    "acceleration_enabled";
inline constexpr char kMouseSettingScrollSensitivity[] = "scroll_sensitivity";
inline constexpr char kMouseSettingScrollAcceleration[] = "scroll_acceleration";

// Touchpad settings dictionary keys.
inline constexpr char kTouchpadSettingSensitivity[] = "sensitivity";
inline constexpr char kTouchpadSettingReverseScrolling[] = "reverse_scrolling";
inline constexpr char kTouchpadSettingAccelerationEnabled[] =
    "acceleration_enabled";
inline constexpr char kTouchpadSettingScrollSensitivity[] =
    "scroll_sensitivity";
inline constexpr char kTouchpadSettingScrollAcceleration[] =
    "scroll_acceleration";
inline constexpr char kTouchpadSettingTapToClickEnabled[] =
    "tap_to_click_enabled";
inline constexpr char kTouchpadSettingThreeFingerClickEnabled[] =
    "three_finger_click_enabled";
inline constexpr char kTouchpadSettingTapDraggingEnabled[] =
    "tap_dragging_enabled";
inline constexpr char kTouchpadSettingHapticSensitivity[] =
    "haptic_sensitivity";
inline constexpr char kTouchpadSettingHapticEnabled[] = "haptic_enabled";
inline constexpr char kTouchpadSettingSimulateRightClick[] =
    "simulate_right_click";

// Pointing stick settings dictionary keys.
inline constexpr char kPointingStickSettingSensitivity[] = "sensitivity";
inline constexpr char kPointingStickSettingSwapRight[] = "swap_right";
inline constexpr char kPointingStickSettingAcceleration[] = "acceleration";

// Button Remapping dictionary keys.
inline constexpr char kButtonRemappings[] = "button_remappings";
inline constexpr char kButtonRemappingName[] = "name";
inline constexpr char kButtonRemappingCustomizableButton[] =
    "customizable_button";
inline constexpr char kButtonRemappingKeyboardCode[] = "vkey";
inline constexpr char kButtonRemappingAcceleratorAction[] =
    "accelerator_action";
inline constexpr char kButtonRemappingKeyEvent[] = "key_event";
inline constexpr char kButtonRemappingDomCode[] = "dom_code";
inline constexpr char kButtonRemappingDomKey[] = "dom_key";
inline constexpr char kButtonRemappingModifiers[] = "modifiers";
inline constexpr char kButtonRemappingStaticShortcutAction[] =
    "static_shortcut_action";

// Last updated dictionary keys.
inline constexpr char kLastUpdatedKey[] = "last_updated";

// Preference key used to access a dictionary that maps device identifiers
// to their corresponding image URLs.
inline constexpr char kDeviceImagesDictPref[] = "settings.devices.images";

}  // namespace ash::prefs

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
