// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_SWITCHES_H_
#define ASH_CONSTANTS_ASH_SWITCHES_H_

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}

namespace ash::switches {

// Prefer adding Features over switches. Features go in ash_features.h.
//
// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see `GetOffTheRecordCommandLine()`.)

// Please keep alphabetized.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAggressiveCacheDiscardThreshold[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAllowFailedPolicyFetchForTest[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAllowOsInstall[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAlmanacApiUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAlwaysEnableHdcp[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAppAutoLaunched[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAppOemManifestFile[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcAvailability[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcAvailable[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDataCleanupOnStart[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableAppSync[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableDownloadProvider[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableGmsCoreCache[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableLocaleSync[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kArcDisableMediaStoreMaintenance[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisablePlayAutoInstall[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableTtsCache[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcDisableUreadahead[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kArcHostUreadaheadGeneration[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcForceShowOptInUi[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcGeneratePlayAutoInstall[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kArcInstallEventChromeLogForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcPackagesCacheMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcPlayStoreAutoUpdate[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcScale[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcStartMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcTosHostForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPrivacyPolicyHostForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcVmMountDebugFs[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcVmUreadaheadMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kArcVmUseHugePages[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshClearFastInkBuffer[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshConstrainPointerToRoot[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshContextualNudgesInterval[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshContextualNudgesResetShownCount[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshDebugShortcuts[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshDeveloperShortcuts[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshDisableTouchExplorationMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshEnableCursorMotionBlur[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshEnableMagnifierKeyScroller[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshEnablePaletteOnAllDisplays[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshEnableTabletMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshEnableWaylandServer[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshForceEnableStylusTools[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshForceStatusAreaCollapsible[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshHideNotificationsForFactory[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshNoNudges[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshPowerButtonPosition[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAshSideVolumeButtonPosition[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshTouchHud[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshUiMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshUiModeClamshell[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAshUiModeTablet[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAuraLegacyPowerButton[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kCellularFirst[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kChildWallpaperLarge[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kChildWallpaperSmall[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kCrosRegion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCryptohomeRecoveryReauthUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCryptohomeRecoveryUseTestEnvironment[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kCryptohomeUseAuthSession[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCryptohomeUseOldEncryptionForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDefaultWallpaperIsOem[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDefaultWallpaperLarge[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDefaultWallpaperSmall[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDemoModeEnrollingUsername[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDemoModeForceArcOfflineProvision[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDemoModeHighlightsApp[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDemoModeScreensaverApp[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDemoModeSwaContentDirectory[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDerelictDetectionTimeout[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDerelictIdleTimeout[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableArcCpuRestriction[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableArcDataWipe[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableArcOptInVerification[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableDemoMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableDeviceDisabling[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableFineGrainedTimeZoneDetection[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableGaiaServices[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableHIDDetectionOnOOBEForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableLacrosKeepAliveForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableLoginAnimations[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableLoginLacrosOpening[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableMachineCertRequest[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableOOBEChromeVoxHintTimerForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableOOBENetworkScreenSkippingForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisablePerUserTimezone[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableRollbackOption[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisableSigninFrameClientCerts[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kDisableVolumeAdjustSound[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableArc[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableArcVm[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableArcVmRtVcpu[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableCastReceiver[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableConsumerKiosk[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableDimShelf[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnableExtensionAssetsSharing[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableHoudini[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableHoudini64[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableHoudiniDlc[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableNdkTranslation[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableNdkTranslation64[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnableOOBEChromeVoxHintForDevMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableOobeTestAPI[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableRequisitionEdits[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnableTabletFormFactor[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnableTouchCalibrationSetting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnableTouchpadThreeFingerClick[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEnterpriseDisableArc[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseForceManualEnrollmentInTestBuilds[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnableForcedReEnrollment[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnableUnifiedStateDetermination[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnableInitialEnrollment[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnableZeroTouchEnrollment[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnrollmentInitialModulus[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEnterpriseEnrollmentModulusLimit[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDisallowPolicyBlockDevMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEolIgnoreProfileCreationTime[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEolResetDismissedPrefs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kExtensionInstallEventChromeLogForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kExternalMetricsCollectionInterval[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kExtraWebAppsDir[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kFakeArcRecommendedAppsForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kFakeDriveFsLauncherChrootPath[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kFakeDriveFsLauncherSocketPath[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFingerprintSensorLocation[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFirstExecAfterBoot[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kForceCryptohomeRecoveryForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceDevToolsAvailable[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceFirstRunUI[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kForceHWIDCheckResultForTest[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kForceHappinessTrackingSystem[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceLaunchBrowser[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceLoginManagerInTests[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceShowCursor[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceShowReleaseTrack[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kForceTabletPowerButton[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFormFactor[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kFrameThrottleFps[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kGuestSession[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kGuestWallpaperLarge[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kGuestWallpaperSmall[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHasChromeOSKeyboard[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHasHps[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHasInternalStylus[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHasNumberPad[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kHomedir[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kIgnoreArcVmDevConf[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kIgnoreUnknownAuthFactors[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kIgnoreUserProfileMappingForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kInstallLogFastUploadForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kInstallSystemExtension[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kKioskSplashScreenMinTimeSeconds[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLacrosAvailabilityIgnore[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLacrosChromeAdditionalArgs[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLacrosChromeAdditionalArgsFile[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLacrosChromeAdditionalEnv[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLacrosChromePath[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLacrosMojoSocketForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLacrosSelectionPolicyIgnore[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLaunchRma[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLoginManager[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLoginProfile[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLoginUser[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBrowserDataMigrationForUser[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBrowserDataMigrationMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBrowserDataBackwardMigrationForUser[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kBrowserDataBackwardMigrationMode[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kForceBrowserDataBackwardMigration[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kForceBrowserDataMigrationForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMarketingOptInUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNaturalScrollDefault[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kNoteTakingAppIds[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeEulaUrlForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeForceTabletFirstRun[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOobeLargeScreenSpecialScaling[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOobePrintFrontendLoadTimings[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeScreenshotDirectory[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOobeShowAccessibilityButtonOnMarketingOptInForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeSkipPostLogin[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeSkipToLogin[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kOobeTimerInterval[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOobeTimezoneOverrideForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kOobeTriggerSyncTimeoutForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kHiddenNetworkMigrationInterval[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kHiddenNetworkMigrationAge[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPrintingPpdChannel[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPrintingPpdChannelProduction[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPrintingPpdChannelStaging[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPrintingPpdChannelDev[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPrintingPpdChannelLocalhost[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kProfileRequiresPolicy[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kPublicAccountsSamlAclUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQsAddFakeBluetoothDevices[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQsAddFakeCastDevices[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kQsShowLocaleTile[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRegulatoryLabelDir[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kRemoteRebootCommandTimeoutInSecondsForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRevenBranding[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRlzPingDelay[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kRmaNotAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSafeMode[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSamlPasswordChangeUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kScheduledRebootGracePeriodInSecondsForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShelfHotseat[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowLoginDevOverlay[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowOobeDevOverlay[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowOobeQuickStartDebugger[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kShowTaps[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSkipForceOnlineSignInForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSkipReorderNudgeShowThresholdDurationForTest[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwCursorOnWideDisplays[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kStabilizeTimeDependentViewForTests[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSupportsClamshellAutoRotation[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSuppressMessageCenterPopups[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTelemetryExtensionDirectory[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTestEncryptionMigrationUI[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTestWallpaperServer[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTetherHostScansIgnoreWiredConnections[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTetherStub[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTimeBeforeOnboardingSurveyInSecondsForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kTouchscreenUsableWhileScreenOff[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kTpmIsDynamic[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUnfilteredBluetoothDevices[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kUpdateRequiredAueForTest[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kUseMyFilesInUserDataDirForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kWebUiDataSourcePathForTesting[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kGetAccessTokenForTest[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kCameraEffectsSupportedByHardware[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kPreventKioskAutolaunchForTesting[];

////////////////////////////////////////////////////////////////////////////////

// Returns true if flag if AuthSession should be used to communicate with
// cryptohomed instead of explicitly authorizing each operation.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAuthSessionCryptohomeEnabled();

// Returns true if this is a Cellular First device.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCellularFirstDevice();

// Returns true if this is reven board.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsRevenBranding();

// Returns true if client certificate authentication for the sign-in frame on
// the Chrome OS sign-in screen is enabled.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSigninFrameClientCertsEnabled();

// Returns true if the Chromebook should ignore its wired connections when
// deciding whether to run scans for tethering hosts. Should be used only for
// testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldTetherHostScansIgnoreWiredConnections();

// Returns true if we should skip all other OOBE pages after user login.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldSkipOobePostLogin();

// Returns true if we should show a11y button on the marketing opt in screen.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldShowAccessibilityButtonOnMarketingOptInForTesting();

// Returns true if the device is of tablet form factor.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTabletFormFactor();

// Returns true if GAIA services has been disabled.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaServicesDisabled();

// Returns true if |kDisableArcCpuRestriction| is true.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcCpuRestrictionDisabled();

// Returns true if |kTpmIsDynamic| is true.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTpmDynamic();

// Returns true if all Bluetooth devices in UI (System Tray/Settings Page.)
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUnfilteredBluetoothDevicesEnabled();

// Returns whether the first user run OOBE flow (sequence of screens shown to
// the user on their first login) should show tablet mode screens when the
// device is not in tablet mode.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldOobeUseTabletModeFirstRun();

// Returns whether OOBE should be scaled for CfM devices.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldScaleOobe();

// Returns true if device policy DeviceMinimumVersion should assume that
// Auto Update Expiration is reached. This should only be used for testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsAueReachedForUpdateRequiredForTest();

// Returns true if the OOBE ChromeVox hint idle detection is disabled for
// testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBEChromeVoxHintTimerDisabledForTesting();

// Returns true if the OOBE Network screen skipping check based on ethernet
// connection is disabled for testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBENetworkScreenSkippingDisabledForTesting();

// Returns true if the OOBE ChromeVox hint is enabled for dev mode.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBEChromeVoxHintEnabledForDevMode();

// Returns true if the OEM Device Requisition can be configured.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsDeviceRequisitionConfigurable();

// Returns true if the OS installation UI flow can be entered.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOsInstallAllowed();

COMPONENT_EXPORT(ASH_CONSTANTS)
absl::optional<base::TimeDelta> ContextualNudgesInterval();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ContextualNudgesResetShownCount();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUsingShelfAutoDim();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldClearFastInkBuffer();

// Returns whether the device has hps hardware.
COMPONENT_EXPORT(ASH_CONSTANTS) bool HasHps();

// Returns true if the duration threshold for considering the nudge to be
// shown is skipped.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSkipRecorderNudgeShowThresholdDurationEnabled();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsStabilizeTimeDependentViewForTestsEnabled();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsCameraEffectsSupportedByHardware();

}  // namespace ash::switches

#endif  // ASH_CONSTANTS_ASH_SWITCHES_H_
