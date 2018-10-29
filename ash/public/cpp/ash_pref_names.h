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
ASH_PUBLIC_EXPORT extern const char kAccessibilityMonoAudioEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickDelayMs[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickEventType[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickRevertToLeftClick[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCaretHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCursorHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityFocusHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySelectToSpeakEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityDictationEnabled[];
ASH_PUBLIC_EXPORT extern const char kShouldAlwaysShowAccessibilityMenu[];

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

ASH_PUBLIC_EXPORT extern const char kDisplayMixedMirrorModeParams[];
ASH_PUBLIC_EXPORT extern const char kDisplayPowerState[];
ASH_PUBLIC_EXPORT extern const char kDisplayProperties[];
ASH_PUBLIC_EXPORT extern const char kDisplayRotationLock[];
ASH_PUBLIC_EXPORT extern const char kDisplayTouchAssociations[];
ASH_PUBLIC_EXPORT extern const char kDisplayTouchPortAssociations[];
ASH_PUBLIC_EXPORT extern const char kExternalDisplayMirrorInfo[];
ASH_PUBLIC_EXPORT extern const char kSecondaryDisplays[];

ASH_PUBLIC_EXPORT extern const char kHasSeenStylus[];
ASH_PUBLIC_EXPORT extern const char kShownPaletteWelcomeBubble[];
ASH_PUBLIC_EXPORT extern const char kEnableStylusTools[];
ASH_PUBLIC_EXPORT extern const char kLaunchPaletteOnEjectEvent[];

// Lock screen notification settings.
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenMode[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeShow[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeHide[];
ASH_PUBLIC_EXPORT extern const char kMessageCenterLockScreenModeHideSensitive[];

ASH_PUBLIC_EXPORT extern const char kNightLightEnabled[];
ASH_PUBLIC_EXPORT extern const char kNightLightTemperature[];
ASH_PUBLIC_EXPORT extern const char kNightLightScheduleType[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomStartTime[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomEndTime[];

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
ASH_PUBLIC_EXPORT extern const char kPowerSmartDimEnabled[];

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

}  // namespace prefs

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
