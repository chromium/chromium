// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_SWITCHES_H_
#define ASH_CONSTANTS_ASH_SWITCHES_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/component_export.h"

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
inline constexpr char kAggressiveCacheDiscardThreshold[] =
    "aggressive-cache-discard";

// If this flag is passed, failed policy fetches will not cause profile
// initialization to fail. This is useful for tests because it means that
// tests don't have to mock out the policy infrastructure.
inline constexpr char kAllowFailedPolicyFetchForTest[] =
    "allow-failed-policy-fetch-for-test";

// When this flag is set, the OS installation UI can be accessed. This
// allows the user to install from USB to disk.
inline constexpr char kAllowOsInstall[] = "allow-os-install";

// Override for the URL used for the ChromeOS Almanac API. Used for local
// testing with a non-production server (e.g.
// "--almanac-api-url=http://localhost:8000").
inline constexpr char kAlmanacApiUrl[] = "almanac-api-url";

// Causes HDCP of the specified type to always be enabled when an external
// display is connected. Used for HDCP compliance testing on ChromeOS.
inline constexpr char kAlwaysEnableHdcp[] = "always-enable-hdcp";

// Specifies whether an app launched in kiosk mode was auto launched with zero
// delay. Used in order to properly restore auto-launched state during session
// restore flow.
inline constexpr char kAppAutoLaunched[] = "app-auto-launched";

// Path for app's OEM manifest file.
inline constexpr char kAppOemManifestFile[] = "app-mode-oem-manifest";

// Signals ARC support status on this device. This can take one of the
// following three values.
// - none: ARC is not installed on this device. (default)
// - installed: ARC is installed on this device, but not officially supported.
//   Users can enable ARC only when Finch experiment is turned on.
// - officially-supported: ARC is installed and supported on this device. So
//   users can enable ARC via settings etc.
inline constexpr char kArcAvailability[] = "arc-availability";

// DEPRECATED: Please use --arc-availability=installed.
// Signals the availability of the ARC instance on this device.
inline constexpr char kArcAvailable[] = "arc-available";

// Switch that blocks KeyMint. When KeyMint is blocked, Keymaster is enabled.
inline constexpr char kArcBlockKeyMint[] = "arc-block-keymint";

// Flag that forces ARC data be cleaned on each start.
inline constexpr char kArcDataCleanupOnStart[] = "arc-data-cleanup-on-start";

// Flag that disables ARC app sync flow that installs some apps silently. Used
// in autotests to resolve racy conditions.
inline constexpr char kArcDisableAppSync[] = "arc-disable-app-sync";

// Used in tests to disable DexOpt cache which is on by default.
inline constexpr char kArcDisableDexOptCache[] = "arc-disable-dexopt-cache";

// Flag that disables ARC download provider that prevents extra content to be
// downloaded and installed in context of Play Store and GMS Core.
inline constexpr char kArcDisableDownloadProvider[] =
    "arc-disable-download-provider";

// Used in autotest to disable GMS-core caches which is on by default.
inline constexpr char kArcDisableGmsCoreCache[] = "arc-disable-gms-core-cache";

// Flag that disables ARC locale sync with Android Container. Used in autotest
// to prevent conditions when certain apps, including Play Store may get
// restarted. Restarting Play Store may cause random test failures. Enabling
// this flag would also forces ARC Container to use 'en-US' as a locale and
// 'en-US,en' as preferred languages.
inline constexpr char kArcDisableLocaleSync[] = "arc-disable-locale-sync";

// Used to disable GMS scheduling of media store periodic indexing and corpora
// maintenance tasks. Used in performance tests to prevent running during
// testing which can cause unstable results or CPU not idle pre-test failures.
inline constexpr char kArcDisableMediaStoreMaintenance[] =
    "arc-disable-media-store-maintenance";

// Flag that disables ARC Play Auto Install flow that installs set of predefined
// apps silently. Used in autotests to resolve racy conditions.
inline constexpr char kArcDisablePlayAutoInstall[] =
    "arc-disable-play-auto-install";

// Used in autotest to disable TTS cache which is on by default.
inline constexpr char kArcDisableTtsCache[] = "arc-disable-tts-cache";

// Flag that enables key and ID attestation for KeyMint.
inline constexpr char kArcEnableAttestation[] = "arc-enable-attestation";

// Flag that indicates ARC is using dev caches generated by data collector in
// Uprev rather than caches from CrOS build stage for arccachesetup service.
inline constexpr char kArcUseDevCaches[] = "arc-use-dev-caches";

// Flag that indicates ARC images are formatted with EROFS (go/arcvm-erofs).
inline constexpr char kArcErofs[] = "arc-erofs";

// Flag that forces Android volumes (DocumentsProviders and Play files) to be
// mounted in the Files app. Used for testing.
inline constexpr char kArcForceMountAndroidVolumesInFiles[] =
    "arc-force-mount-android-volumes-in-files";

// Flag that forces the OptIn ui to be shown. Used in tests.
inline constexpr char kArcForceShowOptInUi[] = "arc-force-show-optin-ui";

// Flag that enables developer options needed to generate an ARC Play Auto
// Install roster. Used manually by developers.
inline constexpr char kArcGeneratePlayAutoInstall[] =
    "arc-generate-play-auto-install";

// Sets the mode of operation for ureadahead during ARC Container boot.
// readahead (default) - used during production and is equivalent to no switch
//                       being set.
// generate - used during Android Uprev data collector to pre-generate pack file
//            and upload to Google Cloud as build artifact for CrOS build image.
// disabled - used for test purpose to disable ureadahead during ARC Container
// boot.
inline constexpr char kArcHostUreadaheadMode[] = "arc-host-ureadahead-mode";

// Write ARC++ install events to chrome log for integration test.
inline constexpr char kArcInstallEventChromeLogForTests[] =
    "arc-install-event-chrome-log-for-tests";

// Used in autotest to specifies how to handle packages cache. Can be
// copy - copy resulting packages.xml to the temporary directory.
// skip-copy - skip initial packages cache setup and copy resulting packages.xml
//             to the temporary directory.
inline constexpr char kArcPackagesCacheMode[] = "arc-packages-cache-mode";

// Used in autotest to forces Play Store auto-update state. Can be
// on - auto-update is forced on.
// off - auto-update is forced off.
inline constexpr char kArcPlayStoreAutoUpdate[] = "arc-play-store-auto-update";

