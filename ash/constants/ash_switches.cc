// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"

#include <algorithm>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace ash::switches {

namespace {

// Max and min number of seconds that must pass between showing user contextual
// nudges when override switch is set.
constexpr base::TimeDelta kAshContextualNudgesMinInterval = base::Seconds(0);
constexpr base::TimeDelta kAshContextualNudgesMaxInterval = base::Seconds(60);

// The hash value for the secret key of the campbell feature.
constexpr char kCampbellHashKey[] =
    "\x78\xb6\xa7\x59\x06\x11\xc7\xea\x09\x7e\x92\xe3\xe9\xff\xa6\x01\x4c"
    "\x03\x18\x32";

// The hash value for the secret key of the conch feature.
constexpr char kConchHashKey[] =
    "\x55\x40\xce\x6c\x95\x34\xae\x33\x4c\x82\x20\xa3\x86\xdb\xbc\xc5\x4d\x49"
    "\x38\xf0";

// The hash value for the secret key of the mahi feature.
constexpr char kMahiHashKey[] =
    "\xFE\x34\x22\x3F\xEA\x73\xC2\xD5\xA6\xE8\x82\x0B\xF3\x67\x7D\x01\xA3\x6F"
    "\x3A\xFF";

// Whether checking the mahi secret key is ignored.
bool g_ignore_mahi_secret_key = false;

// The hash value for the secret key of the mahi feature.
constexpr char kModifierSplitHashKey[] =
    "\xFC\xEF\x09\x7D\x01\x39\x86\x6A\x57\x08\x7C\x22\x5F\x1C\xEF\x8A\x3B\x7E"
    "\x10\x99";

// Whether checking the mahi secret key is ignored.
bool g_ignore_modifier_split_secret_key = true;

// The hash value for the secret key of the sparky feature.
constexpr char kSparkyHashKey[] =
    "\x3b\xcc\x52\x86\xf0\x4d\xfd\xd2\xcf\xd7\x05\xe0\xcc\x97\x95\xfd\x8a\x78"
    "\x44\x77";

// The hash value for the secret key of the Scanner feature update.
constexpr std::string_view kScannerUpdateHashKey(
    "\xF0\xC9\xFD\x45\x31\x92\x95\xAC\xBB\xD8\xD4\xB3\x5F\xF8\x98\x3B\x3B\x4F"
    "\x02\xF1",
    base::kSHA1Length);

// Whether checking the Scanner update secret key is ignored.
bool g_ignore_scanner_update_secret_key = false;

}  // namespace

// Please keep the order of these switches synchronized with the header file
// (i.e. in alphabetical order).

const char kAggressiveCacheDiscardThreshold[] = "aggressive-cache-discard";

// If this flag is passed, failed policy fetches will not cause profile
// initialization to fail. This is useful for tests because it means that
// tests don't have to mock out the policy infrastructure.
const char kAllowFailedPolicyFetchForTest[] =
    "allow-failed-policy-fetch-for-test";

// When this flag is set, the OS installation UI can be accessed. This
// allows the user to install from USB to disk.
const char kAllowOsInstall[] = "allow-os-install";

// Override for the URL used for the ChromeOS Almanac API. Used for local
// testing with a non-production server (e.g.
// "--almanac-api-url=http://localhost:8000").
const char kAlmanacApiUrl[] = "almanac-api-url";

// Causes HDCP of the specified type to always be enabled when an external
// display is connected. Used for HDCP compliance testing on ChromeOS.
const char kAlwaysEnableHdcp[] = "always-enable-hdcp";

// Specifies whether an app launched in kiosk mode was auto launched with zero
// delay. Used in order to properly restore auto-launched state during session
// restore flow.
const char kAppAutoLaunched[] = "app-auto-launched";

// Path for app's OEM manifest file.
const char kAppOemManifestFile[] = "app-mode-oem-manifest";

// Signals ARC support status on this device. This can take one of the
// following three values.
// - none: ARC is not installed on this device. (default)
// - installed: ARC is installed on this device, but not officially supported.
//   Users can enable ARC only when Finch experiment is turned on.
// - officially-supported: ARC is installed and supported on this device. So
//   users can enable ARC via settings etc.
const char kArcAvailability[] = "arc-availability";

// DEPRECATED: Please use --arc-availability=installed.
// Signals the availability of the ARC instance on this device.
const char kArcAvailable[] = "arc-available";

// Switch that blocks KeyMint. When KeyMint is blocked, Keymaster is enabled.
const char kArcBlockKeyMint[] = "arc-block-keymint";

// Flag that forces ARC data be cleaned on each start.
const char kArcDataCleanupOnStart[] = "arc-data-cleanup-on-start";

// Flag that disables ARC app sync flow that installs some apps silently. Used
// in autotests to resolve racy conditions.
const char kArcDisableAppSync[] = "arc-disable-app-sync";

// Used in tests to disable DexOpt cache which is on by default.
const char kArcDisableDexOptCache[] = "arc-disable-dexopt-cache";

// Flag that disables ARC download provider that prevents extra content to be
// downloaded and installed in context of Play Store and GMS Core.
const char kArcDisableDownloadProvider[] = "arc-disable-download-provider";

// Used in autotest to disable GMS-core caches which is on by default.
const char kArcDisableGmsCoreCache[] = "arc-disable-gms-core-cache";

// Flag that disables ARC locale sync with Android Container. Used in autotest
// to prevent conditions when certain apps, including Play Store may get
// restarted. Restarting Play Store may cause random test failures. Enabling
// this flag would also forces ARC Container to use 'en-US' as a locale and
// 'en-US,en' as preferred languages.
const char kArcDisableLocaleSync[] = "arc-disable-locale-sync";

// Used to disable GMS scheduling of media store periodic indexing and corpora
// maintenance tasks. Used in performance tests to prevent running during
// testing which can cause unstable results or CPU not idle pre-test failures.
const char kArcDisableMediaStoreMaintenance[] =
    "arc-disable-media-store-maintenance";

// Flag that disables ARC Play Auto Install flow that installs set of predefined
// apps silently. Used in autotests to resolve racy conditions.
const char kArcDisablePlayAutoInstall[] = "arc-disable-play-auto-install";

// Used in autotest to disable TTS cache which is on by default.
const char kArcDisableTtsCache[] = "arc-disable-tts-cache";

// Flag that indicates ARC is using dev caches generated by data collector in
// Uprev rather than caches from CrOS build stage for arccachesetup service.
const char kArcUseDevCaches[] = "arc-use-dev-caches";

// Flag that indicates ARC images are formatted with EROFS (go/arcvm-erofs).
const char kArcErofs[] = "arc-erofs";

// Flag that forces Android volumes (DocumentsProviders and Play files) to be
// mounted in the Files app. Used for testing.
const char kArcForceMountAndroidVolumesInFiles[] =
    "arc-force-mount-android-volumes-in-files";

// Flag that forces the OptIn ui to be shown. Used in tests.
const char kArcForceShowOptInUi[] = "arc-force-show-optin-ui";

// Flag that enables developer options needed to generate an ARC Play Auto
// Install roster. Used manually by developers.
const char kArcGeneratePlayAutoInstall[] = "arc-generate-play-auto-install";

