// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"

namespace ash {

namespace prefs {

// A boolean pref which determines whether the large cursor feature is enabled.
const char kAccessibilityLargeCursorEnabled[] =
    "settings.a11y.large_cursor_enabled";
// An integer pref that specifies the size of large cursor for accessibility.
const char kAccessibilityLargeCursorDipSize[] =
    "settings.a11y.large_cursor_dip_size";
// A boolean pref which determines whether the sticky keys feature is enabled.
const char kAccessibilityStickyKeysEnabled[] =
    "settings.a11y.sticky_keys_enabled";
// A boolean pref which determines whether spoken feedback is enabled.
const char kAccessibilitySpokenFeedbackEnabled[] = "settings.accessibility";
// A boolean pref which determines whether high contrast is enabled.
const char kAccessibilityHighContrastEnabled[] =
    "settings.a11y.high_contrast_enabled";
// A boolean pref which determines whether screen magnifier is enabled.
// NOTE: We previously had prefs named settings.a11y.screen_magnifier_type and
// settings.a11y.screen_magnifier_type2, but we only shipped one type (full).
// See http://crbug.com/170850 for history.
const char kAccessibilityScreenMagnifierEnabled[] =
    "settings.a11y.screen_magnifier";
// A boolean pref which determines whether screen magnifier should center
// the text input focus.
const char kAccessibilityScreenMagnifierCenterFocus[] =
    "settings.a11y.screen_magnifier_center_focus";
// A double pref which determines a zooming scale of the screen magnifier.
const char kAccessibilityScreenMagnifierScale[] =
    "settings.a11y.screen_magnifier_scale";
// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
const char kAccessibilityVirtualKeyboardEnabled[] =
    "settings.a11y.virtual_keyboard";
// A boolean pref which determines whether the mono audio output is enabled for
// accessibility.
const char kAccessibilityMonoAudioEnabled[] = "settings.a11y.mono_audio";
// A boolean pref which determines whether autoclick is enabled.
const char kAccessibilityAutoclickEnabled[] = "settings.a11y.autoclick";
// An integer pref which determines time in ms between when the mouse cursor
// stops and when an autoclick event is triggered.
const char kAccessibilityAutoclickDelayMs[] =
    "settings.a11y.autoclick_delay_ms";
// An integer pref which determines the event type for an autoclick event. This
// maps to mojom::AccessibilityController::AutoclickEventType.
const char kAccessibilityAutoclickEventType[] =
    "settings.a11y.autoclick_event_type";
// Whether Autoclick should immediately return to left click after performing
// another event type action, or whether it should stay as the other event type.
const char kAccessibilityAutoclickRevertToLeftClick[] =
    "settings.a11y.autoclick_revert_to_left_click";
// A boolean pref which determines whether caret highlighting is enabled.
const char kAccessibilityCaretHighlightEnabled[] =
    "settings.a11y.caret_highlight";
// A boolean pref which determines whether cursor highlighting is enabled.
const char kAccessibilityCursorHighlightEnabled[] =
    "settings.a11y.cursor_highlight";
// A boolean pref which determines whether focus highlighting is enabled.
const char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
// A boolean pref which determines whether select-to-speak is enabled.
const char kAccessibilitySelectToSpeakEnabled[] =
    "settings.a11y.select_to_speak";
// A boolean pref which determines whether switch access is enabled.
const char kAccessibilitySwitchAccessEnabled[] = "settings.a11y.switch_access";
// A boolean pref which determines whether dictation is enabled.
const char kAccessibilityDictationEnabled[] = "settings.a11y.dictation";
// A boolean pref which determines whether the accessibility menu shows
// regardless of the state of a11y features.
const char kShouldAlwaysShowAccessibilityMenu[] = "settings.a11y.enable_menu";

// A boolean pref storing the enabled status of the Docked Magnifier feature.
const char kDockedMagnifierEnabled[] = "ash.docked_magnifier.enabled";
// A double pref storing the scale value of the Docked Magnifier feature by
// which the screen is magnified.
const char kDockedMagnifierScale[] = "ash.docked_magnifier.scale";

// A boolean pref which indicates whether the docked magnifier confirmation
// dialog has ever been shown.
const char kDockedMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.docked_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the high contrast magnifier
// confirmation dialog has ever been shown.
const char kHighContrastAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.high_contrast_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the screen magnifier confirmation
// dialog has ever been shown.
const char kScreenMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.screen_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the dictation confirmation dialog has
// ever been shown.
const char kDictationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.dictation_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the display rotation confirmation
// dialog has ever been shown.
const char kDisplayRotationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted";

// A dictionary pref that stores the mixed mirror mode parameters.
const char kDisplayMixedMirrorModeParams[] =
    "settings.display.mixed_mirror_mode_param";
// Power state of the current displays from the last run.
const char kDisplayPowerState[] = "settings.display.power_state";
// A dictionary pref that stores per display preferences.
const char kDisplayProperties[] = "settings.display.properties";
// A dictionary pref that specifies the state of the rotation lock, and the
// display orientation, for the internal display.
const char kDisplayRotationLock[] = "settings.display.rotation_lock";
// A dictionary pref that stores the touch associations for the device.
const char kDisplayTouchAssociations[] = "settings.display.touch_associations";
// A dictionary pref that stores the port mapping for touch devices.
const char kDisplayTouchPortAssociations[] =
    "settings.display.port_associations";
// A list pref that stores the mirror info for each external display.
const char kExternalDisplayMirrorInfo[] =
    "settings.display.external_display_mirror_info";
// A dictionary pref that specifies per-display layout/offset information.
// Its key is the ID of the display and its value is a dictionary for the
// layout/offset information.
const char kSecondaryDisplays[] = "settings.display.secondary_displays";

// A boolean pref which stores whether a stylus has been seen before.
const char kHasSeenStylus[] = "ash.has_seen_stylus";
// A boolean pref which stores whether a the palette warm welcome bubble
// (displayed when a user first uses a stylus) has been shown before.
const char kShownPaletteWelcomeBubble[] = "ash.shown_palette_welcome_bubble";
// A boolean pref that specifies if the stylus tools should be enabled/disabled.
const char kEnableStylusTools[] = "settings.enable_stylus_tools";
// A boolean pref that specifies if the ash palette should be launched after an
// eject input event has been received.
const char kLaunchPaletteOnEjectEvent[] =
    "settings.launch_palette_on_eject_event";

// A string pref storing the type of lock screen notification mode.
// "show" -> show notifications on the lock screen
// "hide" -> hide notifications at all on the lock screen (default)
// "hideSensitive" -> hide sensitive content on the lock screen
// (other values are treated as "hide")
const char kMessageCenterLockScreenMode[] =
    "ash.message_center.lock_screen_mode";

// Value of each options of the lock screen notification settings. They are
// used the pref of ash::prefs::kMessageCenterLockScreenMode.
const char kMessageCenterLockScreenModeShow[] = "show";
const char kMessageCenterLockScreenModeHide[] = "hide";
const char kMessageCenterLockScreenModeHideSensitive[] = "hideSensitive";

// A boolean pref storing the enabled status of the NightLight feature.
const char kNightLightEnabled[] = "ash.night_light.enabled";

// A double pref storing the screen color temperature set by the NightLight
// feature. The expected values are in the range of 0.0 (least warm) and 1.0
// (most warm).
const char kNightLightTemperature[] = "ash.night_light.color_temperature";

// An integer pref storing the type of automatic scheduling of turning on and
// off the NightLight feature. Valid values are:
// 0 -> NightLight is never turned on or off automatically.
// 1 -> NightLight is turned on and off at the sunset and sunrise times
// respectively.
// 2 -> NightLight schedule times are explicitly set by the user.
//
// See ash::NightLightController::ScheduleType.
const char kNightLightScheduleType[] = "ash.night_light.schedule_type";

// Integer prefs storing the start and end times of the automatic schedule at
// which NightLight turns on and off respectively when the schedule type is set
// to a custom schedule. The times are represented as the number of minutes from
// 00:00 (12:00 AM) regardless of the date or the timezone.
// See ash::TimeOfDayTime.
const char kNightLightCustomStartTime[] = "ash.night_light.custom_start_time";
const char kNightLightCustomEndTime[] = "ash.night_light.custom_end_time";

// Whether the Chrome OS lock screen is allowed.
const char kAllowScreenLock[] = "allow_screen_lock";

// A boolean pref that turns on automatic screen locking.
const char kEnableAutoScreenLock[] = "settings.enable_screen_lock";

// Screen brightness percent values to be used when running on AC power.
// Specified by the policy.
const char kPowerAcScreenBrightnessPercent[] =
    "power.ac_screen_brightness_percent";

// Inactivity time in milliseconds while the system is on AC power before
// the screen should be dimmed, turned off, or locked, before an
// IdleActionImminent D-Bus signal should be sent, or before
// kPowerAcIdleAction should be performed.  0 disables the delay (N/A for
// kPowerAcIdleDelayMs).
const char kPowerAcScreenDimDelayMs[] = "power.ac_screen_dim_delay_ms";
const char kPowerAcScreenOffDelayMs[] = "power.ac_screen_off_delay_ms";
const char kPowerAcScreenLockDelayMs[] = "power.ac_screen_lock_delay_ms";
const char kPowerAcIdleWarningDelayMs[] = "power.ac_idle_warning_delay_ms";

// Screen brightness percent values to be used when running on battery power.
// Specified by the policy.
const char kPowerBatteryScreenBrightnessPercent[] =
    "power.battery_screen_brightness_percent";

// Similar delays while the system is on battery power.
const char kPowerBatteryScreenDimDelayMs[] =
    "power.battery_screen_dim_delay_ms";
const char kPowerBatteryScreenOffDelayMs[] =
    "power.battery_screen_off_delay_ms";
const char kPowerBatteryScreenLockDelayMs[] =
    "power.battery_screen_lock_delay_ms";
const char kPowerBatteryIdleWarningDelayMs[] =
    "power.battery_idle_warning_delay_ms";
const char kPowerBatteryIdleDelayMs[] = "power.battery_idle_delay_ms";
const char kPowerAcIdleDelayMs[] = "power.ac_idle_delay_ms";

// Inactivity delays used to dim the screen or turn it off while the screen is
// locked.
const char kPowerLockScreenDimDelayMs[] = "power.lock_screen_dim_delay_ms";
const char kPowerLockScreenOffDelayMs[] = "power.lock_screen_off_delay_ms";

// Action that should be performed when the idle delay is reached while the
// system is on AC power or battery power.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerAcIdleAction[] = "power.ac_idle_action";
const char kPowerBatteryIdleAction[] = "power.battery_idle_action";

// Action that should be performed when the lid is closed.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerLidClosedAction[] = "power.lid_closed_action";

// Should audio and video activity be used to disable the above delays?
const char kPowerUseAudioActivity[] = "power.use_audio_activity";
const char kPowerUseVideoActivity[] = "power.use_video_activity";

// Should extensions, ARC apps, and other code within Chrome be able to override
// system power management (preventing automatic actions like sleeping, locking,
// or screen dimming)?
const char kPowerAllowWakeLocks[] = "power.allow_wake_locks";

// Should extensions, ARC apps, and other code within Chrome be able to override
// display-related power management? (Disallowing wake locks in general takes
// precedence over this.)
const char kPowerAllowScreenWakeLocks[] = "power.allow_screen_wake_locks";

// Amount by which the screen-dim delay should be scaled while the system
// is in presentation mode. Values are limited to a minimum of 1.0.
const char kPowerPresentationScreenDimDelayFactor[] =
    "power.presentation_screen_dim_delay_factor";

// Amount by which the screen-dim delay should be scaled when user activity is
// observed while the screen is dimmed or soon after the screen has been turned
// off.  Values are limited to a minimum of 1.0.
const char kPowerUserActivityScreenDimDelayFactor[] =
    "power.user_activity_screen_dim_delay_factor";

// Whether the power management delays should start running only after the first
// user activity has been observed in a session.
const char kPowerWaitForInitialUserActivity[] =
    "power.wait_for_initial_user_activity";

// Boolean controlling whether the panel backlight should be forced to a
// nonzero level when user activity is observed.
const char kPowerForceNonzeroBrightnessForUserActivity[] =
    "power.force_nonzero_brightness_for_user_activity";

// Boolean controlling whether smart dim model is enabled.
const char kPowerSmartDimEnabled[] = "power.smart_dim_enabled";

// |kShelfAlignment| and |kShelfAutoHideBehavior| have a local variant. The
// local variant is not synced and is used if set. If the local variant is not
// set its value is set from the synced value (once prefs have been
// synced). This gives a per-machine setting that is initialized from the last
// set value.
// These values are default on the machine but can be overridden by per-display
// values in kShelfPreferences (unless overridden by managed policy).
// String value corresponding to ash::ShelfAlignment (e.g. "Bottom").
const char kShelfAlignment[] = "shelf_alignment";
const char kShelfAlignmentLocal[] = "shelf_alignment_local";
// String value corresponding to ash::ShelfAutoHideBehavior (e.g. "Never").
const char kShelfAutoHideBehavior[] = "auto_hide_behavior";
const char kShelfAutoHideBehaviorLocal[] = "auto_hide_behavior_local";
// Dictionary value that holds per-display preference of shelf alignment and
// auto-hide behavior. Key of the dictionary is the id of the display, and
// its value is a dictionary whose keys are kShelfAlignment and
// kShelfAutoHideBehavior.
const char kShelfPreferences[] = "shelf_preferences";

// Boolean pref indicating whether to show a logout button in the system tray.
const char kShowLogoutButtonInTray[] = "show_logout_button_in_tray";

// Integer pref indicating the length of time in milliseconds for which a
// confirmation dialog should be shown when the user presses the logout button.
// A value of 0 indicates that logout should happen immediately, without showing
// a confirmation dialog.
const char kLogoutDialogDurationMs[] = "logout_dialog_duration_ms";

// A dictionary pref that maps usernames to wallpaper info.
const char kUserWallpaperInfo[] = "user_wallpaper_info";

// A dictionary pref that maps wallpaper file paths to their prominent colors.
const char kWallpaperColors[] = "ash.wallpaper.prominent_colors";

// Boolean pref indicating whether a user has enabled the bluetooth adapter.
const char kUserBluetoothAdapterEnabled[] =
    "ash.user.bluetooth.adapter_enabled";

// Boolean pref indicating system-wide setting for bluetooth adapter power.
const char kSystemBluetoothAdapterEnabled[] =
    "ash.system.bluetooth.adapter_enabled";

// A boolean pref which determines whether tap-dragging is enabled.
const char kTapDraggingEnabled[] = "settings.touchpad.enable_tap_dragging";

// Boolean prefs for the status of the touchscreen and the touchpad.
const char kTouchpadEnabled[] = "events.touch_pad.enabled";
const char kTouchscreenEnabled[] = "events.touch_screen.enabled";

// String pref storing the salt for the pin quick unlock mechanism.
const char kQuickUnlockPinSalt[] = "quick_unlock.pin.salt";

// Dictionary prefs in local state that keeps information about detachable
// bases - for exmaple the last used base per user.
const char kDetachableBaseDevices[] = "ash.detachable_base.devices";

// NOTE: New prefs should start with the "ash." prefix. Existing prefs moved
// into this file should not be renamed, since they may be synced.

}  // namespace prefs

}  // namespace ash