// Set the scale for ARC apps. This is in DPI. e.g. 280 DPI is ~ 1.75 device
// scale factor.
// See
// https://source.android.com/compatibility/android-cdd#3_7_runtime_compatibility
// for list of supported DPI values.
inline constexpr char kArcScale[] = "arc-scale";

// Defines how to start ARC. This can take one of the following values:
// - always-start automatically start with Play Store UI support.
// - always-start-with-no-play-store automatically start without Play Store UI.
// If it is not set, then ARC is started in default mode.
inline constexpr char kArcStartMode[] = "arc-start-mode";

// Sets ARC Terms Of Service hostname url for testing.
inline constexpr char kArcTosHostForTests[] = "arc-tos-host-for-tests";

// Sets the mode of operation for ureadahead during ARCVM boot. If this switch
// is not set, ARCVM ureadahead will check for the presence and age of pack
// file and reads ahead files to page cache for improved boot performance.
// readahead (default) - used during production and is equivalent to no switch
//                       being set. This is used in tast test to explicitly turn
//                       on guest ureadahead (see |kArcDisableUreadahead|).
// generate - used during Android Uprev data collector to pre-generate pack file
//            and upload to Google Cloud as build artifact for CrOS build image.
// disabled - used for test purpose to disable ureadahead during ARCVM boot.
//            note, |kArcDisableUreadahead| also disables both, guest and host
//            parts of ureadahead.
inline constexpr char kArcVmUreadaheadMode[] = "arcvm-ureadahead-mode";

// Madvises the kernel to use Huge Pages for guest memory.
inline constexpr char kArcVmUseHugePages[] = "arcvm-use-hugepages";

// Force the pointer (cursor) position to be kept inside root windows.
inline constexpr char kAshConstrainPointerToRoot[] =
    "ash-constrain-pointer-to-root";

// Overrides the minimum time that must pass between showing user contextual
// nudges. Unit of time is in seconds.
inline constexpr char kAshContextualNudgesInterval[] =
    "ash-contextual-nudges-interval";

// Reset contextual nudge shown count on login.
inline constexpr char kAshContextualNudgesResetShownCount[] =
    "ash-contextual-nudges-reset-shown-count";

// Enable keyboard shortcuts useful for debugging.
inline constexpr char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enable keyboard shortcuts used by developers only.
inline constexpr char kAshDeveloperShortcuts[] = "ash-dev-shortcuts";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
inline constexpr char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables key bindings to scroll magnified screen.
inline constexpr char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the palette on every display, instead of only the internal one.
inline constexpr char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// If the flag is present, it indicates 1) the device has accelerometer and 2)
// the device is a convertible device or a tablet device (thus is capable of
// entering tablet mode). If this flag is not set, then the device is not
// capable of entering tablet mode. For example, Samus has accelerometer, but
// is not a covertible or tablet, thus doesn't have this flag set, thus can't
// enter tablet mode.
inline constexpr char kAshEnableTabletMode[] = "enable-touchview";

// Enable the wayland server.
inline constexpr char kAshEnableWaylandServer[] = "enable-wayland-server";

// Enables the stylus tools next to the status area.
inline constexpr char kAshForceEnableStylusTools[] =
    "force-enable-stylus-tools";

// Forces the status area to allow collapse/expand regardless of the current
// state.
inline constexpr char kAshForceStatusAreaCollapsible[] =
    "force-status-area-collapsible";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
inline constexpr char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Hides educational nudges that can interfere with tast integration tests.
// Somewhat similar to --no-first-run but affects system UI behavior, not
// browser behavior.
inline constexpr char kAshNoNudges[] = "ash-no-nudges";

// Power button position includes the power button's physical display side and
// the percentage for power button center position to the display's
// width/height in landscape_primary screen orientation. The value is a JSON
// object containing a "position" property with the value "left", "right",
// "top", or "bottom". For "left" and "right", a "y" property specifies the
// button's center position as a fraction of the display's height (in [0.0,
// 1.0]) relative to the top of the display. For "top" and "bottom", an "x"
// property gives the position as a fraction of the display's width relative to
// the left side of the display.
inline constexpr char kAshPowerButtonPosition[] = "ash-power-button-position";

// The physical position info of the side volume button while in landscape
// primary screen orientation. The value is a JSON object containing a "region"
// property with the value "keyboard", "screen" and a "side" property with the
// value "left", "right", "top", "bottom".
inline constexpr char kAshSideVolumeButtonPosition[] =
    "ash-side-volume-button-position";

// Enables the heads-up display for tracking touch points.
inline constexpr char kAshTouchHud[] = "ash-touch-hud";

// Enables required things for the selected UI mode, regardless of whether the
// Chromebook is currently in the selected UI mode.
inline constexpr char kAshUiMode[] = "force-tablet-mode";

// Values for the kAshUiMode flag.
inline constexpr char kAshUiModeClamshell[] = "clamshell";

inline constexpr char kAshUiModeTablet[] = "touch_view";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
inline constexpr char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Sets the birch ranker to assume it is evening for birch chip ranking
// purposes.
inline constexpr char kBirchIsEvening[] = "birch-is-evening";

// Sets the birch ranker to assume it is morning for birch chip ranking
// purposes.
inline constexpr char kBirchIsMorning[] = "birch-is-morning";

// Switch used to pass in a secret key for Campbell feature. Unless the correct
// secret key is provided, Campbell feature will remain disabled, regardless of
// the state of the associated feature flag.
inline constexpr char kCampbellKey[] = "campbell-key";

// If this flag is set, it indicates that this device is a "Cellular First"
// device. Cellular First devices use cellular telephone data networks as
// their primary means of connecting to the internet.
// Setting this flag has two consequences:
// 1. Cellular data roaming will be enabled by default.
// 2. UpdateEngine will be instructed to allow auto-updating over cellular
//    data connections.
inline constexpr char kCellularFirst[] = "cellular-first";

// Default large wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
inline constexpr char kChildWallpaperLarge[] = "child-wallpaper-large";

// Default small wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
inline constexpr char kChildWallpaperSmall[] = "child-wallpaper-small";

// Some platforms like ChromeOS default to empty desktop.
// Browser tests may need to add this switch so that at least one browser
// instance is created on startup.
// TODO(nkostylev): Investigate if this switch could be removed.
// (http://crbug.com/40933835)
inline constexpr char kCreateBrowserOnStartupForTests[] =
    "create-browser-on-startup-for-tests";

// Forces CrOS region value.
inline constexpr char kCrosRegion[] = "cros-region";

// Custom crosh command.
inline constexpr char kCroshCommand[] = "crosh-command";