// Sets the mode of operation for ureadahead during ARC Container boot.
// readahead (default) - used during production and is equivalent to no switch
//                       being set.
// generate - used during Android Uprev data collector to pre-generate pack file
//            and upload to Google Cloud as build artifact for CrOS build image.
// disabled - used for test purpose to disable ureadahead during ARC Container
// boot.
const char kArcHostUreadaheadMode[] = "arc-host-ureadahead-mode";

// Write ARC++ install events to chrome log for integration test.
const char kArcInstallEventChromeLogForTests[] =
    "arc-install-event-chrome-log-for-tests";

// Used in autotest to specifies how to handle packages cache. Can be
// copy - copy resulting packages.xml to the temporary directory.
// skip-copy - skip initial packages cache setup and copy resulting packages.xml
//             to the temporary directory.
const char kArcPackagesCacheMode[] = "arc-packages-cache-mode";

// Used in autotest to forces Play Store auto-update state. Can be
// on - auto-update is forced on.
// off - auto-update is forced off.
const char kArcPlayStoreAutoUpdate[] = "arc-play-store-auto-update";

// Set the scale for ARC apps. This is in DPI. e.g. 280 DPI is ~ 1.75 device
// scale factor.
// See
// https://source.android.com/compatibility/android-cdd#3_7_runtime_compatibility
// for list of supported DPI values.
const char kArcScale[] = "arc-scale";

// Defines how to start ARC. This can take one of the following values:
// - always-start automatically start with Play Store UI support.
// - always-start-with-no-play-store automatically start without Play Store UI.
// If it is not set, then ARC is started in default mode.
const char kArcStartMode[] = "arc-start-mode";

// Sets ARC Terms Of Service hostname url for testing.
const char kArcTosHostForTests[] = "arc-tos-host-for-tests";

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
const char kArcVmUreadaheadMode[] = "arcvm-ureadahead-mode";

// Madvises the kernel to use Huge Pages for guest memory.
const char kArcVmUseHugePages[] = "arcvm-use-hugepages";

// Clear the fast ink buffer upon creation. This is needed on some devices that
// do not zero out new buffers.
const char kAshClearFastInkBuffer[] = "ash-clear-fast-ink-buffer";

// Force the pointer (cursor) position to be kept inside root windows.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Overrides the minimum time that must pass between showing user contextual
// nudges. Unit of time is in seconds.
const char kAshContextualNudgesInterval[] = "ash-contextual-nudges-interval";

// Reset contextual nudge shown count on login.
const char kAshContextualNudgesResetShownCount[] =
    "ash-contextual-nudges-reset-shown-count";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enable keyboard shortcuts used by developers only.
const char kAshDeveloperShortcuts[] = "ash-dev-shortcuts";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the palette on every display, instead of only the internal one.
const char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// If the flag is present, it indicates 1) the device has accelerometer and 2)
// the device is a convertible device or a tablet device (thus is capable of
// entering tablet mode). If this flag is not set, then the device is not
// capable of entering tablet mode. For example, Samus has accelerometer, but
// is not a covertible or tablet, thus doesn't have this flag set, thus can't
// enter tablet mode.
const char kAshEnableTabletMode[] = "enable-touchview";

// Enable the wayland server.
const char kAshEnableWaylandServer[] = "enable-wayland-server";

// Enables the stylus tools next to the status area.
const char kAshForceEnableStylusTools[] = "force-enable-stylus-tools";

// Forces the status area to allow collapse/expand regardless of the current
// state.
const char kAshForceStatusAreaCollapsible[] = "force-status-area-collapsible";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
const char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Hides educational nudges that can interfere with tast integration tests.
// Somewhat similar to --no-first-run but affects system UI behavior, not
// browser behavior.
const char kAshNoNudges[] = "ash-no-nudges";

// Power button position includes the power button's physical display side and
// the percentage for power button center position to the display's
// width/height in landscape_primary screen orientation. The value is a JSON
// object containing a "position" property with the value "left", "right",
// "top", or "bottom". For "left" and "right", a "y" property specifies the
// button's center position as a fraction of the display's height (in [0.0,
// 1.0]) relative to the top of the display. For "top" and "bottom", an "x"
// property gives the position as a fraction of the display's width relative to
// the left side of the display.
const char kAshPowerButtonPosition[] = "ash-power-button-position";

// The physical position info of the side volume button while in landscape
// primary screen orientation. The value is a JSON object containing a "region"
// property with the value "keyboard", "screen" and a "side" property with the
// value "left", "right", "top", "bottom".
const char kAshSideVolumeButtonPosition[] = "ash-side-volume-button-position";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// Enables required things for the selected UI mode, regardless of whether the
// Chromebook is currently in the selected UI mode.
const char kAshUiMode[] = "force-tablet-mode";

// Values for the kAshUiMode flag.
const char kAshUiModeClamshell[] = "clamshell";
const char kAshUiModeTablet[] = "touch_view";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Sets the birch ranker to assume it is evening for birch chip ranking
// purposes.
const char kBirchIsEvening[] = "birch-is-evening";

// Sets the birch ranker to assume it is morning for birch chip ranking
// purposes.
const char kBirchIsMorning[] = "birch-is-morning";

// Switch used to pass in a secret key for Campbell feature. Unless the correct
// secret key is provided, Campbell feature will remain disabled, regardless of
// the state of the associated feature flag.
const char kCampbellKey[] = "campbell-key";

// If this flag is set, it indicates that this device is a "Cellular First"
// device. Cellular First devices use cellular telephone data networks as
// their primary means of connecting to the internet.
// Setting this flag has two consequences:
// 1. Cellular data roaming will be enabled by default.
// 2. UpdateEngine will be instructed to allow auto-updating over cellular
//    data connections.
const char kCellularFirst[] = "cellular-first";

// Default large wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperLarge[] = "child-wallpaper-large";

// Default small wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperSmall[] = "child-wallpaper-small";

// Switch used to pass in a secret key for Conch. Unless the correct secret key
// is provided, Conch feature will remain disabled, regardless of the state of
// the associated feature flag.
const char kConchKey[] = "conch-key";

// Forces CrOS region value.
const char kCrosRegion[] = "cros-region";

// Overrides the base url for the Cryptohome recovery service.
const char kCryptohomeRecoveryServiceBaseUrl[] =
    "cryptohome-recovery-service-base-url";

// Forces cryptohome recovery process to use test environment (test keys /
// URLs).
const char kCryptohomeRecoveryUseTestEnvironment[] =
    "cryptohome-recovery-use-test-env";

// Controls if AuthSession API should be used when interacting with cryptohomed.
const char kCryptohomeUseAuthSession[] = "cryptohome-use-authsession";

// Forces cryptohome to create new users using old (ecryptfs) encryption.
// This switch can be used to set up configurations that can be used to
// test encryption migration scenarios.
const char kCryptohomeUseOldEncryptionForTesting[] =
    "cryptohome-use-old-encryption-for-testing";

