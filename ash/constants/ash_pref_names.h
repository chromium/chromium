// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_PREF_NAMES_H_
#define ASH_CONSTANTS_ASH_PREF_NAMES_H_

#include "base/component_export.h"

namespace ash {
namespace prefs {

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistiveInputFeatureSettings[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistPersonalInfoEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistPredictiveWritingEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCalendarIntegrationEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kESimRefreshedEuiccs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kESimProfiles[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kManagedCellularIccidSmdpPair[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEmojiSuggestionEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEmojiSuggestionEnterpriseAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioDevicesMute[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioDevicesGainPercent[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioDevicesVolumePercent[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioMute[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioOutputAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioVolumePercent[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioDevicesState[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioInputDevicesUserPriority[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioOutputDevicesUserPriority[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioDevicesLastSeen[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEduCoexistenceId[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEduCoexistenceToSVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEduCoexistenceToSAcceptedVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShouldSkipInlineLoginWelcomePage[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kQuirksClientLastServerCheck[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceWiFiFastTransitionEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kInputNoiseCancellationEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMultitaskMenuNudgeClamshellShownCount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMultitaskMenuNudgeTabletShownCount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMultitaskMenuNudgeClamshellLastShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMultitaskMenuNudgeTabletLastShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordModifiedTime[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordExpirationTime[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordChangeUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSyncOobeCompleted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemWebAppLastUpdateVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemWebAppLastInstalledLocale[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemWebAppInstallFailureCount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemWebAppLastAttemptedVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemWebAppLastAttemptedLocale[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLoginDisplayPasswordButtonEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSuggestedContentEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherContinueSectionHidden[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherFeedbackOnContinueSectionSent[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherLastContinueRequestTime[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherResultEverLaunched[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherSearchNormalizerParameters[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherUseLongContinueDelay[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceSystemWideTracingEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityLargeCursorEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityLargeCursorDipSize[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityStickyKeysEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySpokenFeedbackEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxAutoRead[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxAnnounceDownloadNotifications[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxAnnounceRichTextAttributes[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxAudioStrategy[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleSideBySide[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleTable[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleTable6[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleTable8[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleTableType[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxBrailleWordWrap[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxCapitalStrategy[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxCapitalStrategyBackup[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxEnableBrailleLogging[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxEnableEarconLogging[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxEnableEventStreamLogging[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxEnableSpeechLogging[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxEventStreamFilters[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxLanguageSwitching[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxMenuBrailleCommands[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxNumberReadingStyle[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxPreferredBrailleDisplayAddress[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxPunctuationEcho[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxSmartStickyMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxSpeakTextUnderMouse[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxUsePitchChanges[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxUseVerboseMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxVirtualBrailleColumns[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxVirtualBrailleRows[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityChromeVoxVoiceName[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityHighContrastEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityScreenMagnifierCenterFocus[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityScreenMagnifierEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityScreenMagnifierFocusFollowingEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityScreenMagnifierMouseFollowingMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityScreenMagnifierScale[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityVirtualKeyboardEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityVirtualKeyboardFeatures[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityMonoAudioEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityShortcutsEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickEventType[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickRevertToLeftClick[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickStabilizePosition[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickMovementThreshold[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityAutoclickMenuPosition[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityColorFiltering[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityGreyscaleAmount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySaturationAmount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySepiaAmount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityHueRotationAmount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityColorVisionCorrectionAmount[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityColorVisionDeficiencyType[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityCaretHighlightEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityCursorHighlightEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityCursorColorEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAccessibilityCursorColor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityFloatingMenuEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityFloatingMenuPosition[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityFocusHighlightEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessSelectDeviceKeyCodes[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessNextDeviceKeyCodes[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessPreviousDeviceKeyCodes[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessAutoScanEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessAutoScanSpeedMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityTabletModeShelfNavigationButtonsEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityDictationEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityDictationLocale[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityDictationLocaleOfflineNudge[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakBackgroundShading[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakEnhancedNetworkVoices[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakEnhancedVoiceName[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakEnhancedVoicesDialogShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakHighlightColor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakNavigationControls[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakVoiceName[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakVoiceSwitching[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAccessibilitySelectToSpeakWordHighlight[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShouldAlwaysShowAccessibilityMenu[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAltTabPerDesk[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kContextualTooltips[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDesksNamesList[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDesksGuidsList[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDesksMetricsList[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDesksWeeklyActiveDesksMetrics[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDesksActiveDesk[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDockedMagnifierEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDockedMagnifierScale[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDockedMagnifierScreenHeightDivisor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDockedMagnifierAcceleratorDialogHasBeenAccepted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kHighContrastAcceleratorDialogHasBeenAccepted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kScreenMagnifierAcceleratorDialogHasBeenAccepted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDictationAcceleratorDialogHasBeenAccepted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDictationDlcSuccessNotificationHasBeenShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDictationDlcOnlySodaDownloadedNotificationHasBeenShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDictationNoDlcsDownloadedNotificationHasBeenShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisplayRotationAcceleratorDialogHasBeenAccepted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisplayRotationAcceleratorDialogHasBeenAccepted2[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisplayMixedMirrorModeParams[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisplayPowerState[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisplayPrivacyScreenEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisplayProperties[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisplayRotationLock[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisplayTouchAssociations[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisplayTouchPortAssociations[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kExternalDisplayMirrorInfo[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSecondaryDisplays[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAllowMGSToStoreDisplayProperties[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFullscreenAlertEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kGestureEducationNotificationShown[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHasSeenStylus[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShownPaletteWelcomeBubble[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableStylusTools[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLaunchPaletteOnEjectEvent[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLocalStateDevicePeripheralDataAccessEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLoginShutdownTimestampPrefName[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCanCellularSetupNotificationBeShown[];

// Managed-guest session privacy warning.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kManagedGuestSessionPrivacyWarningsEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSnoopingProtectionEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSnoopingProtectionNotificationSuppressionEnabled[];

// Lock screen notification settings.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMessageCenterLockScreenMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMessageCenterLockScreenModeShow[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMessageCenterLockScreenModeHide[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kMessageCenterLockScreenModeHideSensitive[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAmbientColorEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDarkModeEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDarkModeScheduleType[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightTemperature[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightScheduleType[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightCustomStartTime[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightCustomEndTime[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightCachedLatitude[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNightLightCachedLongitude[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAutoNightLightNotificationDismissed[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDynamicColorColorScheme[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDynamicColorSeedColor[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOsSettingsEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAllowScreenLock[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableAutoScreenLock[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerAcScreenBrightnessPercent[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcScreenDimDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcScreenOffDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcScreenLockDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcIdleWarningDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcIdleDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerAdaptiveChargingEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerAdaptiveChargingNudgeShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerBatteryScreenBrightnessPercent[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerBatteryScreenDimDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerBatteryScreenOffDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerBatteryScreenLockDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerBatteryIdleWarningDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerBatteryIdleDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerLockScreenDimDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerLockScreenOffDelayMs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAcIdleAction[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerBatteryIdleAction[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerLidClosedAction[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerUseAudioActivity[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerUseVideoActivity[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAllowWakeLocks[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAllowScreenWakeLocks[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerPresentationScreenDimDelayFactor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerUserActivityScreenDimDelayFactor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerWaitForInitialUserActivity[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerForceNonzeroBrightnessForUserActivity[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerFastSuspendWhenBacklightsForcedOff[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerSmartDimEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerAlsLoggingEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerQuickDimEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerQuickLockDelay[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfAlignment[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfAlignmentLocal[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfAutoHideBehavior[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfAutoHideBehaviorLocal[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShelfAutoHideTabletModeBehavior[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShelfAutoHideTabletModeBehaviorLocal[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfLauncherNudge[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfPreferences[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowLogoutButtonInTray[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLogoutDialogDurationMs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSuggestLogoutAfterClosingLastWindow[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUserWallpaperInfo[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kRecentDailyGooglePhotosWallpapers[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kWallpaperColors[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kWallpaperMeanColors[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kWallpaperCelebiColors[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSyncableWallpaperInfo[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kUserBluetoothAdapterEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemBluetoothAdapterEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSystemTrayExpanded[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUserCameraAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUserMicrophoneAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUserGeolocationAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDeviceGeolocationAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceGeolocationCachedLatitude[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceGeolocationCachedLongitude[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTapDraggingEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchscreenEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPinUnlockMaximumLength[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPinUnlockMinimumLength[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPinUnlockWeakPinsAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kQuickUnlockFingerprintRecord[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQuickUnlockModeAllowlist[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kWebAuthnFactors[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQuickUnlockPinSalt[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQuickUnlockPinSecret[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQuickUnlockTimeout[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDetachableBaseDevices[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kCursorMotionBlurEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistantNumSessionsWhereOnboardingShown[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistantTimeOfLastInteraction[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kVpnConfigAllowed[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerPeakShiftEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPowerPeakShiftBatteryThreshold[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPowerPeakShiftDayConfig[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kBootOnAcEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAdvancedBatteryChargeModeEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAdvancedBatteryChargeModeDayConfig[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kBatteryChargeMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBatteryChargeCustomStartCharging[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBatteryChargeCustomStopCharging[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUsbPowerShareEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kUsbPeripheralCableSpeedNotificationShown[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAppListReorderNudge[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherFilesPrivacyNotice[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLockScreenMediaControlsEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLongPressDiacriticsEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kXkbAutoRepeatDelay[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kXkbAutoRepeatEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kXkbAutoRepeatInterval[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSendFunctionKeys[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNaturalScroll[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMouseReverseScroll[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMultipasteNudges[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAppNotificationBadgingEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kGlobalMediaControlsPinned[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPciePeripheralDisplayNotificationRemaining[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLastUsedImeShortcutReminderDismissed[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kNextImeShortcutReminderDismissed[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceI18nShortcutsEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLacrosProxyControllingExtension[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kFastPairEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kUserPairedWithFastPair[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDeskTemplatesEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPreconfiguredDeskTemplates[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorAnnotatorLastUsedMarkerColor[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorCreationFlowEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorCreationFlowLanguage[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorGalleryOnboardingShowCount[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorViewerOnboardingShowCount[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorExcludeTranscriptDialogShown[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorAllowByPolicy[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kProjectorDogfoodForFamilyLinkEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kChromadToCloudMigrationEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFilesAppFolderShortcuts[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFilesAppUIPrefsMigrated[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFilesAppTrashEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLoginScreenWebUILazyLoading[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFloatingWorkspaceEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFloatingWorkspaceV2Enabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowPostRebootNotification[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kUsbDetectorNotificationEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPersonalizationKeyboardBacklightColor[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPersonalizationKeyboardBacklightZoneColors[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPersonalizationKeyboardBacklightColorDisplayType[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShowTouchpadScrollScreenEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAutozoomState[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAutozoomNudges[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRecoveryFactorBehavior[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kApnMigratedIccids[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kBackgroundBlur[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kBackgroundReplace[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPortraitRelighting[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRecordArcAppSyncMetrics[];

// Input device settings.
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPrimaryMouseButtonRight[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMouseSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMouseAcceleration[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMouseScrollSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMouseScrollAcceleration[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadAcceleration[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnableTouchpadThreeFingerClick[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTapToClickEnabled[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadScrollSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadScrollAcceleration[];

COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTouchpadHapticFeedback[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTouchpadHapticClickSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPointingStickSensitivity[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPrimaryPointingStickButtonRight[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPointingStickAcceleration[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTimeOfLastSessionActivation[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOwnerPrimaryMouseButtonRight[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOwnerPrimaryPointingStickButtonRight[];

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOwnerTapToClickEnabled[];

}  // namespace prefs
}  // namespace ash

#endif  // ASH_CONSTANTS_ASH_PREF_NAMES_H_