// Overrides the base url for the Cryptohome recovery service.
inline constexpr char kCryptohomeRecoveryServiceBaseUrl[] =
    "cryptohome-recovery-service-base-url";

// Forces cryptohome recovery process to use test environment (test keys /
// URLs).
inline constexpr char kCryptohomeRecoveryUseTestEnvironment[] =
    "cryptohome-recovery-use-test-env";

// Controls if AuthSession API should be used when interacting with cryptohomed.
inline constexpr char kCryptohomeUseAuthSession[] =
    "cryptohome-use-authsession";

// Forces cryptohome to create new users using old (ecryptfs) encryption.
// This switch can be used to set up configurations that can be used to
// test encryption migration scenarios.
inline constexpr char kCryptohomeUseOldEncryptionForTesting[] =
    "cryptohome-use-old-encryption-for-testing";

// Normally the cryptohome without any any authentication factors
// is considered corrupted. Special mechanism would detect such situation
// during user creation and remove such users. If such user is an owner
// the power wash should be triggered instead. However, if such event happens
// in tests, all logs would be lost, and it would be difficult to investigate
// exact reason behind the Owner user being misconfigured.
// This flag prevents triggering powerwash in such cases, simple user removal
// would be triggered instead.
inline constexpr char kCryptohomeIgnoreCleanupOwnershipForTesting[] =
    "cryptohome-ignore-cleanup-ownership-for-testing";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
inline constexpr char kDefaultWallpaperIsOem[] = "default-wallpaper-is-oem";

// Default large wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
inline constexpr char kDefaultWallpaperLarge[] = "default-wallpaper-large";

// Default small wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
inline constexpr char kDefaultWallpaperSmall[] = "default-wallpaper-small";

// Interval in seconds to wait for a display to reconnect while unlocking or
// logging in with a closed lid.
inline constexpr char kDeferExternalDisplayTimeout[] =
    "defer-external-display-timeout";

// Test Organization Unit (OU) user to use for demo mode. Only pass the part
// before "@cros-demo-mode.com".
inline constexpr char kDemoModeEnrollingUsername[] =
    "demo-mode-enrolling-username";

// Force ARC provision to take code path for offline demo mode.
inline constexpr char kDemoModeForceArcOfflineProvision[] =
    "demo-mode-force-arc-offline-provision";

// App ID to use for highlights app in demo mode.
inline constexpr char kDemoModeHighlightsApp[] =
    "demo-mode-highlights-extension";

// API key for demo mode server.
inline constexpr char kDemoModeServerAPIKey[] = "demo-mode-server-api-key";

// Override the prodution demo mode api url fo demo account sign in.
inline constexpr char kDemoModeServerUrl[] = "demo-mode-server-url";

// App ID to use for screensaver app in demo mode.
inline constexpr char kDemoModeScreensaverApp[] =
    "demo-mode-screensaver-extension";

// Directory from which to fetch the demo mode SWA content (instead of
// downloading from Omaha).
inline constexpr char kDemoModeSwaContentDirectory[] =
    "demo-mode-swa-content-directory";

// Directory from which to fetch the demo mode resource content (instead of
// downloading from Omaha).
inline constexpr char kDemoModeResourceDirectory[] =
    "demo-mode-resource-directory";

// Time in seconds before a machine at OOBE is considered derelict.
inline constexpr char kDerelictDetectionTimeout[] =
    "derelict-detection-timeout";

// Time in seconds before a derelict machines starts demo mode.
inline constexpr char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Prevents any CPU restrictions being set on ARC[VM]. Only meant to be used by
// tests as some tests may time out if the ARC container is throttled.
inline constexpr char kDisableArcCpuRestriction[] =
    "disable-arc-cpu-restriction";

// Disables ARC Opt-in verification process and ARC is enabled by default.
inline constexpr char kDisableArcOptInVerification[] =
    "disable-arc-opt-in-verification";

// Disables the Weather API from being called by Birch. Allows fake users in
// tast tests to avoid making API calls using an invalid GAIA ID, which causes
// errors on the weather server side.
inline constexpr char kDisableBirchWeatherApiForTesting[] =
    "disable-birch-weather-api-for-testing";

// Disables the Chrome OS demo.
inline constexpr char kDisableDemoMode[] = "disable-demo-mode";

// If this switch is set, the device cannot be remotely disabled by its owner.
inline constexpr char kDisableDeviceDisabling[] = "disable-device-disabling";

// Disables DriveFS for testing purposes, used in tast testing and only on test
// images.
inline constexpr char kDisableDriveFsForTesting[] =
    "disable-drive-fs-for-testing";

// Disables fine grained time zone detection.
inline constexpr char kDisableFineGrainedTimeZoneDetection[] =
    "disable-fine-grained-time-zone-detection";

// Disables first-run UI from being shown.
inline constexpr char kDisableFirstRunUI[] = "disable-first-run-ui";

// Disables GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
inline constexpr char kDisableGaiaServices[] = "disable-gaia-services";

// Disables HID-detection OOBE screen.
inline constexpr char kDisableHIDDetectionOnOOBEForTesting[] =
    "disable-hid-detection-on-oobe";

// Disables the redirect of console to /var/log/messages in ChromeOS.
inline constexpr char kDisableLoggingRedirect[] = "disable-logging-redirect";

// Avoid doing expensive animations upon login.
inline constexpr char kDisableLoginAnimations[] = "disable-login-animations";

// Disables apps on the login screen. By default, they are allowed and can be
// installed through policy.
inline constexpr char kDisableLoginScreenApps[] = "disable-login-screen-apps";

// Disables requests for an enterprise machine certificate during attestation.
inline constexpr char kDisableMachineCertRequest[] =
    "disable-machine-cert-request";

// Disables the ChromeVox hint idle detection in OOBE, which can lead to
// unexpected behavior during tests.
inline constexpr char kDisableOOBEChromeVoxHintTimerForTesting[] =
    "disable-oobe-chromevox-hint-timer-for-testing";

// Skips OOBE network setup even if there is no internet connection.
inline constexpr char kOOBESkipNetworkSetupForTesting[] =
    "oobe-skip-network-setup-for-testing";

// Disables network screen skip check which is based on ethernet connection.
inline constexpr char kDisableOOBENetworkScreenSkippingForTesting[] =
    "disable-oobe-network-screen-skipping-for-testing";

// Disables per-user timezone.
inline constexpr char kDisablePerUserTimezone[] = "disable-per-user-timezone";