// Normally the cryptohome without any any authentication factors
// is considered corrupted. Special mechanism would detect such situation
// during user creation and remove such users. If such user is an owner
// the power wash should be triggered instead. However, if such event happens
// in tests, all logs would be lost, and it would be difficult to investigate
// exact reason behind the Owner user being misconfigured.
// This flag prevents triggering powerwash in such cases, simple user removal
// would be triggered instead.
const char kCryptohomeIgnoreCleanupOwnershipForTesting[] =
    "cryptohome-ignore-cleanup-ownership-for-testing";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kDefaultWallpaperIsOem[] = "default-wallpaper-is-oem";

// Default large wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperLarge[] = "default-wallpaper-large";

// Default small wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperSmall[] = "default-wallpaper-small";

// Interval in seconds to wait for a display to reconnect while unlocking or
// logging in with a closed lid.
const char kDeferExternalDisplayTimeout[] = "defer-external-display-timeout";

// Test Organization Unit (OU) user to use for demo mode. Only pass the part
// before "@cros-demo-mode.com".
const char kDemoModeEnrollingUsername[] = "demo-mode-enrolling-username";

// Force ARC provision to take code path for offline demo mode.
const char kDemoModeForceArcOfflineProvision[] =
    "demo-mode-force-arc-offline-provision";

// App ID to use for highlights app in demo mode.
const char kDemoModeHighlightsApp[] = "demo-mode-highlights-extension";

// App ID to use for screensaver app in demo mode.
const char kDemoModeScreensaverApp[] = "demo-mode-screensaver-extension";

// Directory from which to fetch the demo mode SWA content (instead of
// downloading from Omaha).
const char kDemoModeSwaContentDirectory[] = "demo-mode-swa-content-directory";

// Directory from which to fetch the demo mode resource content (instead of
// downloading from Omaha).
const char kDemoModeResourceDirectory[] = "demo-mode-resource-directory";

// Time in seconds before a machine at OOBE is considered derelict.
const char kDerelictDetectionTimeout[] = "derelict-detection-timeout";

// Time in seconds before a derelict machines starts demo mode.
const char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Prevents any CPU restrictions being set on ARC[VM]. Only meant to be used by
// tests as some tests may time out if the ARC container is throttled.
const char kDisableArcCpuRestriction[] = "disable-arc-cpu-restriction";

// Disables ARC Opt-in verification process and ARC is enabled by default.
const char kDisableArcOptInVerification[] = "disable-arc-opt-in-verification";

// Disables the Weather API from being called by Birch. Allows fake users in
// tast tests to avoid making API calls using an invalid GAIA ID, which causes
// errors on the weather server side.
const char kDisableBirchWeatherApiForTesting[] =
    "disable-birch-weather-api-for-testing";

// Disables the Chrome OS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// If this switch is set, the device cannot be remotely disabled by its owner.
const char kDisableDeviceDisabling[] = "disable-device-disabling";

// Disables DriveFS for testing purposes, used in tast testing and only on test
// images.
const char kDisableDriveFsForTesting[] = "disable-drive-fs-for-testing";

// Disables fine grained time zone detection.
const char kDisableFineGrainedTimeZoneDetection[] =
    "disable-fine-grained-time-zone-detection";

// Disables first-run UI from being shown.
const char kDisableFirstRunUI[] = "disable-first-run-ui";

// Disables GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Disables HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBEForTesting[] =
    "disable-hid-detection-on-oobe";

// Disables the Lacros keep alive for testing.
const char kDisableLacrosKeepAliveForTesting[] = "disable-lacros-keep-alive";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// If Lacros is set to the primary web browser, on session login, it is
// automatically launched. This disables the feature, i.e., if this flag is
// set, even if lacros is the primary web browser, it won't automatically
// launch on session login. This is for testing purpose, specifically for Tast.
const char kDisableLoginLacrosOpening[] = "disable-login-lacros-opening";

// Disables requests for an enterprise machine certificate during attestation.
const char kDisableMachineCertRequest[] = "disable-machine-cert-request";

// Disables the ChromeVox hint idle detection in OOBE, which can lead to
// unexpected behavior during tests.
const char kDisableOOBEChromeVoxHintTimerForTesting[] =
    "disable-oobe-chromevox-hint-timer-for-testing";

// Disables network screen skip check which is based on ethernet connection.
const char kDisableOOBENetworkScreenSkippingForTesting[] =
    "disable-oobe-network-screen-skipping-for-testing";

// Disables per-user timezone.
const char kDisablePerUserTimezone[] = "disable-per-user-timezone";

// Disables rollback option on reset screen.
const char kDisableRollbackOption[] = "disable-rollback-option";

// Disables client certificate authentication on the sign-in frame on the Chrome
// OS sign-in profile.
// TODO(crbug.com/41389560): Remove this flag when reaching endpoints that
// request client certs does not hang anymore when there is no system token yet.
const char kDisableSigninFrameClientCerts[] =
    "disable-signin-frame-client-certs";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// DEPRECATED. Please use --arc-availability=officially-supported.
// Enables starting the ARC instance upon session start.
const char kEnableArc[] = "enable-arc";

// Enables ARCVM.
const char kEnableArcVm[] = "enable-arcvm";

// Enables ARCVM DLC.
const char kEnableArcVmDlc[] = "enable-arcvm-dlc";

// Enables ARCVM realtime VCPU feature.
const char kEnableArcVmRtVcpu[] = "enable-arcvm-rt-vcpu";

// Adds ash-browser back to launcher, even if in LacrosOnly mode.
const char kEnableAshDebugBrowser[] = "enable-ash-debug-browser";

// Used to override `kDisableBirchWeatherApiForTesting` for specific tast tests.
const char kEnableBirchWeatherApiForTestingOverride[] =
    "enable-birch-weather-api-for-testing-override";

// Enables the Cast Receiver.
const char kEnableCastReceiver[] = "enable-cast-receiver";

// Enables Shelf Dimming for ChromeOS.
const char kEnableDimShelf[] = "enable-dim-shelf";

// Enables sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[] = "enable-extension-assets-sharing";

// Enables the use of 32-bit Houdini library for ARM binary translation.
const char kEnableHoudini[] = "enable-houdini";

// Enables the use of 64-bit Houdini library for ARM binary translation.
const char kEnableHoudini64[] = "enable-houdini64";

// Enables the use of Houdini DLC library for ARM binary translation. This is
// independent of choosing between the 32-bit vs 64-bit Houdini library. Houdini
// DLC library will be downloaded and installed at run-time instead of at build
// time.
const char kEnableHoudiniDlc[] = "enable-houdini-dlc";

// Enables the use of 32-bit NDK translation library for ARM binary translation.
const char kEnableNdkTranslation[] = "enable-ndk-translation";

// Enables the use of 64-bit NDK translation library for ARM binary translation.
const char kEnableNdkTranslation64[] = "enable-ndk-translation64";

// Enables the ChromeVox hint in OOBE for dev mode. This flag is used
// to override the default dev mode behavior of disabling the feature.
// If both kEnableOOBEChromeVoxHintForDevMode and
// kDisableOOBEChromeVoxHintTimerForTesting are present, the ChromeVox hint
// will be disabled, since the latter flag takes precedence over the former.
const char kEnableOOBEChromeVoxHintForDevMode[] =
    "enable-oobe-chromevox-hint-timer-for-dev-mode";

