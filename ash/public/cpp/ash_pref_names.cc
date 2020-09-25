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
// A pref that identifies which kind of features are enabled for the Web Kiosk
// session.
const char kAccessibilityVirtualKeyboardFeatures[] =
    "settings.a11y.virtual_keyboard_features";
// A boolean pref which determines whether the mono audio output is enabled for
// accessibility.
const char kAccessibilityMonoAudioEnabled[] = "settings.a11y.mono_audio";
// A boolean pref which determines whether autoclick is enabled.
const char kAccessibilityAutoclickEnabled[] = "settings.a11y.autoclick";
// A boolean pref which determines whether the accessibility shortcuts are
// enabled or not.
const char kAccessibilityShortcutsEnabled[] = "settings.a11y.shortcuts_enabled";
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
// Whether Autoclick should stabilize the cursor movement before a click occurs
// or not.
const char kAccessibilityAutoclickStabilizePosition[] =
    "settings.a11y.autoclick_stabilize_position";
// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
const char kAccessibilityAutoclickMovementThreshold[] =
    "settings.a11y.autoclick_movement_threshold";
// The Autoclick menu position on the screen, an AutoclickMenuPosition.
const char kAccessibilityAutoclickMenuPosition[] =
    "settings.a11y.autoclick_menu_position";
// A boolean pref which determines whether caret highlighting is enabled.
const char kAccessibilityCaretHighlightEnabled[] =
    "settings.a11y.caret_highlight";
// A boolean pref which determines whether cursor highlighting is enabled.
const char kAccessibilityCursorHighlightEnabled[] =
    "settings.a11y.cursor_highlight";
// A boolean pref which determines whether custom cursor color is enabled.
const char kAccessibilityCursorColorEnabled[] =
    "settings.a11y.cursor_color_enabled";
// An integer pref which determines the custom cursor color.
const char kAccessibilityCursorColor[] = "settings.a11y.cursor_color";
// A boolean pref which determines whether floating accessibility menu is
// enabled.
const char kAccessibilityFloatingMenuEnabled[] = "settings.a11y.floating_menu";
// Floating a11y menu position, a FloatingMenuPosition;
const char kAccessibilityFloatingMenuPosition[] =
    "settings.a11y.floating_menu_position";
// A boolean pref which determines whether focus highlighting is enabled.
const char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
// A boolean pref which determines whether select-to-speak is enabled.
const char kAccessibilitySelectToSpeakEnabled[] =
    "settings.a11y.select_to_speak";
// A boolean pref which determines whether Switch Access is enabled.
const char kAccessibilitySwitchAccessEnabled[] =
    "settings.a11y.switch_access.enabled";
// A pref that stores the key code for the "select" action.
const char kAccessibilitySwitchAccessSelectKeyCodes[] =
    "settings.a11y.switch_access.select.key_codes";
// A pref that stores the setting value for the "select" action.
const char kAccessibilitySwitchAccessSelectSetting[] =
    "settings.a11y.switch_access.select.setting";
// A pref that stores the key code for the "next" action.
const char kAccessibilitySwitchAccessNextKeyCodes[] =
    "settings.a11y.switch_access.next.key_codes";
// A pref that stores the setting value for the "next" action.
const char kAccessibilitySwitchAccessNextSetting[] =
    "settings.a11y.switch_access.next.setting";
// A pref that stores the key code for the "previous" action.
const char kAccessibilitySwitchAccessPreviousKeyCodes[] =
    "settings.a11y.switch_access.previous.key_codes";
// A pref that stores the setting value for the "previous" action.
const char kAccessibilitySwitchAccessPreviousSetting[] =
    "settings.a11y.switch_access.previous.setting";
// A boolean pref which determines whether auto-scanning is enabled within
// Switch Access.
const char kAccessibilitySwitchAccessAutoScanEnabled[] =
    "settings.a11y.switch_access.auto_scan.enabled";
// An integer pref which determines time delay in ms before automatically
// scanning forward (when auto-scan is enabled).
const char kAccessibilitySwitchAccessAutoScanSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.speed_ms";
// An integer pref which determines time delay in ms before automatically
// scanning forward while navigating the keyboard (when auto-scan is
// enabled).
const char kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.keyboard.speed_ms";
// A boolean pref which, if set, indicates that shelf navigation buttons (home,
// back and overview button) should be shown in tablet mode. Note that shelf
// buttons might be shown even if the pref value is false - for example, if
// spoken feedback, autoclick or switch access are enabled.
const char kAccessibilityTabletModeShelfNavigationButtonsEnabled[] =
    "settings.a11y.tablet_mode_shelf_nav_buttons_enabled";
// A boolean pref which determines whether dictation is enabled.
const char kAccessibilityDictationEnabled[] = "settings.a11y.dictation";
// A boolean pref which determines whether the accessibility menu shows
// regardless of the state of a11y features.
const char kShouldAlwaysShowAccessibilityMenu[] = "settings.a11y.enable_menu";