// If set, the power button in tablet mode is disabled.
inline constexpr char kDisablePowerButtonInTabletMode[] =
    "disable-power-button-in-tablet-mode";

// Disables rollback option on reset screen.
inline constexpr char kDisableRollbackOption[] = "disable-rollback-option";

// Disables volume adjust sound.
inline constexpr char kDisableVolumeAdjustSound[] =
    "disable-volume-adjust-sound";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/40756809)
inline constexpr char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Disables the Welcome Recap feature for factory testing.
inline constexpr char kDisableWelcomeRecapForFactoryTest[] =
    "disable-welcome-recap-for-factory-testing";

// DEPRECATED. Please use --arc-availability=officially-supported.
// Enables starting the ARC instance upon session start.
inline constexpr char kEnableArc[] = "enable-arc";

// Enables ARCVM.
inline constexpr char kEnableArcVm[] = "enable-arcvm";

// Enables ARCVM DLC.
inline constexpr char kEnableArcVmDlc[] = "enable-arcvm-dlc";

// This flag is set when the device's hardware meets the hardware requirements
// for the ARCVM DLC.
inline constexpr char kArcVmDlcHardwareRequirementSatisfied[] =
    "arcvm-dlc-hardware-satisfied";

// Used to override `kDisableBirchWeatherApiForTesting` for specific tast tests.
inline constexpr char kEnableBirchWeatherApiForTestingOverride[] =
    "enable-birch-weather-api-for-testing-override";

// Enables the Cast Receiver.
inline constexpr char kEnableCastReceiver[] = "enable-cast-receiver";

// Enables Shelf Dimming for ChromeOS.
inline constexpr char kEnableDimShelf[] = "enable-dim-shelf";

// Enables sharing assets for installed default apps.
inline constexpr char kEnableExtensionAssetsSharing[] =
    "enable-extension-assets-sharing";

// Enables the use of 32-bit Houdini library for ARM binary translation.
inline constexpr char kEnableHoudini[] = "enable-houdini";

// Enables the use of 64-bit Houdini library for ARM binary translation.
inline constexpr char kEnableHoudini64[] = "enable-houdini64";

// Enables the use of 32-bit NDK translation library for ARM binary translation.
inline constexpr char kEnableNdkTranslation[] = "enable-ndk-translation";

// Enables the use of 64-bit NDK translation library for ARM binary translation.
inline constexpr char kEnableNdkTranslation64[] = "enable-ndk-translation64";

// Enables the ChromeVox hint in OOBE for dev mode. This flag is used
// to override the default dev mode behavior of disabling the feature.
// If both kEnableOOBEChromeVoxHintForDevMode and
// kDisableOOBEChromeVoxHintTimerForTesting are present, the ChromeVox hint
// will be disabled, since the latter flag takes precedence over the former.
inline constexpr char kEnableOOBEChromeVoxHintForDevMode[] =
    "enable-oobe-chromevox-hint-timer-for-dev-mode";

// Enables OOBE testing API for tast tests.
inline constexpr char kEnableOobeTestAPI[] = "enable-oobe-test-api";

// Enables configuring the OEM Device Requisition in the OOBE.
inline constexpr char kEnableRequisitionEdits[] = "enable-requisition-edits";

// Enables tablet form factor.
inline constexpr char kEnableTabletFormFactor[] = "enable-tablet-form-factor";

// Enables the touch calibration option in MD settings UI for valid touch
// displays.
inline constexpr char kEnableTouchCalibrationSetting[] =
    "enable-touch-calibration-setting";

// Enables touchpad three-finger-click as middle button.
inline constexpr char kEnableTouchpadThreeFingerClick[] =
    "enable-touchpad-three-finger-click";

// Disables ARC for managed accounts.
inline constexpr char kEnterpriseDisableArc[] = "enterprise-disable-arc";

// Whether to force manual enrollment instead of trying cert based enrollment.
// Only works on test builds.
inline constexpr char kEnterpriseForceManualEnrollmentInTestBuilds[] =
    "enterprise-force-manual-enrollment-in-test-builds";

// Whether to enable forced enterprise re-enrollment on Flex.
inline constexpr char kEnterpriseEnableForcedReEnrollmentOnFlex[] =
    "enterprise-enable-forced-re-enrollment-on-flex";

// Whether to enable state determination.
inline constexpr char kEnterpriseEnableUnifiedStateDetermination[] =
    "enterprise-enable-state-determination";

// Disallow blocking developer mode through enterprise device policy:
// - Fail enterprise enrollment if enrolling would block dev mode.
// - Don't apply new device policy if it would block dev mode.
// This is only usable on test builds.
inline constexpr char kDisallowPolicyBlockDevMode[] =
    "disallow-policy-block-dev-mode";

// Ignore the profile creation time when determining whether to show the end of
// life notification incentive. This is meant to make manual testing easier.
inline constexpr char kEolIgnoreProfileCreationTime[] =
    "eol-ignore-profile-creation-time";

// Reset the end of life notification prefs to their default value, at the
// start of the user session. This is meant to make manual testing easier.
inline constexpr char kEolResetDismissedPrefs[] = "eol-reset-dismissed-prefs";

// Write extension install events to chrome log for integration test.
inline constexpr char kExtensionInstallEventChromeLogForTests[] =
    "extension-install-event-chrome-log-for-tests";

// Interval in seconds between Chrome reading external metrics from
// /var/lib/metrics/uma-events.
inline constexpr char kExternalMetricsCollectionInterval[] =
    "external-metrics-collection-interval";

// Name of a subdirectory of the main external web apps directory which
// additional web apps configs should be loaded from. Used to load
// device-specific web apps.
inline constexpr char kExtraWebAppsDir[] = "extra-web-apps-dir";

// Specifies number of recommended (fake) ARC apps during user onboarding.
// App descriptions are generated locally instead of being fetched from server.
// Limited to ChromeOS-on-linux and test images only.
inline constexpr char kFakeArcRecommendedAppsForTesting[] =
    "fake-arc-recommended-apps-for-testing";

// An absolute path to the chroot hosting the DriveFS to use. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
inline constexpr char kFakeDriveFsLauncherChrootPath[] =
    "fake-drivefs-launcher-chroot-path";

// A relative path to socket to communicat with the fake DriveFS launcher within
// the chroot specified by kFakeDriveFsLauncherChrootPath. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
inline constexpr char kFakeDriveFsLauncherSocketPath[] =
    "fake-drivefs-launcher-socket-path";