// Enables OOBE testing API for tast tests.
const char kEnableOobeTestAPI[] = "enable-oobe-test-api";

// Enables configuring the OEM Device Requisition in the OOBE.
const char kEnableRequisitionEdits[] = "enable-requisition-edits";

// Enables tablet form factor.
const char kEnableTabletFormFactor[] = "enable-tablet-form-factor";

// Enables the touch calibration option in MD settings UI for valid touch
// displays.
const char kEnableTouchCalibrationSetting[] =
    "enable-touch-calibration-setting";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[] =
    "enable-touchpad-three-finger-click";

// Disables ARC for managed accounts.
const char kEnterpriseDisableArc[] = "enterprise-disable-arc";

// Whether to force manual enrollment instead of trying cert based enrollment.
// Only works on test builds.
const char kEnterpriseForceManualEnrollmentInTestBuilds[] =
    "enterprise-force-manual-enrollment-in-test-builds";

// Whether to enable unified state determination.
const char kEnterpriseEnableUnifiedStateDetermination[] =
    "enterprise-enable-unified-state-determination";

// Whether to enable forced enterprise re-enrollment.
const char kEnterpriseEnableForcedReEnrollment[] =
    "enterprise-enable-forced-re-enrollment";

// Whether to enable forced enterprise re-enrollment on Flex.
const char kEnterpriseEnableForcedReEnrollmentOnFlex[] =
    "enterprise-enable-forced-re-enrollment-on-flex";

// Whether to enable initial enterprise enrollment.
const char kEnterpriseEnableInitialEnrollment[] =
    "enterprise-enable-initial-enrollment";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Disallow blocking developer mode through enterprise device policy:
// - Fail enterprise enrollment if enrolling would block dev mode.
// - Don't apply new device policy if it would block dev mode.
// This is only usable on test builds.
const char kDisallowPolicyBlockDevMode[] = "disallow-policy-block-dev-mode";

// Ignore the profile creation time when determining whether to show the end of
// life notification incentive. This is meant to make manual testing easier.
const char kEolIgnoreProfileCreationTime[] = "eol-ignore-profile-creation-time";

// Reset the end of life notification prefs to their default value, at the
// start of the user session. This is meant to make manual testing easier.
const char kEolResetDismissedPrefs[] = "eol-reset-dismissed-prefs";

// Write extension install events to chrome log for integration test.
const char kExtensionInstallEventChromeLogForTests[] =
    "extension-install-event-chrome-log-for-tests";

// Interval in seconds between Chrome reading external metrics from
// /var/lib/metrics/uma-events.
const char kExternalMetricsCollectionInterval[] =
    "external-metrics-collection-interval";

// Name of a subdirectory of the main external web apps directory which
// additional web apps configs should be loaded from. Used to load
// device-specific web apps.
const char kExtraWebAppsDir[] = "extra-web-apps-dir";

// Specifies number of recommended (fake) ARC apps during user onboarding.
// App descriptions are generated locally instead of being fetched from server.
// Limited to ChromeOS-on-linux and test images only.
const char kFakeArcRecommendedAppsForTesting[] =
    "fake-arc-recommended-apps-for-testing";

// An absolute path to the chroot hosting the DriveFS to use. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherChrootPath[] =
    "fake-drivefs-launcher-chroot-path";

// A relative path to socket to communicat with the fake DriveFS launcher within
// the chroot specified by kFakeDriveFsLauncherChrootPath. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherSocketPath[] =
    "fake-drivefs-launcher-socket-path";

// Fingerprint sensor location indicates the physical sensor's location. The
// value is a string with possible values: "power-button-top-left",
// "keyboard-bottom-left", keyboard-bottom-right", "keyboard-top-right".
const char kFingerprintSensorLocation[] = "fingerprint-sensor-location";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Forces a chip with fake coral data to be shown.
const char kForceBirchFakeCoral[] = "force-birch-fake-coral";

// Forces a fetch of Birch data whenever an informed restore session starts.
const char kForceBirchFetch[] = "force-birch-fetch";

// If set, skips the logic in birch release notes provider and always sets
// release notes item.
const char kForceBirchReleaseNotes[] = "force-birch-release-notes";

// Forces fetching tokens for Cryptohome Recovery.
const char kForceCryptohomeRecoveryForTesting[] =
    "force-cryptohome-recovery-for-testing";

// Forces first-run UI to be shown for every login.
const char kForceFirstRunUI[] = "force-first-run-ui";

// Forces Hardware ID check (happens during OOBE) to fail or succeed. Possible
// values: "failure" or "success". Should be used only for testing.
const char kForceHWIDCheckResultForTest[] = "force-hwid-check-result-for-test";

// Force enables the Happiness Tracking System for the device. This ignores
// user profile check and time limits and shows the notification every time
// for any type of user. Should be used only for testing.
const char kForceHappinessTrackingSystem[] = "force-happiness-tracking-system";

// Forces FullRestoreService to launch browser for telemetry tests.
const char kForceLaunchBrowser[] = "force-launch-browser";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[] = "force-login-manager-in-tests";

// Forces the cursor to be shown even if we are mimicking touch events. Note
// that cursor changes are locked when using this switch.
const char kForceShowCursor[] = "force-show-cursor";

// Force the "release track" UI to show in the system tray. Simulates the system
// being on a non-stable release channel with feedback enabled.
const char kForceShowReleaseTrack[] = "force-show-release-track";

// If set, tablet-like power button behavior (i.e. tapping the button turns the
// screen off) is used even if the device is in laptop mode.
const char kForceTabletPowerButton[] = "force-tablet-power-button";

// Specifies the device's form factor. If provided, this flag overrides the
// value from the LSB release info. Possible values are: "CHROMEBASE",
// "CHROMEBIT", "CHROMEBOOK", "REFERENCE", "CHROMEBOX"
const char kFormFactor[] = "form-factor";

// Specifies campaigns to override for testing.
const char kGrowthCampaigns[] = "growth-campaigns";

// Clear all growth framework Feature Engagement events at session start for
// testing.
const char kGrowthCampaignsClearEventsAtSessionStart[] =
    "growth-campaigns-clear-events-at-session-start";

// Path for which to load growth campaigns file for testing (instead of
// downloading from Omaha).
const char kGrowthCampaignsPath[] = "growth-campaigns-path";

// Specifies the device current time in `SecondsSinceUnixEpoch` format for
// testing.
const char kGrowthCampaignsCurrentTimeSecondsSinceUnixEpoch[] =
    "growth-campaigns-current-time";

// Specifies the device registered time in `SecondsSinceUnixEpoch` format for
// testing.
const char kGrowthCampaignsRegisteredTimeSecondsSinceUnixEpoch[] =
    "growth-campaigns-registered-time";

// Specifies the delay time to trigger campaigns for testing.
const char kGrowthCampaignsDelayedTriggerTimeInSecs[] =
    "growth-campaigns-delayed-trigger-time-in-secs";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// Large wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperLarge[] = "guest-wallpaper-large";

