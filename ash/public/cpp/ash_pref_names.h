// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
#define ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

namespace prefs {

ASH_PUBLIC_EXPORT extern const char kAccessibilityLargeCursorEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityLargeCursorDipSize[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityStickyKeysEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySpokenFeedbackEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityHighContrastEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierCenterFocus[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierScale[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityVirtualKeyboardEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityVirtualKeyboardFeatures[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityMonoAudioEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityShortcutsEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickDelayMs[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickEventType[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickRevertToLeftClick[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickStabilizePosition[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickMovementThreshold[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickMenuPosition[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCaretHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCursorHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCursorColorEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCursorColor[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityFloatingMenuEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityFloatingMenuPosition[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityFocusHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySelectToSpeakEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessSelectKeyCodes[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessSelectSetting[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessNextKeyCodes[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessNextSetting[];
ASH_PUBLIC_EXPORT extern const char
    kAccessibilitySwitchAccessPreviousKeyCodes[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessPreviousSetting[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessAutoScanEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessAutoScanSpeedMs[];
ASH_PUBLIC_EXPORT extern const char
    kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs[];
ASH_PUBLIC_EXPORT extern const char
    kAccessibilityTabletModeShelfNavigationButtonsEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityDictationEnabled[];
ASH_PUBLIC_EXPORT extern const char kShouldAlwaysShowAccessibilityMenu[];

ASH_PUBLIC_EXPORT extern const char kContextualTooltips[];

ASH_PUBLIC_EXPORT extern const char kDesksNamesList[];

ASH_PUBLIC_EXPORT extern const char kDockedMagnifierEnabled[];
ASH_PUBLIC_EXPORT extern const char kDockedMagnifierScale[];
ASH_PUBLIC_EXPORT extern const char
    kDockedMagnifierAcceleratorDialogHasBeenAccepted[];
ASH_PUBLIC_EXPORT extern const char
    kHighContrastAcceleratorDialogHasBeenAccepted[];
ASH_PUBLIC_EXPORT extern const char
    kScreenMagnifierAcceleratorDialogHasBeenAccepted[];
ASH_PUBLIC_EXPORT extern const char
    kDictationAcceleratorDialogHasBeenAccepted[];
ASH_PUBLIC_EXPORT extern const char
    kDisplayRotationAcceleratorDialogHasBeenAccepted[];
ASH_PUBLIC_EXPORT extern const char
    kDisplayRotationAcceleratorDialogHasBeenAccepted2[];

ASH_PUBLIC_EXPORT extern const char kDisplayMixedMirrorModeParams[];
ASH_PUBLIC_EXPORT extern const char kDisplayPowerState[];
ASH_PUBLIC_EXPORT extern const char kDisplayPrivacyScreenEnabled[];
ASH_PUBLIC_EXPORT extern const char kDisplayProperties[];
ASH_PUBLIC_EXPORT extern const char kDisplayRotationLock[];
ASH_PUBLIC_EXPORT extern const char kDisplayTouchAssociations[];
ASH_PUBLIC_EXPORT extern const char kDisplayTouchPortAssociations[];
ASH_PUBLIC_EXPORT extern const char kExternalDisplayMirrorInfo[];
ASH_PUBLIC_EXPORT extern const char kSecondaryDisplays[];

ASH_PUBLIC_EXPORT extern const char kGestureEducationNotificationShown[];

ASH_PUBLIC_EXPORT extern const char kHasSeenStylus[];
ASH_PUBLIC_EXPORT extern const char kShownPaletteWelcomeBubble[];
ASH_PUBLIC_EXPORT extern const char kEnableStylusTools[];
ASH_PUBLIC_EXPORT extern const char kLaunchPaletteOnEjectEvent[];

// Managed-guest session privacy warning.
ASH_PUBLIC_EXPORT extern const char
    kManagedGuestSessionPrivacyWarningsEnabled[];

// Lock screen notification settings.
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenMode[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeShow[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeHide[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeHideSensitive[];

ASH_PUBLIC_EXPORT extern const char kAmbientColorEnabled[];
ASH_PUBLIC_EXPORT extern const char kNightLightEnabled[];
ASH_PUBLIC_EXPORT extern const char kNightLightTemperature[];
ASH_PUBLIC_EXPORT extern const char kNightLightScheduleType[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomStartTime[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomEndTime[];
ASH_PUBLIC_EXPORT extern const char kNightLightCachedLatitude[];
ASH_PUBLIC_EXPORT extern const char kNightLightCachedLongitude[];
ASH_PUBLIC_EXPORT extern const char kAutoNightLightNotificationDismissed[];

ASH_PUBLIC_EXPORT extern const char kOsSettingsEnabled[];

ASH_PUBLIC_EXPORT extern const char kAllowScreenLock[];
ASH_PUBLIC_EXPORT extern const char kEnableAutoScreenLock[];
ASH_PUBLIC_EXPORT extern const char kPowerAcScreenBrightnessPercent[];
ASH_PUBLIC_EXPORT extern const char kPowerAcScreenDimDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerAcScreenOffDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerAcScreenLockDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerAcIdleWarningDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerAcIdleDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryScreenBrightnessPercent[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryScreenDimDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryScreenOffDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryScreenLockDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryIdleWarningDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryIdleDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerLockScreenDimDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerLockScreenOffDelayMs[];
ASH_PUBLIC_EXPORT extern const char kPowerAcIdleAction[];
ASH_PUBLIC_EXPORT extern const char kPowerBatteryIdleAction[];
ASH_PUBLIC_EXPORT extern const char kPowerLidClosedAction[];
ASH_PUBLIC_EXPORT extern const char kPowerUseAudioActivity[];
ASH_PUBLIC_EXPORT extern const char kPowerUseVideoActivity[];
ASH_PUBLIC_EXPORT extern const char kPowerAllowWakeLocks[];
ASH_PUBLIC_EXPORT extern const char kPowerAllowScreenWakeLocks[];
ASH_PUBLIC_EXPORT extern const char kPowerPresentationScreenDimDelayFactor[];
ASH_PUBLIC_EXPORT extern const char kPowerUserActivityScreenDimDelayFactor[];
ASH_PUBLIC_EXPORT extern const char kPowerWaitForInitialUserActivity[];
ASH_PUBLIC_EXPORT extern const char
    kPowerForceNonzeroBrightnessForUserActivity[];
ASH_PUBLIC_EXPORT extern const char kPowerFastSuspendWhenBacklightsForcedOff[];
ASH_PUBLIC_EXPORT extern const char kPowerSmartDimEnabled[];
ASH_PUBLIC_EXPORT extern const char kPowerAlsLoggingEnabled[];

ASH_PUBLIC_EXPORT extern const char kShelfAlignment[];
ASH_PUBLIC_EXPORT extern const char kShelfAlignmentLocal[];
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehavior[];
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehaviorLocal[];
ASH_PUBLIC_EXPORT extern const char kShelfPreferences[];

ASH_PUBLIC_EXPORT extern const char kShowLogoutButtonInTray[];
ASH_PUBLIC_EXPORT extern const char kLogoutDialogDurationMs[];

ASH_PUBLIC_EXPORT extern const char kUserWallpaperInfo[];
ASH_PUBLIC_EXPORT extern const char kWallpaperColors[];

ASH_PUBLIC_EXPORT extern const char kUserBluetoothAdapterEnabled[];
ASH_PUBLIC_EXPORT extern const char kSystemBluetoothAdapterEnabled[];

ASH_PUBLIC_EXPORT extern const char kTapDraggingEnabled[];
ASH_PUBLIC_EXPORT extern const char kTouchpadEnabled[];
ASH_PUBLIC_EXPORT extern const char kTouchscreenEnabled[];

ASH_PUBLIC_EXPORT extern const char kQuickUnlockPinSalt[];

ASH_PUBLIC_EXPORT extern const char kDetachableBaseDevices[];

ASH_PUBLIC_EXPORT extern const char kCursorMotionBlurEnabled[];

ASH_PUBLIC_EXPORT extern const char kAssistantNumSessionsWhereOnboardingShown[];
ASH_PUBLIC_EXPORT extern const char kAssistantTimeOfLastInteraction[];

ASH_PUBLIC_EXPORT extern const char kVpnConfigAllowed[];

ASH_PUBLIC_EXPORT extern const char kPowerPeakShiftEnabled[];
ASH_PUBLIC_EXPORT extern const char kPowerPeakShiftBatteryThreshold[];
ASH_PUBLIC_EXPORT extern const char kPowerPeakShiftDayConfig[];

ASH_PUBLIC_EXPORT extern const char kBootOnAcEnabled[];

ASH_PUBLIC_EXPORT extern const char kAdvancedBatteryChargeModeEnabled[];
ASH_PUBLIC_EXPORT extern const char kAdvancedBatteryChargeModeDayConfig[];

ASH_PUBLIC_EXPORT extern const char kBatteryChargeMode[];
ASH_PUBLIC_EXPORT extern const char kBatteryChargeCustomStartCharging[];
ASH_PUBLIC_EXPORT extern const char kBatteryChargeCustomStopCharging[];

ASH_PUBLIC_EXPORT extern const char kUsbPowerShareEnabled[];

ASH_PUBLIC_EXPORT extern const char kAssistantPrivacyInfoShownInLauncher[];
ASH_PUBLIC_EXPORT extern const char kAssistantPrivacyInfoDismissedInLauncher[];

ASH_PUBLIC_EXPORT extern const char kSuggestedContentInfoShownInLauncher[];
ASH_PUBLIC_EXPORT extern const char kSuggestedContentInfoDismissedInLauncher[];

ASH_PUBLIC_EXPORT extern const char kLockScreenMediaControlsEnabled[];

ASH_PUBLIC_EXPORT extern const char kXkbAutoRepeatDelay[];
ASH_PUBLIC_EXPORT extern const char kXkbAutoRepeatEnabled[];
ASH_PUBLIC_EXPORT extern const char kXkbAutoRepeatInterval[];

ASH_PUBLIC_EXPORT extern const char kNaturalScroll[];
ASH_PUBLIC_EXPORT extern const char kMouseReverseScroll[];

ASH_PUBLIC_EXPORT extern const char kMultipasteNudges[];

ASH_PUBLIC_EXPORT extern const char kDarkModeEnabled[];
ASH_PUBLIC_EXPORT extern const char kColorModeThemed[];

ASH_PUBLIC_EXPORT extern const char kAppNotificationBadgingEnabled[];

ASH_PUBLIC_EXPORT extern const char kReverseGestureNotificationCount[];

ASH_PUBLIC_EXPORT extern const char kGlobalMediaControlsPinned[];

}  // namespace prefs

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