// Fingerprint sensor location indicates the physical sensor's location. The
// value is a string with possible values: "power-button-top-left",
// "keyboard-bottom-left", keyboard-bottom-right", "keyboard-top-right".
inline constexpr char kFingerprintSensorLocation[] =
    "fingerprint-sensor-location";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
inline constexpr char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Forces a fake backend to generate coral groups.
inline constexpr char kForceBirchFakeCoralBackend[] =
    "force-birch-fake-coral-backend";

// Forces a chip with fake coral group to be shown.
inline constexpr char kForceBirchFakeCoralGroup[] =
    "force-birch-fake-coral-group";

// Forces a fetch of Birch data whenever an informed restore session starts.
inline constexpr char kForceBirchFetch[] = "force-birch-fetch";

// If set, skips the logic in birch release notes provider and always sets
// release notes item.
inline constexpr char kForceBirchReleaseNotes[] = "force-birch-release-notes";

// Forces fetching tokens for Cryptohome Recovery.
inline constexpr char kForceCryptohomeRecoveryForTesting[] =
    "force-cryptohome-recovery-for-testing";

// If set, the developer tools are forced to be available.
inline constexpr char kForceDevToolsAvailable[] = "force-devtools-available";

// Forces first-run UI to be shown for every login.
inline constexpr char kForceFirstRunUI[] = "force-first-run-ui";

// Forces Hardware ID check (happens during OOBE) to fail or succeed. Possible
// values: "failure" or "success". Should be used only for testing.
inline constexpr char kForceHWIDCheckResultForTest[] =
    "force-hwid-check-result-for-test";

// Force enables the Happiness Tracking System for the device. This ignores
// user profile check and time limits and shows the notification every time
// for any type of user. Should be used only for testing.
inline constexpr char kForceHappinessTrackingSystem[] =
    "force-happiness-tracking-system";

// Forces FullRestoreService to launch browser for telemetry tests.
inline constexpr char kForceLaunchBrowser[] = "force-launch-browser";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
inline constexpr char kForceLoginManagerInTests[] =
    "force-login-manager-in-tests";

// Forces the cursor to be shown even if we are mimicking touch events. Note
// that cursor changes are locked when using this switch.
inline constexpr char kForceShowCursor[] = "force-show-cursor";

// Force the "release track" UI to show in the system tray. Simulates the system
// being on a non-stable release channel with feedback enabled.
inline constexpr char kForceShowReleaseTrack[] = "force-show-release-track";

// If set, tablet-like power button behavior (i.e. tapping the button turns the
// screen off) is used even if the device is in laptop mode.
inline constexpr char kForceTabletPowerButton[] = "force-tablet-power-button";

// Specifies the device's form factor. If provided, this flag overrides the
// value from the LSB release info. Possible values are: "CHROMEBASE",
// "CHROMEBIT", "CHROMEBOOK", "REFERENCE", "CHROMEBOX"
inline constexpr char kFormFactor[] = "form-factor";

// Specifies campaigns to override for testing.
inline constexpr char kGrowthCampaigns[] = "growth-campaigns";

// Clear all growth framework Feature Engagement events at session start for
// testing.
inline constexpr char kGrowthCampaignsClearEventsAtSessionStart[] =
    "growth-campaigns-clear-events-at-session-start";

// Path for which to load growth campaigns file for testing (instead of
// downloading from Omaha).
inline constexpr char kGrowthCampaignsPath[] = "growth-campaigns-path";

// Specifies the device current time in `SecondsSinceUnixEpoch` format for
// testing.
inline constexpr char kGrowthCampaignsCurrentTimeSecondsSinceUnixEpoch[] =
    "growth-campaigns-current-time";

// Specifies the device registered time in `SecondsSinceUnixEpoch` format for
// testing.
inline constexpr char kGrowthCampaignsRegisteredTimeSecondsSinceUnixEpoch[] =
    "growth-campaigns-registered-time";

// Specifies the delay time to trigger campaigns for testing.
inline constexpr char kGrowthCampaignsDelayedTriggerTimeInSecs[] =
    "growth-campaigns-delayed-trigger-time-in-secs";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
inline constexpr char kGuestSession[] = "bwsi";

// Large wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
inline constexpr char kGuestWallpaperLarge[] = "guest-wallpaper-large";

// Small wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
inline constexpr char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If set, the system is a Chromebook with a "standard Chrome OS keyboard",
// which generally means one with a Search key in the standard Caps Lock
// location above the Left Shift key. It should be unset for Chromebooks with
// both Search and Caps Lock keys (e.g. stout) and for devices like Chromeboxes
// that only use external keyboards.
inline constexpr char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

// Whether this device that has hps.
inline constexpr char kHasHps[] = "has-hps";

// Whether this device has an internal stylus.
inline constexpr char kHasInternalStylus[] = "has-internal-stylus";

// If set, the system is a Chromebook with a number pad as part of its internal
// keyboard.
inline constexpr char kHasNumberPad[] = "has-number-pad";

// Defines user homedir. This defaults to primary user homedir.
inline constexpr char kHomedir[] = "homedir";

// If set, the "ignore_dev_conf" field in StartArcVmRequest message will
// consequently be set such that all development configuration directives in
// /usr/local/vms/etc/arcvm_dev.conf will be ignored during ARCVM start.
inline constexpr char kIgnoreArcVmDevConf[] = "ignore-arcvm-dev-conf";

// If true, chrome would silently ignore unknown auth factor types
// instead of crashing.
inline constexpr char kIgnoreUnknownAuthFactors[] =
    "ignore-unknown-auth-factors";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
inline constexpr char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// Decreases delay in uploading installation event logs for integration test.
inline constexpr char kInstallLogFastUploadForTests[] =
    "install-log-fast-upload-for-tests";

// Minimum time the kiosk splash screen will be shown in seconds.
inline constexpr char kKioskSplashScreenMinTimeSeconds[] =
    "kiosk-splash-screen-min-time-seconds";

// Start Chrome in RMA mode. Launches RMA app automatically.
// kRmaNotAllowed switch takes priority over this one.
inline constexpr char kLaunchRma[] = "launch-rma";

// Enables the lobster feature.
inline constexpr char kLobsterFeatureKey[] = "lobster-feature-key";

// Enables Chrome-as-a-login-manager behavior.
inline constexpr char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
inline constexpr char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
inline constexpr char kLoginUser[] = "login-user";

// Specifies the user that the browser data migration should happen for.
inline constexpr char kBrowserDataMigrationForUser[] =
    "browser-data-migration-for-user";