// Small wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If set, the system is a Chromebook with a "standard Chrome OS keyboard",
// which generally means one with a Search key in the standard Caps Lock
// location above the Left Shift key. It should be unset for Chromebooks with
// both Search and Caps Lock keys (e.g. stout) and for devices like Chromeboxes
// that only use external keyboards.
const char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

// Whether this device that has hps.
const char kHasHps[] = "has-hps";

// Whether this device has an internal stylus.
const char kHasInternalStylus[] = "has-internal-stylus";

// If set, the system is a Chromebook with a number pad as part of its internal
// keyboard.
const char kHasNumberPad[] = "has-number-pad";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[] = "homedir";

// If set, the "ignore_dev_conf" field in StartArcVmRequest message will
// consequently be set such that all development configuration directives in
// /usr/local/vms/etc/arcvm_dev.conf will be ignored during ARCVM start.
const char kIgnoreArcVmDevConf[] = "ignore-arcvm-dev-conf";

// If true, chrome would silently ignore unknown auth factor types
// instead of crashing.
const char kIgnoreUnknownAuthFactors[] = "ignore-unknown-auth-factors";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// If true, the time dependent views (such as the time view) show with the
// predefined fixed time.
const char kStabilizeTimeDependentViewForTests[] =
    "stabilize-time-dependent-view-for-tests";

// Decreases delay in uploading installation event logs for integration test.
const char kInstallLogFastUploadForTests[] =
    "install-log-fast-upload-for-tests";

// Minimum time the kiosk splash screen will be shown in seconds.
const char kKioskSplashScreenMinTimeSeconds[] =
    "kiosk-splash-screen-min-time-seconds";

// When this flag is set, the lacros-availability policy is ignored.
const char kLacrosAvailabilityIgnore[] = "lacros-availability-ignore";

// If this switch is set, then ash-chrome will pass additional arguments when
// launching lacros-chrome. The string '####' is used as a delimiter. Example:
// --lacros-chrome-additional-args="--foo=5####--bar=/tmp/dir name". Will
// result in two arguments passed to lacros-chrome:
//   --foo=5
//   --bar=/tmp/dir name
const char kLacrosChromeAdditionalArgs[] = "lacros-chrome-additional-args";

// If this switch is set, then ash-chrome will read from the provided path
// and pass additional arguments when launching lacros-chrome. Each non-empty
// line in the file will be treated as an argument. Example file contents:
//   --foo=5
//   --bar=/tmp/dir name
const char kLacrosChromeAdditionalArgsFile[] =
    "lacros-chrome-additional-args-file";

// Additional environment variables set for lacros-chrome. The string '####' is
// used as a delimiter. For example:
// --lacros-chrome-additional-env=WAYLAND_DEBUG=client####FOO=bar
// will enable Wayland protocol logging and set FOO=bar.
const char kLacrosChromeAdditionalEnv[] = "lacros-chrome-additional-env";

// If this switch is set, then ash-chrome will exec the lacros-chrome binary
// from the indicated path rather than from component updater. Note that the
// path should be to a directory that contains a binary named 'chrome'.
const char kLacrosChromePath[] = "lacros-chrome-path";

// If set, ash-chrome will drop a Unix domain socket to wait for a process to
// connect to it, and the connection will be used to request file descriptors
// from ash-chrome, and when the process forks to start a lacros-chrome, the
// obtained file descriptor will be used by lacros-chrome to set up the mojo
// connection with ash-chrome. There are mainly two use cases:
// 1. Test launcher to run browser tests in testing environment.
// 2. A terminal to start lacros-chrome with a debugger.
const char kLacrosMojoSocketForTesting[] = "lacros-mojo-socket-for-testing";

// When this flag is set, the lacros-selection policy is ignored.
const char kLacrosSelectionPolicyIgnore[] = "lacros-selection-policy-ignore";

// If set, it passes the ids of additional extensions allowed to run in
// both ash and lacros when lacros is enabled. The ids are separated by ",".
// This should only used for testing.
// Note: The ids passed to this switch and the ids passed to
// kExtensionsRunInAshOnly should be mutually exclusive, i.e., without overlaps.
// If any extension passed to this switch are to be published to app service,
// it must be listed in one of the app service block switches so that
// it won't be published to app service in both ash and lacros. Currently,
// we don't have any use case with an extension running in both ash and lacros
// to be published to app service, therefore, we haven't defined the app service
// block switch for extensions.
const char kExtensionsRunInBothAshAndLacros[] =
    "extensions-run-in-ash-and-lacros";

// If set, it passes the ids of additional extension apps allowed to run in
// in both ash and lacros when lacros is enabled. The ids are separated by ",".
// This should only used for testing.
// Note: The ids passed to this switch and the ids passed to
// kExtensionAppsRunInAshOnly should be mutually exclusive, i.e., without
// overlaps. If any extension app passed to this switch are to be publisedh to
// app service, it must be listed in one of the app service block switches so
// that it won't be published to app service in both ash and lacros. Currently,
// we only have the use case of an extension app running in both ash and lacros
// to be published to app service in lacros only, therefore, we only add the
// kExtensionAppsBlockForAppServiceInAsh switch.
const char kExtensionAppsRunInBothAshAndLacros[] =
    "extension-apps-run-in-ash-and-lacros";

// If set, it passes the ids of the additional extensions allowed to run in
// ash only when lacros is enabled. The ids are separated by ",".
// This should only used for testing.
const char kExtensionsRunInAshOnly[] = "extensions-run-in-ash-only";

// If set, it passes the ids of the additional extension apps allowed to run in
// ash only when lacros is enabled. The ids are separated by ",".
// This should only used for testing.
const char kExtensionAppsRunInAshOnly[] = "extension-apps-run-in-ash-only";

// If set, it passes the ids of the extension apps blocked for app service
// in ash when lacros is enabled. The ids are separated by ",".
// This should only used for testing.
const char kExtensionAppsBlockForAppServiceInAsh[] =
    "extension-apps-block-for-app-service-in-ash";

// Start Chrome in RMA mode. Launches RMA app automatically.
// kRmaNotAllowed switch takes priority over this one.
const char kLaunchRma[] = "launch-rma";

// Enables the lobster feature.
const char kLobsterFeatureKey[] = "lobster-feature-key";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
const char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[] = "login-user";

// This flag is set if lacros is not allowed. Specifically this flag is set if
// there are more than two signed in users i.e. inside multi-user session.
const char kDisallowLacros[] = "disallow-lacros";

// This flag disables "disallow-lacros" above, if both are set together.
// I.e., if user flips feature flag, or policy is set, lacros can be
// used, event if --disallow-lacros is set.
const char kDisableDisallowLacros[] = "disable-disallow-lacros";

// This flag is a replacement for
// `features::kLacrosOnly` during the in-between phase where users should not be
// able to enable Lacros but developers should for debugging. Just like
// `features::kLacrosOnly`, passing the flag alone does not guarantee that
// Lacros is enabled and other conditions like whether Lacros is allowed to be
// enabled i.e. `standalone_browser::BrowserSupport::IsAllowed()` still apply.
const char kEnableLacrosForTesting[] = "enable-lacros-for-testing";

// Supply secret key for the mahi feature.
const char kMahiFeatureKey[] = "mahi-feature-key";