// A dictionary storing the number of times and most recent time all contextual
// tooltips have been shown.
const char kContextualTooltips[] = "settings.contextual_tooltip.shown_info";

// A list containing the stored virtual desks names in the same order of the
// desks in the overview desks bar. This list will be used to restore the desks,
// their order, and their names for the primary user on first signin. If a desk
// hasn't been renamed by the user (i.e. it uses one of the default
// automatically-assigned desk names such as "Desk 1", "Desk 2", ... etc.), its
// name will appear in this list as an empty string. The desk names are stored
// as UTF8 strings.
const char kDesksNamesList[] = "ash.desks.desks_names_list";

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
// Renamed 10/2019 to force reset the pref to false.
const char kDisplayRotationAcceleratorDialogHasBeenAccepted2[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted2";

// A dictionary pref that stores the mixed mirror mode parameters.
const char kDisplayMixedMirrorModeParams[] =
    "settings.display.mixed_mirror_mode_param";
// Power state of the current displays from the last run.
const char kDisplayPowerState[] = "settings.display.power_state";
// A dictionary pref that stores per display preferences.
const char kDisplayProperties[] = "settings.display.properties";
// Boolean controlling whether privacy screen is enabled.
const char kDisplayPrivacyScreenEnabled[] =
    "settings.display.privacy_screen_enabled";
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

// A boolean pref storing whether the gesture education notification has ever
// been shown to the user, which we use to stop showing it again.
const char kGestureEducationNotificationShown[] =
    "ash.gesture_education.notification_shown";

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

// Boolean pref indicating whether the privacy warning of the managed-guest
// session on both; the login screen and inside the auto-launched session,
// should be displayed or not.
const char kManagedGuestSessionPrivacyWarningsEnabled[] =
    "managed_session.privacy_warning_enabled";

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

// A boolean pref storing the enabled status of the ambient color feature.
const char kAmbientColorEnabled[] = "ash.ambient_color.enabled";

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

// Double prefs storing the most recent valid geoposition, which is only used
// when the device lacks connectivity and we're unable to retrieve a valid
// geoposition to calculate the sunset / sunrise times.
const char kNightLightCachedLatitude[] = "ash.night_light.cached_latitude";
const char kNightLightCachedLongitude[] = "ash.night_light.cached_longitude";

// A boolean pref storing whether the AutoNightLight notification has ever been
// dismissed by the user, which we use to stop showing it again.
const char kAutoNightLightNotificationDismissed[] =
    "ash.auto_night_light.notification_dismissed";

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

// Boolean controlling whether a shorter suspend delay should be used after the
// user forces the display off by pressing the power button. Provided to allow
// policy to control this behavior.
const char kPowerFastSuspendWhenBacklightsForcedOff[] =
    "power.fast_suspend_when_backlights_forced_off";

// Boolean controlling whether smart dim model is enabled.
const char kPowerSmartDimEnabled[] = "power.smart_dim_enabled";

// Boolean controlling whether ALS logging is enabled.
const char kPowerAlsLoggingEnabled[] = "power.als_logging_enabled";

// Boolean controlling whether the settings is enabled. This pref is intended to
// be set only by policy not by user.
const char kOsSettingsEnabled[] = "os_settings_enabled";

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

// Pref storing the number of sessions in which Assistant onboarding was shown.
const char kAssistantNumSessionsWhereOnboardingShown[] =
    "ash.assistant.num_sessions_where_onboarding_shown";

// Pref storing the time of the last Assistant interaction.
const char kAssistantTimeOfLastInteraction[] =
    "ash.assistant.time_of_last_interaction";

// Whether the user is allowed to disconnect and configure VPN connections.
const char kVpnConfigAllowed[] = "vpn_config_allowed";

// A boolean pref that indicates whether power peak shift is enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftEnabled[] = "ash.power.peak_shift_enabled";

// An integer pref that specifies the power peak shift battery threshold in
// percent.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftBatteryThreshold[] =
    "ash.power.peak_shift_battery_threshold";

// A dictionary pref that specifies the power peak shift day configs.
// For details see "DevicePowerPeakShiftDayConfig" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftDayConfig[] = "ash.power.peak_shift_day_config";

// A boolean pref that indicates whether boot on AC is enabled.
const char kBootOnAcEnabled[] = "ash.power.boot_on_ac_enabled";

// A boolean pref that indicates whether advanced battery charge mode is
// enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kAdvancedBatteryChargeModeEnabled[] =
    "ash.power.advanced_battery_charge_mode_enabled";

// A dictionary pref that specifies the advanced battery charge mode day config.
// For details see "DeviceAdvancedBatteryChargeModeDayConfig" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kAdvancedBatteryChargeModeDayConfig[] =
    "ash.power.advanced_battery_charge_mode_day_config";

// An integer pref that specifies the battery charge mode.
// For details see "DeviceBatteryChargeMode" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeMode[] = "ash.power.battery_charge_mode";

// An integer pref that specifies the battery charge custom start charging in
// percent.
// For details see "DeviceBatteryChargeCustomStartCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeCustomStartCharging[] =
    "ash.power.battery_charge_custom_start_charging";

// An integer pref that specifies the battery charge custom stop charging in
// percent.
// For details see "DeviceBatteryChargeCustomStopCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeCustomStopCharging[] =
    "ash.power.battery_charge_custom_stop_charging";

// A boolean pref that indicates whether USB power share is enabled.
// For details see "DeviceUsbPowerShareEnabled" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kUsbPowerShareEnabled[] = "ash.power.usb_power_share_enabled";

// An integer pref that specifies how many times the Assistant privacy info has
// been shown in Launcher. This value will increment by one every time when
// Launcher changes state from Peeking to Half or FullscreenSearch up to a
// predefined threshold, e.g. six times. If the info has been shown for more
// than the threshold, do not show the privacy info any more.
const char kAssistantPrivacyInfoShownInLauncher[] =
    "ash.launcher.assistant_privacy_info_shown";

// A boolean pref that indicates whether the Assistant privacy info may be
// displayed to user. A false value indicates that the info can be displayed if
// the value of |kAssistantPrivacyInfoShownInLauncher| is smaller than the
// predefined threshold. A true value implies that the user has dismissed the
// info view, and do not show the privacy info any more.
const char kAssistantPrivacyInfoDismissedInLauncher[] =
    "ash.launcher.assistant_privacy_info_dismissed";

// An integer pref that specifies how many times the Suggested Content privacy
// info has been shown in Launcher. This value will increment by one every time
// when Launcher changes state from Peeking to Half or FullscreenSearch up to a
// predefined threshold, e.g. six times. If the info has been shown for more
// than the threshold, do not show the privacy info any more.
const char kSuggestedContentInfoShownInLauncher[] =
    "ash.launcher.suggested_content_info_shown";

// A boolean pref that indicates whether the Suggested Content privacy info may
// be displayed to user. A false value indicates that the info can be displayed
// if the value of |kSuggestedContentInfoShownInLauncher| is smaller than the
// predefined threshold. A true value implies that the user has dismissed the
// info view, and do not show the privacy info any more.
const char kSuggestedContentInfoDismissedInLauncher[] =
    "ash.launcher.suggested_content_info_dismissed";

// A boolean pref that indicates whether lock screen media controls are enabled.
// Controlled by user policy.
const char kLockScreenMediaControlsEnabled[] =
    "ash.lock_screen_media_controls_enabled";

// Boolean pref which determines whether key repeat is enabled.
const char kXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";

// Integer pref which determines key repeat delay (in ms).
const char kXkbAutoRepeatDelay[] = "settings.language.xkb_auto_repeat_delay_r2";

// Integer pref which determines key repeat interval (in ms).
const char kXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes were added to the three prefs above when we changed the
// preferences to not be user-configurable or sync with the cloud. The prefs are
// now user-configurable and syncable again, but we don't want to overwrite the
// current values with the old synced values, so we continue to use this suffix.

// A boolean pref which is true if touchpad reverse scroll is enabled.
const char kNaturalScroll[] = "settings.touchpad.natural_scroll";
// A boolean pref which is true if mouse reverse scroll is enabled.
const char kMouseReverseScroll[] = "settings.mouse.reverse_scroll";

// A dictionary storing the number of times and most recent time the multipaste
// contextual nudge was shown.
const char kMultipasteNudges[] = "ash.clipboard.multipaste_nudges";

// A boolean pref that indicates whether dark mode is enabled.
const char kDarkModeEnabled[] = "cros.system.dark_mode_enabled";
// A boolean pref that indicates whether the color mode is themed. If true, the
// background color will be calculated based on extracted wallpaper color.
const char kColorModeThemed[] = "cros.system.color_mode_themed";

// A boolean pref that indicates whether app badging is shown in launcher and
// shelf.
const char kAppNotificationBadgingEnabled[] =
    "ash.app_notification_badging_enabled";

// An integer pref that counts how many times the reverse gesture notification
// shows.
const char kReverseGestureNotificationCount[] =
    "ash.wm.reverse_gesture_notification_count";

// An integer pref that indicates whether global media controls is pinned to
// shelf or it's unset and need to be determined by screen size during runtime.
const char kGlobalMediaControlsPinned[] =
    "ash.system.global_media_controls_pinned";

// NOTE: New prefs should start with the "ash." prefix. Existing prefs moved
// into this file should not be renamed, since they may be synced.

}  // namespace prefs

}  // namespace ash