// Run move migration instead of copy. Passed with
// `kBrowserDataMigrationForUser`.
inline constexpr char kBrowserDataMigrationMode[] =
    "browser-data-migration-mode";

// Specifies the user that the browser data backward migration should happen
// for.
inline constexpr char kBrowserDataBackwardMigrationForUser[] =
    "browser-data-backward-migration-for-user";

// Backward migration mode. Passed with `kBrowserDataBackwardMigrationForUser`.
inline constexpr char kBrowserDataBackwardMigrationMode[] =
    "browser-data-backward-migration-mode";

// Tells Chrome to forcefully trigger backward data migration.
inline constexpr char kForceBrowserDataBackwardMigration[] =
    "force-browser-data-backward-migration";

// Force skip or force migration. Should only be used for testing.
inline constexpr char kForceBrowserDataMigrationForTesting[] =
    "force-browser-data-migration-for-testing";

// Determines the URL to be used when calling the backend.
inline constexpr char kMarketingOptInUrl[] = "marketing-opt-in-url";

// Enables natural scroll by default.
inline constexpr char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// An optional comma-separated list of IDs of apps that can be used to take
// notes. If unset, a hardcoded list is used instead.
inline constexpr char kNoteTakingAppIds[] = "note-taking-app-ids";

// Disable metrics consent for testing.
inline constexpr char kOobeDisablePreConsentMetricsForTesting[] =
    "oobe-disable-pre-consent-metrics-for-testing";

// Allows the eula url to be overridden for tests.
inline constexpr char kOobeEulaUrlForTests[] = "oobe-eula-url-for-tests";

// Indicates that the first user run flow (sequence of OOBE screens after the
// first user login) should show tablet mode centric screens, even if the device
// is not in tablet mode.
inline constexpr char kOobeForceTabletFirstRun[] =
    "oobe-force-tablet-first-run";

// Indicates that OOBE should be scaled for big displays similar to how Meets
// app scales UI.
// TODO(crbug.com/1205364): Remove after adding new scheme.
inline constexpr char kOobeLargeScreenSpecialScaling[] =
    "oobe-large-screen-special-scaling";

// When present, prints the time it takes for OOBE's frontend to load.
// See go/oobe-frontend-trace-timings for details.
inline constexpr char kOobePrintFrontendLoadTimings[] =
    "oobe-print-frontend-load-timings";

// Specifies directory for screenshots taken with OOBE UI Debugger.
inline constexpr char kOobeScreenshotDirectory[] = "oobe-screenshot-dir";

// Shows a11y button on the marketing opt in without visiting gesture navigation
// screen.
inline constexpr char kOobeShowAccessibilityButtonOnMarketingOptInForTesting[] =
    "oobe-show-accessibility-button-on-marketing-opt-in-for-testing";

// Skips new user check in the personalized recommend apps screen for testing.
inline constexpr char kOobeSkipNewUserCheckForTesting[] =
    "oobe-skip-new-user-check-for-testing";

// Skips all other OOBE pages after user login.
inline constexpr char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Returns true if we should skip split modifier check on the split modifier
// info screen.
inline constexpr char kOobeSkipSplitModifierCheckForTesting[] =
    "oobe-skip-split-modifier-check-for-testing";

// Skip to login screen.
inline constexpr char kOobeSkipToLogin[] = "oobe-skip-to-login";

// Interval at which we check for total time on OOBE.
inline constexpr char kOobeTimerInterval[] = "oobe-timer-interval";

// Allows the timezone to be overridden on the marketing opt-in screen.
inline constexpr char kOobeTimezoneOverrideForTests[] =
    "oobe-timezone-override-for-tests";

// Trigger sync engine initialization timeout in OOBE for testing.
inline constexpr char kOobeTriggerSyncTimeoutForTests[] =
    "oobe-trigger-sync-timeout-for-tests";

// If set, the overview button will be visible.
inline constexpr char kOverviewButtonForTests[] = "overview-button-for-tests";

// If set, the overrides the overscan settings on all displays.
inline constexpr char kOverscanInsetsOverride[] = "overscan-insets-override";

// Controls how often the HiddenNetworkHandler class checks for wrongly hidden
// networks. The interval should be provided in seconds, should follow the
// format "--hidden-network-migration-interval=#", and should be >= 1.
inline constexpr char kHiddenNetworkMigrationInterval[] =
    "hidden-network-migration-interval";

// Sets how long a wrongly hidden network must have existed in order to be
// considered for removal. The interval should be provided in days, should
// follow the format "--hidden-network-migration-age=#", and should be >= 0.
inline constexpr char kHiddenNetworkMigrationAge[] =
    "hidden-network-migration-age";

// Sets the channel from which the PPD files are loaded.
inline constexpr char kPrintingPpdChannel[] = "printing-ppd-channel";

inline constexpr char kPrintingPpdChannelProduction[] = "production";

inline constexpr char kPrintingPpdChannelStaging[] = "staging";

inline constexpr char kPrintingPpdChannelDev[] = "dev";

inline constexpr char kPrintingPpdChannelLocalhost[] = "localhost";

// Sets Privacy Policy hostname url for testing.
inline constexpr char kPrivacyPolicyHostForTests[] =
    "privacy-policy-host-for-tests";

// If set to "true", the profile requires policy during restart (policy load
// must succeed, otherwise session restart should fail).
inline constexpr char kProfileRequiresPolicy[] = "profile-requires-policy";

// SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
// (e.g. for SAML managed guest sessions)
// TODO(crbug.com/40636049): Remove when URL is sent by DMServer.
inline constexpr char kPublicAccountsSamlAclUrl[] =
    "public-accounts-saml-acl-url";

// Adds fake Bluetooth devices to the quick settings menu for UI testing.
inline constexpr char kQsAddFakeBluetoothDevices[] =
    "qs-add-fake-bluetooth-devices";

// Adds fake Cast devices to the quick settings menu for UI testing.
inline constexpr char kQsAddFakeCastDevices[] = "qs-add-fake-cast-devices";

// Forces the quick settings "locale" FeatureTile to show. Normally it only
// shows in demo mode, which does not work in the emulator.
inline constexpr char kQsShowLocaleTile[] = "qs-show-locale-tile";

// The name of the per-model directory which contains per-region
// subdirectories with regulatory label files for this model.
// The per-model directories (if there are any) are located under
// "/usr/share/chromeos-assets/regulatory_labels/".
inline constexpr char kRegulatoryLabelDir[] = "regulatory-label-dir";