// Supply secret key for the sparky feature.
const char kSparkyFeatureKey[] = "sparky-feature-key";

// Supply server url for the sparky feature.
const char kSparkyServerUrl[] = "sparky-server-url";

// Specifies the user that the browser data migration should happen for.
const char kBrowserDataMigrationForUser[] = "browser-data-migration-for-user";

// Specifies the user that the browser data backward migration should happen
// for.
const char kBrowserDataBackwardMigrationForUser[] =
    "browser-data-backward-migration-for-user";

// Supply secret key for Coral feature.
const char kCoralFeatureKey[] = "coral-feature-key";

// Tells Chrome to forcefully trigger backward data migration.
extern const char kForceBrowserDataBackwardMigration[] =
    "force-browser-data-backward-migration";

// Run move migration instead of copy. Passed with
// `kBrowserDataMigrationForUser`.
const char kBrowserDataMigrationMode[] = "browser-data-migration-mode";

// Backward migration mode. Passed with `kBrowserDataBackwardMigrationForUser`.
const char kBrowserDataBackwardMigrationMode[] =
    "browser-data-backward-migration-mode";

// Force skip or force migration. Should only be used for testing.
const char kForceBrowserDataMigrationForTesting[] =
    "force-browser-data-migration-for-testing";

// The base URL for the App Mall.
const char kMallUrl[] = "mall-url";

// Determines the URL to be used when calling the backend.
const char kMarketingOptInUrl[] = "marketing-opt-in-url";

// Supply secret key for modifier split feature.
const char kModifierSplitFeatureKey[] = "modifier-split-feature-key";

// Enables natural scroll by default.
const char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// An optional comma-separated list of IDs of apps that can be used to take
// notes. If unset, a hardcoded list is used instead.
const char kNoteTakingAppIds[] = "note-taking-app-ids";

// Enables a prototype version of the PIN-only OOBE flow. Only for tests.
// TODO(b/365059362) - Remove once more stable.
const char kOobeEnablePinOnlyPrototype[] = "oobe-enable-pin-only-prototype";

// Allows the eula url to be overridden for tests.
const char kOobeEulaUrlForTests[] = "oobe-eula-url-for-tests";

// Indicates that the first user run flow (sequence of OOBE screens after the
// first user login) should show tablet mode centric screens, even if the device
// is not in tablet mode.
const char kOobeForceTabletFirstRun[] = "oobe-force-tablet-first-run";

// Indicates that OOBE should be scaled for big displays similar to how Meets
// app scales UI.
// TODO(crbug.com/1205364): Remove after adding new scheme.
const char kOobeLargeScreenSpecialScaling[] =
    "oobe-large-screen-special-scaling";

// When present, prints the time it takes for OOBE's frontend to load.
// See go/oobe-frontend-trace-timings for details.
const char kOobePrintFrontendLoadTimings[] = "oobe-print-frontend-load-timings";

// Specifies directory for screenshots taken with OOBE UI Debugger.
const char kOobeScreenshotDirectory[] = "oobe-screenshot-dir";

// Shows a11y button on the marketing opt in without visiting gesture navigation
// screen.
const char kOobeShowAccessibilityButtonOnMarketingOptInForTesting[] =
    "oobe-show-accessibility-button-on-marketing-opt-in-for-testing";

// Skips new user check in the personalized recommend apps screen for testing.
const char kOobeSkipNewUserCheckForTesting[] =
    "oobe-skip-new-user-check-for-testing";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Returns true if we should skip split modifier check on the split modifier
// info screen.
const char kOobeSkipSplitModifierCheckForTesting[] =
    "oobe-skip-split-modifier-check-for-testing";

// Skip to login screen.
const char kOobeSkipToLogin[] = "oobe-skip-to-login";

// Interval at which we check for total time on OOBE.
const char kOobeTimerInterval[] = "oobe-timer-interval";

// Allows the timezone to be overridden on the marketing opt-in screen.
const char kOobeTimezoneOverrideForTests[] = "oobe-timezone-override-for-tests";

// Trigger sync engine initialization timeout in OOBE for testing.
const char kOobeTriggerSyncTimeoutForTests[] =
    "oobe-trigger-sync-timeout-for-tests";

// If set, the overview button will be visible.
const char kOverviewButtonForTests[] = "overview-button-for-tests";

// Controls how often the HiddenNetworkHandler class checks for wrongly hidden
// networks. The interval should be provided in seconds, should follow the
// format "--hidden-network-migration-interval=#", and should be >= 1.
const char kHiddenNetworkMigrationInterval[] =
    "hidden-network-migration-interval";

// Sets how long a wrongly hidden network must have existed in order to be
// considered for removal. The interval should be provided in days, should
// follow the format "--hidden-network-migration-age=#", and should be >= 0.
const char kHiddenNetworkMigrationAge[] = "hidden-network-migration-age";

// Sets the channel from which the PPD files are loaded.
const char kPrintingPpdChannel[] = "printing-ppd-channel";
const char kPrintingPpdChannelProduction[] = "production";
const char kPrintingPpdChannelStaging[] = "staging";
const char kPrintingPpdChannelDev[] = "dev";
const char kPrintingPpdChannelLocalhost[] = "localhost";

// Sets Privacy Policy hostname url for testing.
const char kPrivacyPolicyHostForTests[] = "privacy-policy-host-for-tests";

// If set to "true", the profile requires policy during restart (policy load
// must succeed, otherwise session restart should fail).
const char kProfileRequiresPolicy[] = "profile-requires-policy";

// SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
// (e.g. for SAML managed guest sessions)
// TODO(crbug.com/40636049): Remove when URL is sent by DMServer.
const char kPublicAccountsSamlAclUrl[] = "public-accounts-saml-acl-url";

// Adds fake Bluetooth devices to the quick settings menu for UI testing.
const char kQsAddFakeBluetoothDevices[] = "qs-add-fake-bluetooth-devices";

// Adds fake Cast devices to the quick settings menu for UI testing.
const char kQsAddFakeCastDevices[] = "qs-add-fake-cast-devices";

// Forces the quick settings "locale" FeatureTile to show. Normally it only
// shows in demo mode, which does not work in the emulator.
const char kQsShowLocaleTile[] = "qs-show-locale-tile";

// The name of the per-model directory which contains per-region
// subdirectories with regulatory label files for this model.
// The per-model directories (if there are any) are located under
// "/usr/share/chromeos-assets/regulatory_labels/".
const char kRegulatoryLabelDir[] = "regulatory-label-dir";

// Testing delay for reboot command. Useful for tast tests.
const char kRemoteRebootCommandDelayInSecondsForTesting[] =
    "remote-reboot-command-timeout-in-seconds-for-testing";

// Indicates that reven UI strings and features should be shown.
const char kRevenBranding[] = "reven-branding";

// The rlz ping delay (in seconds) that overwrites the default value.
const char kRlzPingDelay[] = "rlz-ping-delay";

// Start Chrome without opening RMA or checking the current RMA state.
const char kRmaNotAllowed[] = "rma-not-allowed";

// The switch added by session_manager daemon when chrome crashes 3 times or
// more within the first 60 seconds on start.
// See BrowserJob::ExportArgv in platform2/login_manager/browser_job.cc.
const char kSafeMode[] = "safe-mode";

// Password change url for SAML users.
// TODO(crbug.com/40618074): Remove when the bug is fixed.
const char kSamlPasswordChangeUrl[] = "saml-password-change-url";

// New modular design for the shelf with apps separated into a hotseat UI and
// smaller shelf in clamshell mode.
const char kShelfHotseat[] = "shelf-hotseat";

// Supply the secret key for Scanner (for more details see b/363103871).
const char kScannerUpdateKey[] = "scanner-update-key";

// Supply secret key for Seal feature.
const char kSealKey[] = "seal-key";

// Testing grace period for DeviceScheduledReboot policy. Useful for tast tests.
// See `ShouldSkipRebootDueToGracePeriod` in scheduled_task_util.h.
const char kScheduledRebootGracePeriodInSecondsForTesting[] =
    "scheduled-reboot-grace-period-in-seconds-for-testing";

// If true, the developer tool overlay will be shown for the login/lock screen.
// This makes it easier to test layout logic.
const char kShowLoginDevOverlay[] = "show-login-dev-overlay";

// Enables OOBE UI Debugger for ease of navigation between screens during manual
// testing. Limited to ChromeOS-on-linux and test images only.
const char kShowOobeDevOverlay[] = "show-oobe-dev-overlay";

// Enables the QuickStart debugger in OOBE which mimics an Android phone.
const char kShowOobeQuickStartDebugger[] = "show-oobe-quick-start-debugger";

// Draws a circle at each touch point, similar to the Android OS developer
// option "Show taps".
const char kShowTaps[] = "show-taps";

// Disables online sign-in enforcement in tast tests.
const char kSkipForceOnlineSignInForTesting[] =
    "skip-force-online-signin-for-testing";

// Skip multidevice setup screen during tast tests.
const char kSkipMultideviceScreenForTesting[] = "skip-multidevice-screen";

// Used to skip the threshold duration that the reorder nudge has to show before
// the nudge is considered as shown.
const char kSkipReorderNudgeShowThresholdDurationForTest[] =
    "skip-reorder-nudge-show-threshold-duration";

// If set, the device will be forced to stay in clamshell UI mode but screen
// auto rotation will be supported. E.g, chromebase device Dooly.
const char kSupportsClamshellAutoRotation[] =
    "supports-clamshell-auto-rotation";

// Hides all Message Center notification popups (toasts). Used for testing.
const char kSuppressMessageCenterPopups[] = "suppress-message-center-popups";

// Specifies directory for the Telemetry System Web Extension.
const char kTelemetryExtensionDirectory[] = "telemetry-extension-dir";

// TODO(b/299642185): Remove this flag by the end of 2023.
// ChromeOS does not support empty passwords for users, but some legacy test
// setups might use empty password for users.
const char kTemporaryAllowEmptyPasswordsInTests[] =
    "allow-empty-passwords-in-tests";

// Enables testing for encryption migration UI.
const char kTestEncryptionMigrationUI[] = "test-encryption-migration-ui";

// Enables the wallpaper picker to fetch images from the test server.
const char kTestWallpaperServer[] = "test-wallpaper-server";

// Tells the Chromebook to scan for a tethering host even if there is already a
// wired connection. This allows end-to-end tests to be deployed over ethernet
// without that connection preventing scans and thereby blocking the testing of
// cases with no preexisting connection. Should be used only for testing.
const char kTetherHostScansIgnoreWiredConnections[] =
    "tether-host-scans-ignore-wired-connections";

// Overrides Tether with stub service. Provide integer arguments for the number
// of fake networks desired, e.g. 'tether-stub=2'.
const char kTetherStub[] = "tether-stub";

// Used for overriding the required user activity time before running the
// onboarding survey.
const char kTimeBeforeOnboardingSurveyInSecondsForTesting[] =
    "time-before-onboarding-survey-in-seconds-for-testing";

// Chromebases' touchscreens can be used to wake from suspend, unlike the
// touchscreens on other Chrome OS devices. If set, the touchscreen is kept
// enabled while the screen is off so that it can be used to turn the screen
// back on after it has been turned off for inactivity but before the system has
// suspended.
const char kTouchscreenUsableWhileScreenOff[] =
    "touchscreen-usable-while-screen-off";

// Enables TPM selection in runtime.
const char kTpmIsDynamic[] = "tpm-is-dynamic";

// Shows all Bluetooth devices in UI (System Tray/Settings Page.)
const char kUnfilteredBluetoothDevices[] = "unfiltered-bluetooth-devices";

// If this switch is passed, the device policy DeviceMinimumVersion
// assumes that the device has reached Auto Update Expiration. This is useful
// for testing the policy behaviour on the DUT.
const char kUpdateRequiredAueForTest[] = "aue-reached-for-update-required-test";

// Use the fake FakeCrasAudioClient to handle system audio controls.
const char kUseFakeCrasAudioClientForDBus[] =
    "use-fake-cras-audio-client-for-dbus";

// Flag that stored MyFiles folder inside the user data directory.
// $HOME/Downloads is used as MyFiles folder for ease access to local files for
// debugging when running on Linux. By setting this flag, <cryptohome>/MyFiles
// is used even on Linux.
const char kUseMyFilesInUserDataDirForTesting[] =
    "use-myfiles-in-user-data-dir-for-testing";

// If provided, any webui will be loaded from <flag value>/<handler_name>, where
// handler_name is the name passed to MaybeConfigureTestableDataSource, if the
// file exists.
// For example, if the flag is /tmp/resource_overrides, attempting to load
// js/app_main.js from the data source named "help_app/untrusted" will first
// attempt to load from /tmp/resource_overrides/help_app/untrusted/js/main.js.
const char kWebUiDataSourcePathForTesting[] =
    "web-ui-data-source-path-for-testing";

// Enable the getAccessToken autotest API which creates access tokens using
// the internal OAuth client ID.
const char kGetAccessTokenForTest[] = "get-access-token-for-test";

// Prevent kiosk autolaunch for testing.
const char kPreventKioskAutolaunchForTesting[] =
    "prevent-kiosk-autolaunch-for-testing";

// Allows the Ash shelf to apply the default pin layout without waiting for Sync
// to download data from the server (which many tests can't achieve).
const char kAllowDefaultShelfPinLayoutIgnoringSync[] =
    "ash-allow-default-shelf-pin-layout-ignoring-sync";

// On devices that support refresh rate throttling, force the throttling
// behavior to be active regardless of system state.
const char kForceRefreshRateThrottle[] = "force-refresh-rate-throttle";

bool IsAuthSessionCryptohomeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kCryptohomeUseAuthSession);
}

bool IsCellularFirstDevice() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kCellularFirst);
}

bool IsRevenBranding() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kRevenBranding);
}

bool IsSigninFrameClientCertsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCerts);
}

bool ShouldTetherHostScansIgnoreWiredConnections() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTetherHostScansIgnoreWiredConnections);
}

bool ShouldSkipNewUserCheckForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeSkipNewUserCheckForTesting);
}

bool ShouldSkipSplitModifierCheckForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeSkipSplitModifierCheckForTesting);
}

bool ShouldSkipOobePostLogin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOobeSkipPostLogin);
}

bool ShouldShowAccessibilityButtonOnMarketingOptInForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeShowAccessibilityButtonOnMarketingOptInForTesting);
}

bool IsAshDebugBrowserEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableAshDebugBrowser);
}

bool IsTabletFormFactor() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTabletFormFactor);
}

bool ShouldMultideviceScreenBeSkippedForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSkipMultideviceScreenForTesting);
}

bool IsGaiaServicesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableGaiaServices);
}

bool IsArcCpuRestrictionDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableArcCpuRestriction);
}

bool IsTpmDynamic() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kTpmIsDynamic);
}

bool IsUnfilteredBluetoothDevicesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUnfilteredBluetoothDevices);
}

bool ShouldOobeUseTabletModeFirstRun() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeForceTabletFirstRun);
}

bool ShouldScaleOobe() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeLargeScreenSpecialScaling);
}

bool IsAueReachedForUpdateRequiredForTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUpdateRequiredAueForTest);
}

bool AreEmptyPasswordsAllowedForForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTemporaryAllowEmptyPasswordsInTests);
}

bool IsOOBEChromeVoxHintTimerDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableOOBEChromeVoxHintTimerForTesting);
}

bool IsOOBENetworkScreenSkippingDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableOOBENetworkScreenSkippingForTesting);
}

bool IsOOBEChromeVoxHintEnabledForDevMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableOOBEChromeVoxHintForDevMode);
}

bool IsOobePinOnlyPrototypeEnabled() {
  // Directly dependent on the 'PasswordlessSetup' flag. This command line
  // switch provides an 'early preview' into the PasswordlessSetup (PIN-only)
  // flow and will be removed once it is more stable.
  return features::IsAllowPasswordlessSetupEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kOobeEnablePinOnlyPrototype);
}

bool IsOverviewButtonEnabledForTests() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOverviewButtonForTests);
}

bool IsDeviceRequisitionConfigurable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableRequisitionEdits);
}

bool IsOsInstallAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kAllowOsInstall);
}

std::optional<base::TimeDelta> ContextualNudgesInterval() {
  int numeric_cooldown_time;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAshContextualNudgesInterval) &&
      base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kAshContextualNudgesInterval),
          &numeric_cooldown_time)) {
    base::TimeDelta cooldown_time = base::Seconds(numeric_cooldown_time);
    cooldown_time = std::clamp(cooldown_time, kAshContextualNudgesMinInterval,
                               kAshContextualNudgesMaxInterval);
    return std::optional<base::TimeDelta>(cooldown_time);
  }
  return std::nullopt;
}

bool ContextualNudgesResetShownCount() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshContextualNudgesResetShownCount);
}

bool IsUsingShelfAutoDim() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableDimShelf);
}

bool ShouldClearFastInkBuffer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshClearFastInkBuffer);
}

bool HasHps() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kHasHps);
}

bool IsSkipRecorderNudgeShowThresholdDurationEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSkipReorderNudgeShowThresholdDurationForTest);
}

bool IsStabilizeTimeDependentViewForTestsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kStabilizeTimeDependentViewForTests);
}

bool UseFakeCrasAudioClientForDBus() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUseFakeCrasAudioClientForDBus);
}

bool ShouldAllowDefaultShelfPinLayoutIgnoringSync() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAllowDefaultShelfPinLayoutIgnoringSync);
}

bool IsCampbellSecretKeyMatched() {
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --campbell-key="INSERT KEY HERE"
  //  --enable-features=CampbellGlyph:icon/<icon>
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kCampbellKey));

  const bool key_matched = (provided_key_hash == kCampbellHashKey);
  if (!key_matched) {
    LOG(ERROR)
        << "Provided campbel secrey key does not match the expected one.";
  }

  return key_matched;
}

bool IsConchSecretKeyMatched() {
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --conch-key="INSERT KEY HERE" --enable-features=Conch
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kConchKey));

  bool key_matched = (provided_key_hash == kConchHashKey);
  if (!key_matched) {
    LOG(ERROR)
        << "Provided conch secret key does not match with the expected one.";
  }

  return key_matched;
}

bool IsMahiSecretKeyMatched() {
  if (g_ignore_mahi_secret_key) {
    return true;
  }

  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --mahi-feature-key="INSERT KEY HERE" --enable-features=Mahi
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMahiFeatureKey));

  bool mahi_key_matched = (provided_key_hash == kMahiHashKey);
  if (!mahi_key_matched) {
    LOG(ERROR) << "Provided secret key does not match with the expected one.";
  }

  return mahi_key_matched;
}

base::AutoReset<bool> SetIgnoreMahiSecretKeyForTest() {
  return {&g_ignore_mahi_secret_key, true};
}

bool IsSparkySecretKeyMatched() {
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --sparky-feature-key="INSERT KEY HERE" --enable-features=Sparky
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kSparkyFeatureKey));

  bool sparky_key_matched = (provided_key_hash == kSparkyHashKey);
  if (!sparky_key_matched) {
    LOG(ERROR) << "Provided secret key does not match with the expected one.";
  }

  return sparky_key_matched;
}

bool IsModifierSplitSecretKeyMatched() {
  if (g_ignore_modifier_split_secret_key) {
    return true;
  }

  static const bool modifier_split_key_matched = []() {
    // Commandline looks like:
    //  out/Default/chrome --user-data-dir=/tmp/tmp123
    //  --modifier-split-feature-key="INSERT KEY HERE"
    //  --enable-features=ModifierSplit
    const std::string provided_key_hash = base::SHA1HashString(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kModifierSplitFeatureKey));

    const bool modifier_split_key_matched =
        (provided_key_hash == kModifierSplitHashKey);
    if (!modifier_split_key_matched) {
      LOG(ERROR) << "Provided secret key does not match with the expected one.";
    }

    return modifier_split_key_matched;
  }();

  return modifier_split_key_matched;
}

base::AutoReset<bool> SetIgnoreModifierSplitSecretKeyForTest() {
  return {&g_ignore_modifier_split_secret_key, true};
}

std::optional<std::string> ObtainSparkyServerUrl() {
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --sparky-server-url="INSERT KEY HERE"
  //  --enable-features=Sparky
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSparkyServerUrl)) {
    return std::make_optional(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kSparkyServerUrl));
  }
  return std::nullopt;
}

bool IsScannerUpdateSecretKeyMatched() {
  if (g_ignore_scanner_update_secret_key) {
    return true;
  }

  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --scanner-update-key="INSERT KEY HERE" --enable-features=ScannerUpdate
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kScannerUpdateKey));

  const bool scanner_key_matched = (provided_key_hash == kScannerUpdateHashKey);
  if (!scanner_key_matched) {
    LOG(ERROR) << "Provided secret key does not match with the expected one.";
  }

  return scanner_key_matched;
}

base::AutoReset<bool> SetIgnoreScannerUpdateSecretKeyForTest() {
  return {&g_ignore_scanner_update_secret_key, true};
}

}  // namespace ash::switches