// Testing delay for reboot command. Useful for tast tests.
inline constexpr char kRemoteRebootCommandDelayInSecondsForTesting[] =
    "remote-reboot-command-timeout-in-seconds-for-testing";

// Indicates that reven UI strings and features should be shown.
inline constexpr char kRevenBranding[] = "reven-branding";

// The rlz ping delay (in seconds) that overwrites the default value.
inline constexpr char kRlzPingDelay[] = "rlz-ping-delay";

// Start Chrome without opening RMA or checking the current RMA state.
inline constexpr char kRmaNotAllowed[] = "rma-not-allowed";

// The switch added by session_manager daemon when chrome crashes 3 times or
// more within the first 60 seconds on start.
// See BrowserJob::ExportArgv in platform2/login_manager/browser_job.cc.
inline constexpr char kSafeMode[] = "safe-mode";

// Password change url for SAML users.
// TODO(crbug.com/40618074): Remove when the bug is fixed.
inline constexpr char kSamlPasswordChangeUrl[] = "saml-password-change-url";

// Testing grace period for DeviceScheduledReboot policy. Useful for tast tests.
// See `ShouldSkipRebootDueToGracePeriod` in scheduled_task_util.h.
inline constexpr char kScheduledRebootGracePeriodInSecondsForTesting[] =
    "scheduled-reboot-grace-period-in-seconds-for-testing";

inline constexpr char kSchedulerConfigurationConservative[] = "conservative";

// Specifies what the default scheduler configuration value is if the user does
// not set one.
inline constexpr char kSchedulerConfigurationDefault[] =
    "scheduler-configuration-default";

inline constexpr char kSchedulerConfigurationPerformance[] = "performance";

// Selects the scheduler configuration specified in the parameter.
inline constexpr char kSchedulerConfiguration[] = "scheduler-configuration";

// New modular design for the shelf with apps separated into a hotseat UI and
// smaller shelf in clamshell mode.
inline constexpr char kShelfHotseat[] = "shelf-hotseat";

// See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/ash_merge_session_loader_throttle.cc
// for details on this switch.
inline constexpr char kShortMergeSessionTimeoutForTest[] =
    "short-merge-session-timeout-for-test";

// If true, the developer tool overlay will be shown for the login/lock screen.
// This makes it easier to test layout logic.
inline constexpr char kShowLoginDevOverlay[] = "show-login-dev-overlay";

// Enables OOBE UI Debugger for ease of navigation between screens during manual
// testing. Limited to ChromeOS-on-linux and test images only.
inline constexpr char kShowOobeDevOverlay[] = "show-oobe-dev-overlay";

// Enables the QuickStart debugger in OOBE which mimics an Android phone.
inline constexpr char kShowOobeQuickStartDebugger[] =
    "show-oobe-quick-start-debugger";

// Draws a circle at each touch point, similar to the Android OS developer
// option "Show taps".
inline constexpr char kShowTaps[] = "show-taps";

// Disables online sign-in enforcement in tast tests.
inline constexpr char kSkipForceOnlineSignInForTesting[] =
    "skip-force-online-signin-for-testing";

// Skip multidevice setup screen during tast tests.
inline constexpr char kSkipMultideviceScreenForTesting[] =
    "skip-multidevice-screen";

// Used to skip the threshold duration that the reorder nudge has to show before
// the nudge is considered as shown.
inline constexpr char kSkipReorderNudgeShowThresholdDurationForTest[] =
    "skip-reorder-nudge-show-threshold-duration";

// If true, the time dependent views (such as the time view) show with the
// predefined fixed time.
inline constexpr char kStabilizeTimeDependentViewForTests[] =
    "stabilize-time-dependent-view-for-tests";

// If set, the device will be forced to stay in clamshell UI mode but screen
// auto rotation will be supported. E.g, chromebase device Dooly.
inline constexpr char kSupportsClamshellAutoRotation[] =
    "supports-clamshell-auto-rotation";

// Hides all Message Center notification popups (toasts). Used for testing.
inline constexpr char kSuppressMessageCenterPopups[] =
    "suppress-message-center-popups";

// Specifies directory for the Telemetry System Web Extension.
inline constexpr char kTelemetryExtensionDirectory[] =
    "telemetry-extension-dir";

// TODO(b/299642185): Remove this flag by the end of 2023.
// ChromeOS does not support empty passwords for users, but some legacy test
// setups might use empty password for users.
inline constexpr char kTemporaryAllowEmptyPasswordsInTests[] =
    "allow-empty-passwords-in-tests";

// Enables testing for encryption migration UI.
inline constexpr char kTestEncryptionMigrationUI[] =
    "test-encryption-migration-ui";

// Passes the name of the current running automated test to Chrome.
inline constexpr char kTestName[] = "test-name";

// Enables the wallpaper picker to fetch images from the test server.
inline constexpr char kTestWallpaperServer[] = "test-wallpaper-server";

// Tells the Chromebook to scan for a tethering host even if there is already a
// wired connection. This allows end-to-end tests to be deployed over ethernet
// without that connection preventing scans and thereby blocking the testing of
// cases with no preexisting connection. Should be used only for testing.
inline constexpr char kTetherHostScansIgnoreWiredConnections[] =
    "tether-host-scans-ignore-wired-connections";

// Overrides Tether with stub service. Provide integer arguments for the number
// of fake networks desired, e.g. 'tether-stub=2'.
inline constexpr char kTetherStub[] = "tether-stub";

// Used for overriding the required user activity time before running the
// onboarding survey.
inline constexpr char kTimeBeforeOnboardingSurveyInSecondsForTesting[] =
    "time-before-onboarding-survey-in-seconds-for-testing";

// Chromebases' touchscreens can be used to wake from suspend, unlike the
// touchscreens on other Chrome OS devices. If set, the touchscreen is kept
// enabled while the screen is off so that it can be used to turn the screen
// back on after it has been turned off for inactivity but before the system has
// suspended.
inline constexpr char kTouchscreenUsableWhileScreenOff[] =
    "touchscreen-usable-while-screen-off";

// Enables TPM selection in runtime.
inline constexpr char kTpmIsDynamic[] = "tpm-is-dynamic";

// Shows all Bluetooth devices in UI (System Tray/Settings Page.)
inline constexpr char kUnfilteredBluetoothDevices[] =
    "unfiltered-bluetooth-devices";

// If this switch is passed, the device policy DeviceMinimumVersion
// assumes that the device has reached Auto Update Expiration. This is useful
// for testing the policy behaviour on the DUT.
inline constexpr char kUpdateRequiredAueForTest[] =
    "aue-reached-for-update-required-test";

// Use the fake FakeCrasAudioClient to handle system audio controls.
inline constexpr char kUseFakeCrasAudioClientForDBus[] =
    "use-fake-cras-audio-client-for-dbus";

// Flag that stored MyFiles folder inside the user data directory.
// $HOME/Downloads is used as MyFiles folder for ease access to local files for
// debugging when running on Linux. By setting this flag, <cryptohome>/MyFiles
// is used even on Linux.
inline constexpr char kUseMyFilesInUserDataDirForTesting[] =
    "use-myfiles-in-user-data-dir-for-testing";

// If provided, any webui will be loaded from <flag value>/<handler_name>, where
// handler_name is the name passed to MaybeConfigureTestableDataSource, if the
// file exists.
// For example, if the flag is /tmp/resource_overrides, attempting to load
// js/app_main.js from the data source named "help_app/untrusted" will first
// attempt to load from /tmp/resource_overrides/help_app/untrusted/js/main.js.
inline constexpr char kWebUiDataSourcePathForTesting[] =
    "web-ui-data-source-path-for-testing";

// Enable the getAccessToken autotest API which creates access tokens using
// the internal OAuth client ID.
inline constexpr char kGetAccessTokenForTest[] = "get-access-token-for-test";

// Prevent kiosk autolaunch for testing.
inline constexpr char kPreventKioskAutolaunchForTesting[] =
    "prevent-kiosk-autolaunch-for-testing";

// Allows the Ash shelf to apply the default pin layout without waiting for Sync
// to download data from the server (which many tests can't achieve).
inline constexpr char kAllowDefaultShelfPinLayoutIgnoringSync[] =
    "ash-allow-default-shelf-pin-layout-ignoring-sync";

// On devices that support refresh rate throttling, force the throttling
// behavior to be active regardless of system state.
inline constexpr char kForceRefreshRateThrottle[] =
    "force-refresh-rate-throttle";

// Value of GAIA auth code for --force-app-mode.
inline constexpr char kAppModeAuthCode[] = "app-mode-auth-code";

// Value of OAuth2 refresh token for --force-app-mode.
inline constexpr char kAppModeOAuth2Token[] = "app-mode-oauth-token";

// Allows setting a different destination ID for connection-monitoring GCM
// messages. Useful when running against a non-prod management server.
inline constexpr char kMonitoringDestinationID[] = "monitoring-destination-id";

// Frequency in Milliseconds for system log uploads. Should only be used for
// testing purposes.
inline constexpr char kSystemLogUploadFrequency[] =
    "system-log-upload-frequency";

// When specified with a url string as parameter, the given url overrides the
// Android Messages for Web PWA installation and app urls using a base of the
// given domain with approrpiate suffixes.
inline constexpr char kCustomAndroidMessagesDomain[] =
    "custom-android-messages-domain";

// Enables verbose logging level for Nearby Share.
inline constexpr char kNearbyShareVerboseLogging[] =
    "nearby-share-verbose-logging";

////////////////////////////////////////////////////////////////////////////////

// Returns true if flag if AuthSession should be used to communicate with
// cryptohomed instead of explicitly authorizing each operation.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAuthSessionCryptohomeEnabled();

// Returns true if this is a Cellular First device.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCellularFirstDevice();

// Returns true if this is reven board.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsRevenBranding();

// Returns true if the Chromebook should ignore its wired connections when
// deciding whether to run scans for tethering hosts. Should be used only for
// testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldTetherHostScansIgnoreWiredConnections();

// Returns true if we should skip new user check on the recommend apps screen.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldSkipNewUserCheckForTesting();

// Returns true if we should skip all other OOBE pages after user login.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldSkipOobePostLogin();

// Returns true if we should disable pre-consent metrics for testing. Consent
// won't be enabled by CrosPreConsentMetricsManager.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldDisablePreConsentMetricsForTesting();

// Returns true if we should skip split modifier check on the split modifier
// info screen.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldSkipSplitModifierCheckForTesting();

// Returns true if we should show a11y button on the marketing opt in screen.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldShowAccessibilityButtonOnMarketingOptInForTesting();

// Returns true if the device is of tablet form factor.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTabletFormFactor();

// Returns true if GAIA services has been disabled.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaServicesDisabled();

// Returns true if we should skip MultideviceSetup screen.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldMultideviceScreenBeSkippedForTesting();

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

// Returns true if the OOBE network setup is skipped even if there is no
// internet connection.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBENetworkSetupSkippedForTesting();

// Returns true if the OOBE Network screen skipping check based on ethernet
// connection is disabled for testing.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBENetworkScreenSkippingDisabledForTesting();

// Returns true if empty passwords can be used by automated tests.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool AreEmptyPasswordsAllowedForForTesting();

// Returns true if the OOBE ChromeVox hint is enabled for dev mode.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsOOBEChromeVoxHintEnabledForDevMode();

// Returns true if the overview button is set to be always visible. Mostly for
// dev purpose.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOverviewButtonEnabledForTests();

// Returns true if the OEM Device Requisition can be configured.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsDeviceRequisitionConfigurable();

// Returns true if the OS installation UI flow can be entered.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOsInstallAllowed();

COMPONENT_EXPORT(ASH_CONSTANTS)
std::optional<base::TimeDelta> ContextualNudgesInterval();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ContextualNudgesResetShownCount();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUsingShelfAutoDim();

// Returns whether the device has hps hardware.
COMPONENT_EXPORT(ASH_CONSTANTS) bool HasHps();

// Returns true if the duration threshold for considering the nudge to be
// shown is skipped.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSkipRecorderNudgeShowThresholdDurationEnabled();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsStabilizeTimeDependentViewForTestsEnabled();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool UseFakeCrasAudioClientForDBus();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldAllowDefaultShelfPinLayoutIgnoringSync();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsCampbellSecretKeyMatched();

COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsScannerUpdateSecretKeyMatched();

COMPONENT_EXPORT(ASH_CONSTANTS)
base::AutoReset<bool> SetIgnoreScannerUpdateSecretKeyForTest();

// Returns true if per-user timezone preferences are enabled.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsPerUserTimezoneEnabled();

// Returns true if fine-grained time zone detection is enabled.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsFineGrainedTimeZoneDetectionEnabled();

}  // namespace ash::switches

#endif  // ASH_CONSTANTS_ASH_SWITCHES_H_
