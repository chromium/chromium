// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "ash/constants/ash_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/libsegmentation/buildflags.h"
#include "chromeos/constants/chromeos_features.h"

#if defined(ARCH_CPU_ARM_FAMILY)
#include "base/command_line.h"
#endif  // defined(ARCH_CPU_ARM_FAMILY)

namespace ash::features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
BASE_FEATURE(kInstantTetheringBackgroundAdvertisementSupport,
             "InstantTetheringBackgroundAdvertisementSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// Enables the UI and logic that minimizes the amount of time the device spends
// at full battery. This preserves battery lifetime.
BASE_FEATURE(kAdaptiveCharging,
             "AdaptiveCharging",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the logic to show the notifications for Adaptive Charging features.
// This is intended to be used by developers to test the UI aspect of the
// feature.
BASE_FEATURE(kAdaptiveChargingForTesting,
             "AdaptiveChargingForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI for additional on-device parental controls that can be used to
// enable or block ARC++ apps.
BASE_FEATURE(kOnDeviceAppControls,
             "OnDeviceAppControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
BASE_FEATURE(kAllowAmbientEQ,
             "AllowAmbientEQ",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Cross-Device features, e.g. Nearby Share, Smart Lock, Fast Pair, etc.
// This flag is used to disable Cross-Device on platforms where we cannot yet
// guarantee a good experience with the stock Bluetooth hardware (e.g. Reven /
// ChromeOS Flex). Access through IsCrossDeviceFeatureSuiteAllowed().
BASE_FEATURE(kAllowCrossDeviceFeatureSuite,
             "AllowCrossDeviceFeatureSuite",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows DevTools to open from the context menu and shortcut keys in Ash if
// Lacros is the only browser.
BASE_FEATURE(kAllowDevtoolsInSystemUI,
             "AllowDevtoolsInSystemUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether devices are updated before reboot after the first update.
BASE_FEATURE(kAllowRepeatedUpdates,
             "AllowRepeatedUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Always reinstall system web apps, instead of only doing so after version
// upgrade or locale changes.
BASE_FEATURE(kAlwaysReinstallSystemWebApps,
             "ReinstallSystemWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shows settings for adjusting scroll acceleration/sensitivity for
// mouse.
BASE_FEATURE(kAllowScrollSettings,
             "AllowScrollSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAltClickAndSixPackCustomization,
             "AltClickAndSixPackCustomization",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow Dev channel to use Prod server feature.
BASE_FEATURE(kAmbientModeDevUseProdFeature,
             "ChromeOSAmbientModeDevChannelUseProdServer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Ambient mode album selection with photo previews.
BASE_FEATURE(kAmbientModePhotoPreviewFeature,
             "ChromeOSAmbientModePhotoPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the logic for managed screensaver is enabled or not.
BASE_FEATURE(kAmbientModeManagedScreensaver,
             "ChromeOSAmbientModeManagedScreensaver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds support for allowing or disabling APN modification by policy.
BASE_FEATURE(kAllowApnModificationPolicy,
             "AllowApnModificationPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the annotator feature is enabled in ChromeOS.
BASE_FEATURE(kAnnotatorMode,
             "AnnotatorMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApnRevamp, "ApnRevamp", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable ARC ADB sideloading support.
BASE_FEATURE(kArcAdbSideloadingFeature,
             "ArcAdbSideloading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether files shared from ARC apps to Web Apps should be shared
// through the FuseBox service.
BASE_FEATURE(kArcFuseBoxFileSharing,
             "ArcFuseBoxFileSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
BASE_FEATURE(kArcManagedAdbSideloadingSupport,
             "ArcManagedAdbSideloadingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to defer loading of active tabs of background (occluded)
// browser windows during session restore.
BASE_FEATURE(kAshSessionRestoreDeferOccludedActiveTabLoad,
             "AshSessionRestoreDeferOccludedActiveTabLoad",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable assistive multi word suggestions.
BASE_FEATURE(kAssistMultiWord,
             "AssistMultiWord",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantNativeIcons,
             "AssistantNativeIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the ui to show the toggle for controlling hfp-mic-sr.
BASE_FEATURE(kAudioHFPMicSRToggle,
             "AudioHFPMicSRToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables audio I/O selection improvement algorithm. http://launch/4301655.
BASE_FEATURE(kAudioSelectionImprovement,
             "AudioSelectionImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the AudioSourceFetcher resamples the audio for speech
// recongnition.
BASE_FEATURE(kAudioSourceFetcherResampling,
             "AudioSourceFetcherResampling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Audio URL that is designed to help user debug or troubleshoot
// common issues on ChromeOS.
BASE_FEATURE(kAudioUrl, "AudioUrl", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Auto Night Light feature which sets the default schedule type to
// sunset-to-sunrise until the user changes it to something else. This feature
// is not exposed to the end user, and is enabled only via cros_config for
// certain devices.
BASE_FEATURE(kAutoNightLight,
             "AutoNightLight",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
BASE_FEATURE(kAutoScreenBrightness,
             "AutoScreenBrightness",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables collection of autocorrect strings for federated analytics via Private
// Heavy Hitters (PHH).
BASE_FEATURE(kAutocorrectFederatedPhh,
             "AutocorrectFederatedPhh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables params tuning experiment for autocorrect on ChromeOS.
BASE_FEATURE(kAutocorrectParamsTuning,
             "AutocorrectParamsTuning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a toggle for enabling autocorrect on ChromeOS.
BASE_FEATURE(kAutocorrectToggle,
             "AutocorrectToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using a toggle for enabling autocorrect on ChromeOS.
BASE_FEATURE(kAutocorrectByDefault,
             "AutocorrectByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutocorrectUseReplaceSurroundingText,
             "AutocorrectUseReplaceSurroundingText",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the autozoom nudge shown prefs will be reset at the start of
// each new user session.
BASE_FEATURE(kAutozoomNudgeSessionReset,
             "AutozoomNudgeSessionReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Make Battery Saver available.
BASE_FEATURE(kBatterySaver,
             "CrosBatterySaver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determines the behavior of the battery saver controller auto enable threshold
// and notification timing.
const base::FeatureParam<BatterySaverNotificationBehavior>::Option
    battery_saver_notification_options[] = {
        {BatterySaverNotificationBehavior::kBSMAutoEnable, "kBSMAutoEnable"},
        {BatterySaverNotificationBehavior::kBSMOptIn, "kBSMOptIn"},
};
const base::FeatureParam<BatterySaverNotificationBehavior>
    kBatterySaverNotificationBehavior{
        &kBatterySaver, "BatterySaverNotificationBehavior",
        BatterySaverNotificationBehavior::kBSMAutoEnable,
        &battery_saver_notification_options};

// Determines the charge percent of when we will activate Battery Saver
// automatically and send a notification.
const base::FeatureParam<double> kBatterySaverActivationChargePercent{
    &kBatterySaver, "BatterySaverActivationChargePercent", 20};

// Make Battery Saver on all the time, even when charged or charging.
BASE_FEATURE(kBatterySaverAlwaysOn,
             "CrosBatterySaverAlwaysOn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Display coral information in birch UI.
BASE_FEATURE(kBirchCoral, "BirchCoral", base::FEATURE_DISABLED_BY_DEFAULT);

// Display weather information in birch UI. The feature is rolled out via Finch
// to allow measurement of weather server load (QPS).
BASE_FEATURE(kBirchWeather, "BirchWeather", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
BASE_FEATURE(kBluetoothFixA2dpPacketSize,
             "BluetoothFixA2dpPacketSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Bluetooth Disconnect Warning feature.
BASE_FEATURE(kBluetoothDisconnectWarning,
             "BluetoothDisconnectWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the Bluetooth Quality Report feature.
BASE_FEATURE(kBluetoothQualityReport,
             "BluetoothQualityReport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Boca feature on ChromeOS
BASE_FEATURE(kBoca, "Boca", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Boca consumer user experience on ChromeOS.
BASE_FEATURE(kBocaConsumer, "BocaConsumer", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Boca extension consumer experience on ChromeOS.
BASE_FEATURE(kBocaExtensionConsumer,
             "BocaExtensionConsumer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCrosSwitcher, "CrosSwitcher", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables digital zoom control inside Camera app.
BASE_FEATURE(kCameraAppDigitalZoom,
             "CameraAppDigitalZoom",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Make the text on PDFs saved from Camera app accessible.
BASE_FEATURE(kCameraAppPdfOcr,
             "CameraAppPdfOcr",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable OCR features on the preview in Camera app.
BASE_FEATURE(kCameraAppPreviewOcr,
             "CameraAppPreviewOcr",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Indicates whether the camera super resolution is supported. Note that this
// feature is overridden by login_manager based on whether a per-board build
// sets the USE camera_feature_super_res flag. Refer to:
// chromiumos/src/platform2/login_manager/chrome_setup.cc
BASE_FEATURE(kCameraSuperResSupported,
             "CameraSuperResSupported",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Big GL when using Borealis.
BASE_FEATURE(kBorealisBigGl, "BorealisBigGl", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable dGPU when using Borealis.
BASE_FEATURE(kBorealisDGPU, "BorealisDGPU", base::FEATURE_ENABLED_BY_DEFAULT);

// Bypass some hardware checks when deciding whether to block/allow borealis.
BASE_FEATURE(kBorealisEnableUnsupportedHardware,
             "BorealisEnableUnsupportedHardware",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force the steam client to be on its beta version. If not set, the client will
// be on its stable version.
BASE_FEATURE(kBorealisForceBetaClient,
             "BorealisForceBetaClient",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force the steam client to render in 2x size (using GDK_SCALE as discussed in
// b/171935238#comment4).
BASE_FEATURE(kBorealisForceDoubleScale,
             "BorealisForceDoubleScale",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prevent the steam client from exercising ChromeOS integrations, in this mode
// it functions more like the linux client.
BASE_FEATURE(kBorealisLinuxMode,
             "BorealisLinuxMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable borealis on this device. This won't necessarily allow it, since you
// might fail subsequent checks.
BASE_FEATURE(kBorealisPermitted,
             "BorealisPermitted",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the "provision" flag when mounting Borealis' stateful disk.
// TODO(b/288361720): This is temporary while we test the 'provision'
// mount option. Once we're satisfied things are stable, we'll make this
// the default and remove this feature/flag.
BASE_FEATURE(kBorealisProvision,
             "BorealisProvision",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disable use of calculated scale for -forcedesktopscaling on Steam client.
// Scale will default to a value of 1.
BASE_FEATURE(kBorealisScaleClientByDPI,
             "BorealisScaleClientByDPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBorealisZinkGlDriver,
             "BorealisZinkGlDriver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows UserDataAuth client to use fingerprint auth factor.
BASE_FEATURE(kFingerprintAuthFactor,
             "FingerprintAuthFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<BorealisZinkGlDriverParam>::Option
    borealis_zink_gl_driver_options[] = {
        {BorealisZinkGlDriverParam::kZinkEnableRecommended,
         "ZinkEnableRecommended"},
        {BorealisZinkGlDriverParam::kZinkEnableAll, "ZinkEnableAll"}};
const base::FeatureParam<BorealisZinkGlDriverParam> kBorealisZinkGlDriverParam{
    &kBorealisZinkGlDriver, "BorealisZinkGlDriverParam",
    BorealisZinkGlDriverParam::kZinkEnableRecommended,
    &borealis_zink_gl_driver_options};

// Enables client cert caching in ClientCertStoreAsh.
BASE_FEATURE(kUseKcerClientCertStore,
             "UseKcerClientCertStore",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the feature to parameterize glyph for "Campbell" feature.
BASE_FEATURE(kCampbellGlyph,
             "CampbellGlyph",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the nudges/tutorials that inform users of the screen capture keyboard
// shortcut and feature tile.
BASE_FEATURE(kCaptureModeEducation,
             "CaptureModeEducation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Determines how we educate the user to the screen capture entry points.
constexpr base::FeatureParam<CaptureModeEducationParam>::Option
    capture_mode_education_type_options[] = {
        {CaptureModeEducationParam::kShortcutNudge, "ShortcutNudge"},
        {CaptureModeEducationParam::kShortcutTutorial, "ShortcutTutorial"},
        {CaptureModeEducationParam::kQuickSettingsNudge, "QuickSettingsNudge"}};
const base::FeatureParam<CaptureModeEducationParam> kCaptureModeEducationParam{
    &kCaptureModeEducation, "CaptureModeEducationParam",
    CaptureModeEducationParam::kShortcutNudge,
    &capture_mode_education_type_options};

// Enables bypassing the 3 times / 24 hours show limits for the Capture Mode
// education nudges and tutorials.
BASE_FEATURE(kCaptureModeEducationBypassLimits,
             "CaptureModeEducationBypassLimits",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allow eSIM installation bypass the non-cellular internet
// connectivity check.
BASE_FEATURE(kCellularBypassESimInstallationConnectivityCheck,
             "CellularBypassESimInstallationConnectivityCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use second the Euicc that is exposed by Hermes in Cellular Setup
// and Settings.
BASE_FEATURE(kCellularUseSecondEuicc,
             "CellularUseSecondEuicc",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allow the user to switch from Gaia password to local password in
// Settings and in the recovery flow.
BASE_FEATURE(kChangePasswordFactorSetup,
             "ChangePasswordFactorSeteup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Multiple scraped passwords should be checked against password in
// cryptohome.
BASE_FEATURE(kCheckPasswordsAgainstCryptohomeHelper,
             "CheckPasswordsAgainstCryptohomeHelper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled alongside the keyboard auto-repeat setting, holding down Ctrl+V
// will cause the clipboard history menu to show. From there, the user can
// select a clipboard history item to replace the initially pasted content.
BASE_FEATURE(kClipboardHistoryLongpress,
             "ClipboardHistoryLongpress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled when the user copies a URL that is present in the primary user
// profile's browsing history, the clipboard history menu will show the page
// title as part of the URL's menu item.
BASE_FEATURE(kClipboardHistoryUrlTitles,
             "ClipboardHistoryUrlTitles",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling/disabling conch.
BASE_FEATURE(kConch, "Conch", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, conch will use microphone to capture system audio.
BASE_FEATURE(kConchSystemAudioFromMic,
             "ConchSystemAudioFromMic",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled and account falls under the new deal, will be allowed to toggle
// auto updates.
BASE_FEATURE(kConsumerAutoUpdateToggleAllowed,
             "ConsumerAutoUpdateToggleAllowed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a smooth overview mode transition based on the gesture position.
BASE_FEATURE(kContinuousOverviewScrollAnimation,
             "ContinuousOverviewScrollAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling/disabling the coral feature.
BASE_FEATURE(kCoralFeature, "CoralFeature", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables execution of routine for copying client keys and certs from NSS DB to
// software backed Chaps slot. It's only respected if the
// EnableNssDbClientCertsRollback feature flag is disabled.
BASE_FEATURE(kCopyClientKeysCertsToChaps,
             "CopyClientKeysCertsToChaps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds location access control to Privacy Hub.
BASE_FEATURE(kCrosPrivacyHub,
             "CrosPrivacyHub",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables app permissions view inside Privacy Hub.
BASE_FEATURE(kCrosPrivacyHubAppPermissions,
             "CrosPrivacyHubAppPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds controls to the OS Apps subpages for managing sensor system access and
// more.
BASE_FEATURE(kCrosPrivacyHubAppPermissionsV2,
             "CrosPrivacyHubAppPermissionsV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables syncing attestation certificates to cryptauth for use by Cross Device
// features, including Eche and Phone Hub.
BASE_FEATURE(kCryptauthAttestationSyncing,
             "CryptauthAttestationSyncing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini GPU support.
// Note that this feature can be overridden by login_manager based on
// whether a per-board build sets the USE virtio_gpu flag.
// Refer to: chromiumos/src/platform2/login_manager/chrome_setup.cc
BASE_FEATURE(kCrostiniGpuSupport,
             "CrostiniGpuSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force enable recreating the LXD DB at LXD launch.
BASE_FEATURE(kCrostiniResetLxdDb,
             "CrostiniResetLxdDb",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental UI creating and managing multiple Crostini containers.
BASE_FEATURE(kCrostiniMultiContainer,
             "CrostiniMultiContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Qt application IME support.
BASE_FEATURE(kCrostiniQtImeSupport,
             "CrostiniQtImeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Virtual Keyboard support.
BASE_FEATURE(kCrostiniVirtualKeyboardSupport,
             "CrostiniVirtualKeyboardSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
BASE_FEATURE(kCryptAuthV2DeviceActivityStatus,
             "CryptAuthV2DeviceActivityStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables use of the connectivity status from Cryptauth's
// GetDevicesActivityStatus API to sort devices.
BASE_FEATURE(kCryptAuthV2DeviceActivityStatusUseConnectivity,
             "CryptAuthV2DeviceActivityStatusUseConnectivity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables use of last activity time to deduplicate eligible host
// phones in multidevice setup dropdown list. We assume that different copies
// of same device share the same last activity time but different last update
// time.
BASE_FEATURE(kCryptAuthV2DedupDeviceLastActivityTime,
             "CryptAuthV2DedupDeviceLastActivityTime",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disable a Files banner about Google One offer. This flag is used by G1+
// nudge to conditionally disable the G1 file banner via finch.
BASE_FEATURE(kDisableGoogleOneOfferFilesBanner,
             "DisableGoogleOneOfferFilesBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the default value for the option to set up
// cryptohome recovery presented for consumer users.
// - if enabled, recovery would set up by default (opt-out mode)
// - if disabled, user have to explicitly opt-in to use recovery
BASE_FEATURE(kCryptohomeRecoveryByDefaultForConsumers,
             "CryptohomeRecoveryByDefaultForConsumers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the behavior during onboarding when the RecoveryFactorBehavior
// policy is unset.
// - if enabled, treat as "recommended enable recovery" policy value.
// - if disabled, treat as "recommended disable recovery" policy value.
BASE_FEATURE(kCryptohomeRecoveryByDefaultForEnterprise,
             "CryptohomeRecoveryByDefaultForEnterprise",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Toggle different display features based on user setting and power state
BASE_FEATURE(kDisplayPerformanceMode,
             "DisplayPerformanceMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Performance optimization that only renders visible windows in each desk's
// preview within the desk bar. Should have no user-visible effects except
// lower latency when opening the desk bar and overview mode.
BASE_FEATURE(kDeskBarWindowOcclusionOptimization,
             "DeskBarWindowOcclusionOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds a desk button to the shelf that the user can use to navigate between
// desks.
BASE_FEATURE(kDeskButton, "DeskButton", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Sync for desk templates on ChromeOS.
BASE_FEATURE(kDeskTemplateSync,
             "DeskTemplateSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDesksTemplates,
             "DesksTemplates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables diacritics on longpress on the physical keyboard.
BASE_FEATURE(kDiacriticsOnPhysicalKeyboardLongpress,
             "DiacriticsOnPhysicalKeyboardLongpress",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables diacritics on longpress on the physical keyboard by default.
BASE_FEATURE(kDiacriticsOnPhysicalKeyboardLongpressDefaultOn,
             "DiacriticsOnPhysicalKeyboardLongpressDefaultOn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, diacritics on longpress will use |ReplaceSurroundingText| API,
// which is atomic and compatible with more apps.
BASE_FEATURE(kDiacriticsUseReplaceSurroundingText,
             "DiacriticsUseReplaceSurroundingText",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables hardware requirement checks for Bruschetta installer, allowing for
// more easy development against changes of said requirements.
BASE_FEATURE(kDisableBruschettaInstallChecks,
             "DisableBruschettaInstallChecks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to disable the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
BASE_FEATURE(kDisableCryptAuthV1DeviceSync,
             "DisableCryptAuthV1DeviceSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for disable/enable Lacros TTS support.
// The flag is enabled by default so that the feature is disabled before it is
// completedly implemented.
BASE_FEATURE(kDisableLacrosTtsSupport,
             "DisableLacrosTtsSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disables the DNS proxy service for ChromeOS.
BASE_FEATURE(kDisableDnsProxy,
             "DisableDnsProxy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disconnect WiFi when the device get connected to Ethernet.
BASE_FEATURE(kDisconnectWiFiOnEthernetConnected,
             "DisconnectWiFiOnEthernetConnected",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables indicators to hint where displays are connected.
BASE_FEATURE(kDisplayAlignAssist,
             "DisplayAlignAssist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
BASE_FEATURE(kDockedMagnifier,
             "DockedMagnifier",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, DriveFS will be used for Drive sync.
BASE_FEATURE(kDriveFs, "DriveFS", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables DriveFS' experimental local files mirroring functionality.
BASE_FEATURE(kDriveFsMirroring,
             "DriveFsMirroring",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to Chrome's Network Service for DriveFS.
BASE_FEATURE(kDriveFsChromeNetworking,
             "DriveFsChromeNetworking",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables DriveFS' bulk pinning functionality.
BASE_FEATURE(kDriveFsBulkPinning,
             "DriveFsBulkPinning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Carries DriveFS' bulk-pinning experimental parameters.
BASE_FEATURE(kDriveFsBulkPinningExperiment,
             "DriveFsBulkPinningExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables DriveFS' bulk pinning functionality. This flag is to be enabled by
// the feature management module.
BASE_FEATURE(kFeatureManagementDriveFsBulkPinning,
             "FeatureManagementDriveFsBulkPinning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partial support of CSE files on ChromeOS: users will be able to see
// the files and open in web apps, but not to open/read/write CSE files locally.
BASE_FEATURE(kDriveFsShowCSEFiles,
             "DriveFsShowCSEFiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables authenticating to Wi-Fi networks using EAP-GTC.
BASE_FEATURE(kEapGtcWifiAuthentication,
             "EapGtcWifiAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the System Web App (SWA) version of Eche.
BASE_FEATURE(kEcheSWA, "EcheSWA", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Debug Mode of Eche.
BASE_FEATURE(kEcheSWADebugMode,
             "EcheSWADebugMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the E2E latecny measurement of Eche.
BASE_FEATURE(kEcheSWAMeasureLatency,
             "EcheSWAMeasureLatency",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending start signaling to establish Eche's WebRTC connection.
BASE_FEATURE(kEcheSWASendStartSignaling,
             "EcheSWASendStartSignaling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows disabling the stun servers when establishing a WebRTC connection to
// Eche.
BASE_FEATURE(kEcheSWADisableStunServer,
             "EcheSWADisableStunServer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows CrOS to analyze Android
// network information to provide more context on connection errors.
BASE_FEATURE(kEcheSWACheckAndroidNetworkInfo,
             "EcheSWACheckAndroidNetworkInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows CrOS to process Android
// accessibility tree information.
BASE_FEATURE(kEcheSWAProcessAndroidAccessibilityTree,
             "EcheSWAProcessAndroidAccessibilityTree",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables metrics revamp for Eche.
BASE_FEATURE(kEcheMetricsRevamp,
             "EcheMetricsRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables background blur for the app list, shelf, unified system tray,
// autoclick menu, etc. Also enables the AppsGridView mask layer, slower devices
// may have choppier app list animations while in this mode. crbug.com/765292.
BASE_FEATURE(kEnableBackgroundBlur,
             "EnableBackgroundBlur",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables settings to control internal display brightness and auto-brightness.
BASE_FEATURE(kEnableBrightnessControlInSettings,
             "EnableBrightnessControlInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables an update to 24px rounded corners for all `TrayBubbleView`s and
// `AppListBubbleView`.
BASE_FEATURE(kEnableBubbleCornerRadiusUpdate,
             "EnableBubbleCornerRadiusUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables exporting of the selected Preferences so that they can be accessed
// early in the sign-in flow, before loading Profile.
BASE_FEATURE(kEnableEarlyPrefs,
             "EnableEarlyPrefs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables external keyboard testers in the diagnostics app.
BASE_FEATURE(kEnableExternalKeyboardsInDiagnostics,
             "EnableExternalKeyboardsInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the device hostname.
BASE_FEATURE(kEnableHostnameSetting,
             "EnableHostnameSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables keyboard backlight control in settings.
BASE_FEATURE(kEnableKeyboardBacklightControlInSettings,
             "EnableKeyboardBacklightControlInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables keyboard backlight toggle.
BASE_FEATURE(kEnableKeyboardBacklightToggle,
             "EnableKeyboardBacklightToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable keyboard rewriter fix.
BASE_FEATURE(kEnableKeyboardRewriterFix,
             "EnableKeyboardRewriterFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Login WebUI was always loaded for legacy reasons even when it was not needed.
// When enabled, it will make login WebUI loaded only before showing it.
BASE_FEATURE(kEnableLazyLoginWebUILoading,
             "EnableLazyLoginWebUILoading",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables LocalSearchService to be initialized.
BASE_FEATURE(kEnableLocalSearchService,
             "EnableLocalSearchService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OAuth support when printing via the IPP protocol.
BASE_FEATURE(kEnableOAuthIpp,
             "EnableOAuthIpp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the OOBE ChromeVox hint dialog and announcement feature.
BASE_FEATURE(kEnableOobeChromeVoxHint,
             "EnableOobeChromeVoxHint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Kiosk UI in Login screen.
BASE_FEATURE(kEnableKioskLoginScreen,
             "EnableKioskLoginScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing notification after the password change for SAML users.
BASE_FEATURE(kEnableSamlNotificationOnPasswordChangeSuccess,
             "EnableSamlNotificationOnPasswordChangeSuccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables all registered system web apps, regardless of their respective
// feature flags.
BASE_FEATURE(kEnableAllSystemWebApps,
             "EnableAllSystemWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables RFC8925 (prefer IPv6-only on an IPv6-only-capable network).
BASE_FEATURE(kEnableRFC8925, "EnableRFC8925", base::FEATURE_ENABLED_BY_DEFAULT);

// TODO:(b/345017297): If enabled, touchscreen mapping experience is visible in
// settings.
BASE_FEATURE(kEnableTouchscreenMappingExperience,
             "EnableTouchscreenMappingExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, touchpad cards will be shown in the diagnostics app's input
// section.
BASE_FEATURE(kEnableTouchpadsInDiagnosticsApp,
             "EnableTouchpadsInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, touchscreen cards will be shown in the diagnostics app's input
// section.
BASE_FEATURE(kEnableTouchscreensInDiagnosticsApp,
             "EnableTouchscreensInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables rollback routine which will delete client keys and certificates
// from the software backed Chaps storage. Copies of keys and certificates will
// will continue to exist in NSS DB.
BASE_FEATURE(kEnableNssDbClientCertsRollback,
             "EnableNssDbClientCertsRollback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables WiFi QoS to detect and prioritize selected egress network traffic
// using WiFi QoS/WMM in congested WiFi environments.
BASE_FEATURE(kEnableWifiQos, "EnableWifiQos", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables WiFi QoS to detect and prioritize selected egress network traffic
// using WiFi QoS/WMM in congested WiFi environments. For an Enterprise enrolled
// device:
// - If this flag is enabled, the feature will be controlled by EnableWifiQos;
// - If this flag is disabled, the feature will be disabled.
BASE_FEATURE(kEnableWifiQosEnterprise,
             "EnableWifiQosEnterprise",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enforces Ash extension keep-list. Only the extensions/Chrome apps in the
// keep-list are enabled in Ash.
BASE_FEATURE(kEnforceAshExtensionKeeplist,
             "EnforceAshExtensionKeeplist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables entering overview mode by clicking the wallpaper with the mouse.
BASE_FEATURE(kEnterOverviewFromWallpaper,
             "EnterOverviewFromWallpaper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to the chrome://enterprise-reporting WebUI.
BASE_FEATURE(kEnterpriseReportingUI,
             "EnterpriseReportingUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ephemeral network configuration policies are respected.
BASE_FEATURE(kEphemeralNetworkPolicies,
             "kEphemeralNetworkPolicies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the DeviceEphemeralNetworkPoliciesEnabled policy is
// respected.
// This is on-by-default, only intended to be used as a kill switch in case we
// find some issue with the policy processing.
BASE_FEATURE(kEphemeralNetworkPoliciesEnabledPolicy,
             "EphemeralNetworkPoliciesEnabledPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Device End Of Lifetime warning notifications.
BASE_FEATURE(kEolWarningNotifications,
             "EolWarningNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Device End Of Lifetime incentive notifications.
BASE_FEATURE(kEolIncentive, "EolIncentive", base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<EolIncentiveParam>::Option eol_incentive_options[] = {
    {EolIncentiveParam::kNoOffer, "no_offer"},
    {EolIncentiveParam::kOffer, "offer"},
    {EolIncentiveParam::kOfferWithWarning, "offer_with_warning"}};
const base::FeatureParam<EolIncentiveParam> kEolIncentiveParam{
    &kEolIncentive, "incentive_type", EolIncentiveParam::kNoOffer,
    &eol_incentive_options};

BASE_FEATURE(kEolIncentiveSettings,
             "EolIncentiveSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable support for touchpad with haptic feedback.
BASE_FEATURE(kExoHapticFeedbackSupport,
             "ExoHapticFeedbackSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables version 3 of the zwp_linux_dmabuf_v1 Wayland protocol.
// This version adds support for DRM modifiers and is required by Mesas Vulkan
// WSI, which otherwise falls back to software rendering.
BASE_FEATURE(kExoLinuxDmabufV3,
             "ExoLinuxDmabufV3",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables version 4 of the zwp_linux_dmabuf_v1 Wayland protocol.
// This version adds support for dynamic feedback, allowing the compositor to
// give clients hints about more optimal DRM formats and modifiers depending on
// e.g. available KMS hardware planes.
BASE_FEATURE(kExoLinuxDmabufV4,
             "ExoLinuxDmabufV4",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending explicit modifiers for the zwp_linux_dmabuf_v1 Wayland
// protocol. This option only has an effect with version 3 or 4 of the protocol.
// If disabled only the DRM_FORMAT_MOD_INVALID modifier will be send,
// effectively matching version 2 behavior more closely.
BASE_FEATURE(kExoLinuxDmabufModifiers,
             "ExoLinuxDmabufModifiers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
BASE_FEATURE(kExoOrdinalMotion,
             "ExoOrdinalMotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows RGB Keyboard to test new animations/patterns.
BASE_FEATURE(kExperimentalRgbKeyboardPatterns,
             "ExperimentalRgbKeyboardPatterns",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables extended updates opt-in functionality.
BASE_FEATURE(kExtendedUpdatesOptInFeature,
             "ExtendedUpdatesOptInFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables policy that controls feature to allow Family Link accounts on school
// owned devices.
BASE_FEATURE(kFamilyLinkOnSchoolDevice,
             "FamilyLinkOnSchoolDevice",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Fast Pair feature.
BASE_FEATURE(kFastPair, "FastPair", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables logic for handling BLE address rotations during retroactive pair
// scenarios.
BASE_FEATURE(kFastPairBleRotation,
             "FastPairBleRotation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Sets mode to DEBUG when fetching metadata from the Nearby server, allowing
// debug devices to trigger Fast Pair notifications.
BASE_FEATURE(kFastPairDebugMetadata,
             "FastPairDebugMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using longterm Handshake retry logic for Fast Pair.
BASE_FEATURE(kFastPairHandshakeLongTermRefactor,
             "FastPairHandshakeLongTermRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables prototype support for Fast Pair HID (non-keyboard).
BASE_FEATURE(kFastPairHID, "FastPairHID", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables prototype support for Fast Pair for keyboards.
BASE_FEATURE(kFastPairKeyboards,
             "FastPairKeyboards",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Saved Devices nicknames logic for Fast Pair.
BASE_FEATURE(kFastPairSavedDevicesNicknames,
             "FastPairSavedDevicesNicknames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The amount of minutes we should wait before allowing notifications for a
// recently lost device.
const base::FeatureParam<double> kFastPairDeviceLostNotificationTimeoutMinutes{
    &kFastPair, "fast-pair-device-lost-notification-timeout-minutes", 5};

// Enables Fast Pair sub feature to prevent notifications for recently lost
// devices for |kFastPairDeviceLostNotificationTimeout|.
BASE_FEATURE(kFastPairPreventNotificationsForRecentlyLostDevice,
             "FastPairPreventNotificationsForRecentlyLostDevice",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables link to Progressive Web Application companion app to configure
// Pixel Buds after Fast Pair.
BASE_FEATURE(kFastPairPwaCompanion,
             "FastPairPwaCompanion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The URI for the Pixel Buds Fast Pair web companion.
const base::FeatureParam<std::string> kFastPairPwaCompanionInstallUri{
    &kFastPairPwaCompanion, "pwa-companion-install-uri",
    /*default*/ "https://mypixelbuds.google.com/"};

// (optional) The app ID for the installed Pixel Buds Fast Pair web
// companion.
const base::FeatureParam<std::string> kFastPairPwaCompanionAppId{
    &kFastPairPwaCompanion, "pwa-companion-app-id",
    /*default*/ "ckdjfcfapbgminighllemapmpdlpihia"};

// (optional) The Play Store link to download the Pixel Buds Fast Pair
// web companion.
const base::FeatureParam<std::string> kFastPairPwaCompanionPlayStoreUri{
    &kFastPairPwaCompanion, "pwa-companion-play-store-uri",
    /*default*/
    "https://play.google.com/store/apps/"
    "details?id=com.google.android.apps.wearables.maestro.companion"};

// Comma separated list of Device IDs that the Pixel Buds companion app
// supports.
const base::FeatureParam<std::string> kFastPairPwaCompanionDeviceIds{
    &kFastPairPwaCompanion, "pwa-companion-device-ids",
    /*default*/
    "08A97F,5A36A5,6EDAF7,9ADB11,A7D7A0,C8E228,D87A3E,F2020E,F58DE7,30346C,"
    "7862CE,C193F7,05D40E,02FC97,AB442D,FB19ED,C55C79,2EE57B"};

// Enables the "Saved Devices" Fast Pair page in scenario in Bluetooth Settings.
BASE_FEATURE(kFastPairSavedDevices,
             "FastPairSavedDevices",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the "Saved Devices" Fast Pair strict interpretation of opt-in status,
// meaning that a user's preferences determine if retroactive pairing and
// subsequent pairing scenarios are enabled.
BASE_FEATURE(kFastPairSavedDevicesStrictOptIn,
             "FastPairSavedDevicesStrictOptIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Fast Pair Devices in Bluetooth Settings page.
BASE_FEATURE(kFastPairDevicesBluetoothSettings,
             "FastPairDevicesBluetoothSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows the creation of up to 16 desks (default is 8). This flag
// is intended to be controlled by the feature management module.
BASE_FEATURE(kFeatureManagement16Desks,
             "FeatureManagement16Desks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows borealis on certain boards whose features are determined by
// FeatureManagement. This feature does not apply to all boards, and does not
// guarantee borealis will be available (due to additional hardware checks).
BASE_FEATURE(kFeatureManagementBorealis,
             "FeatureManagementBorealis",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts some content in the Help app to the intended target population.
// This flag is only intended to be modified by the feature management module.
BASE_FEATURE(kFeatureManagementShowoff,
             "FeatureManagementShowoff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts the time-of-day wallpaper/screensaver features to the intended
// target population, whereas the `kTimeOfDayScreenSaver|Wallpaper` flags
// control the feature's rollout within said target population. These flags are
// only intended to be modified by the feature_management module.
BASE_FEATURE(kFeatureManagementTimeOfDayScreenSaver,
             "FeatureManagementTimeOfDayScreenSaver",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureManagementTimeOfDayWallpaper,
             "FeatureManagementTimeOfDayWallpaper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated service. If enabled, launches federated service when
// user first login.
BASE_FEATURE(kFederatedService,
             "FederatedService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the federated service to schedule tasks. If disabled, federated
// service works as a simple example receiver and storage.
// This is useful when we want to disable the federated tasks only and allow the
// customers to report examples, because e.g. the tensorflow graphs cost too
// much resources while example storage is supposed to be cheap and safe.
BASE_FEATURE(kFederatedServiceScheduleTasks,
             "FederatedServiceScheduleTasks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the federated strings service.
BASE_FEATURE(kFederatedStringsService,
             "FederatedStringsService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated strings service to schedule tasks.
BASE_FEATURE(kFederatedStringsServiceScheduleTasks,
             "FederatedStringsServiceScheduleTasks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables scheduling of launcher query federated analytics version 2 tasks.
BASE_FEATURE(kFederatedLauncherQueryAnalyticsVersion2Task,
             "FederatedLauncherQueryAnalyticsVersion2Task",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the files transfer conflict dialog in Files app.
BASE_FEATURE(kFilesConflictDialog,
             "FilesConflictDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the kernel drivers (instead of the FUSE mounters) for the exFAT and
// NTFS filesystems on systems that support them (b/358446133).
// TODO(b/364409158) Remove this feature.
BASE_FEATURE(kFilesKernelDrivers,
             "FilesKernelDrivers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables local image search by query in the Files app.
BASE_FEATURE(kFilesLocalImageSearch,
             "FilesLocalImageSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables materialized views in Files App.
BASE_FEATURE(kFilesMaterializedViews,
             "FilesMaterializedViews",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partitioning of removable disks in file manager.
BASE_FEATURE(kFilesSinglePartitionFormat,
             "FilesSinglePartitionFormat",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable files app trash for Drive.
BASE_FEATURE(kFilesTrashDrive,
             "FilesTrashDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the v2 version of the Firmware Updates app.
BASE_FEATURE(kFirmwareUpdateUIV2,
             "FirmwareUpdateUIV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables first party Vietnamese input method.
BASE_FEATURE(kFirstPartyVietnameseInput,
             "FirstPartyVietnameseInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the Flex Auto-Enrollment feature on ChromeOS
BASE_FEATURE(kFlexAutoEnrollment,
             "FlexAutoEnrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables firmware updates from LVFS for ChromeOS Flex.
BASE_FEATURE(kFlexFirmwareUpdate,
             "FlexFirmwareUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls Floating SSO feature which can move cookies between ChromeOS
// enterprise devices. The feature is also guarded by an enterprise policy. This
// flag controls if we are allowed to launch the service observing the policy
// and if we show the user selectable UI when the policy is enabled.
BASE_FEATURE(kFloatingSso, "FloatingSso", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Floating Workspace feature on ChromeOS
BASE_FEATURE(kFloatingWorkspace,
             "FloatingWorkspace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Maximum delay to wait for restoring Floating Workspace after login.
constexpr base::FeatureParam<base::TimeDelta>
    kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin{
        &kFloatingWorkspace, "MaxTimeAvailableForRestoreAfterLogin",
        base::Seconds(3)};

// Enables or disables Floating Workspace V2 feature on ChromeOS
BASE_FEATURE(kFloatingWorkspaceV2,
             "FloatingWorkspaceV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Maximum delay to wait for restoring Floating Workspace V2 after login.
constexpr base::FeatureParam<base::TimeDelta>
    kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin{
        &kFloatingWorkspaceV2, "MaxTimeAvailableForRestoreAfterLoginV2",
        base::Seconds(30)};

// Time interval to capture current desk as desk template and upload template to
// server.
constexpr base::FeatureParam<base::TimeDelta>
    kFloatingWorkspaceV2PeriodicJobIntervalInSeconds{
        &kFloatingWorkspaceV2, "PeriodicJobIntervalInSeconds",
        base::Seconds(30)};

// Enables or disables Focus Mode feature on ChromeOS.
BASE_FEATURE(kFocusMode, "FocusMode", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Focus Mode YTM integration on ChromeOS.
BASE_FEATURE(kFocusModeYTM, "FocusModeYTM", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, makes the Projector app use server side speech
// recognition instead of on-device speech recognition.
BASE_FEATURE(kForceEnableServerSideSpeechRecognitionForDev,
             "ForceEnableServerSideSpeechRecognitionForDev",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force enables on-device apps controls regardless of the device region.
// Used for development and testing only. Should remain disabled by default.
// See `kOnDeviceAppControls` description for the feature details.
BASE_FEATURE(kForceOnDeviceAppControlsForAllRegions,
             "ForceOnDeviceAppControlsForAllRegions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Drive to forcibly resync office files. Operations such as copy,
// move, ZIP on MS Office files call on the Drive to resync the files.
BASE_FEATURE(kForceReSyncDrive,
             "ForceReSyncDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling/disabling the forest feature.
// For more info, see go/crosforest.
BASE_FEATURE(kForestFeature, "ForestFeature", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow keeping full screen mode after unlock.
BASE_FEATURE(kFullscreenAfterUnlockAllowed,
             "FullscreenAfterUnlockAllowed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, there will be an alert bubble showing up when the device
// returns from low brightness (e.g., sleep, closed cover) without a lock screen
// and the active window is in fullscreen.
// TODO(crbug.com/40140761): Remove this after the feature is launched.
BASE_FEATURE(kFullscreenAlertBubble,
             "EnableFullscreenBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Debugging UI for ChromeOS FuseBox service.
BASE_FEATURE(kFuseBoxDebug, "FuseBoxDebug", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the fwupd dbus client should be active. This is used only
// for testing to prevent the fwupd service from spooling and re-activating
// powerd service.
BASE_FEATURE(kBlockFwupdClient,
             "BlockFwupdClient",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Classroom Student Glanceable on time management surface.
BASE_FEATURE(kGlanceablesTimeManagementClassroomStudentView,
             "GlanceablesTimeManagementClassroomStudentView",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Tasks Glanceable on time management surface.
BASE_FEATURE(kGlanceablesTimeManagementTasksView,
             "GlanceablesTimeManagementTasksView",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fetching assigned (shared) tasks for Google Tasks integration.
BASE_FEATURE(kGlanceablesTimeManagementTasksViewAssignedTasks,
             "GlanceablesTimeManagementTasksViewAssignedTasks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables logging new Gaia account creation event.
BASE_FEATURE(kGaiaRecordAccountCreation,
             "GaiaRecordAccountCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Game Dashboard.
BASE_FEATURE(kGameDashboard, "GameDashboard", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Gamepad Support.
BASE_FEATURE(kGameDashboardGamepadSupport,
             "GameDashboardGamepadSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Game Dashboard for additional PWA games.
BASE_FEATURE(kGameDashboardGamePWAs,
             "GameDashboardGamePWAs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables additional games being evaluated for the Game Dashboard.
BASE_FEATURE(kGameDashboardGamesInTest,
             "GameDashboardGamesInTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Game Dashboard Main Menu utility views.
BASE_FEATURE(kGameDashboardUtilities,
             "GameDashboardUtilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Game Dashboard's Record Game feature. This flag is to be enabled
// by the feature management module.
BASE_FEATURE(kFeatureManagementGameDashboardRecordGame,
             "FeatureManagementGameDashboardRecordGame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls gamepad vibration in Exo.
BASE_FEATURE(kGamepadVibration,
             "ExoGamepadVibration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable a D-Bus service for accessing gesture properties.
BASE_FEATURE(kGesturePropertiesDBusService,
             "GesturePropertiesDBusService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ability to record the screen into an animated GIF image from the
// native screen capture tool.
BASE_FEATURE(kGifRecording, "GifRecording", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Graduation app for EDU users if the Graduation policy allows it.
BASE_FEATURE(kGraduation, "Graduation", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a Files banner about Google One offer. This flag is used by Gamgee
// nudge to conditionally disable the G1 file banner for CBX boards via finch.
BASE_FEATURE(kGoogleOneOfferFilesBanner,
             "GoogleOneOfferFilesBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables targeting for feature-aware devices, as controlled by the feature
// management module.
BASE_FEATURE(kFeatureManagementGrowthFramework,
             "FeatureManagementGrowthFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables growth framework.
BASE_FEATURE(kGrowthFramework,
             "GrowthFramework",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable set app window as nudge parent.
BASE_FEATURE(kGrowthCampaignsNudgeParentToAppWindow,
             "GrowthCampaignsNudgeParentToAppWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables CrOS events recording with growth campaigns.
BASE_FEATURE(kGrowthCampaignsCrOSEvents,
             "GrowthCampaignsCrOSEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns experiment tag targeting is enabled. The
// flag also used by finch to tag the session with finch params.
BASE_FEATURE(kGrowthCampaignsExperimentTagTargeting,
             "GrowthCampaignsExperimentTagTargeting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of predefined Growth Framework experiment flag that will be associated
// with a finch study to deliver finch param for each experiment group to
// create randomization group that match the experiment tag targeting in
// Growth campaigns.
// The group will be selected by `predefinedFeatureIndex` config in experimental
// campaigns.
BASE_FEATURE(kGrowthCampaignsExperiment1,
             "GrowthCampaignsExperiment1",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment2,
             "GrowthCampaignsExperiment2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment3,
             "GrowthCampaignsExperiment3",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment4,
             "GrowthCampaignsExperiment4",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment5,
             "GrowthCampaignsExperiment5",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment6,
             "GrowthCampaignsExperiment6",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment7,
             "GrowthCampaignsExperiment7",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment8,
             "GrowthCampaignsExperiment8",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment9,
             "GrowthCampaignsExperiment9",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment10,
             "GrowthCampaignsExperiment10",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment11,
             "GrowthCampaignsExperiment11",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment12,
             "GrowthCampaignsExperiment12",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment13,
             "GrowthCampaignsExperiment13",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment14,
             "GrowthCampaignsExperiment14",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment15,
             "GrowthCampaignsExperiment15",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment16,
             "GrowthCampaignsExperiment16",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment17,
             "GrowthCampaignsExperiment17",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment18,
             "GrowthCampaignsExperiment18",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment19,
             "GrowthCampaignsExperiment19",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment20,
             "GrowthCampaignsExperiment20",
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of one-off Growth Framework experiment flag that will be associated
// with a finch study to deliver finch param for each experiment group to
// create randomization group that match the experiment tag targeting in
// Growth campaigns.
// The group will be selected by `oneOffExpFeatureIndex` config in experimental
// campaigns.
// Different from the predefined feature flag section above. These flags are
// used by study/groups that refer to multiple feature flags.
BASE_FEATURE(kGrowthCampaignsExperimentFileAppGamgee,
             "GrowthCampaignsExperimentFileAppGamgee",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperimentG1Nudge,
             "GrowthCampaignsExperimentG1Nudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables consumer session customizations with growth campaigns.
BASE_FEATURE(kGrowthCampaignsInConsumerSession,
             "GrowthCampaignsInConsumerSession",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Demo Mode customizations with growth campaigns.
BASE_FEATURE(kGrowthCampaignsInDemoMode,
             "GrowthCampaignsInDemoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Show the nudge widget inside the window bounds and parent to the window.
BASE_FEATURE(kGrowthCampaignsShowNudgeInsideWindowBounds,
             "GrowthCampaignsShowNudgeInsideWindowBounds",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering when loading campaigns complete.
BASE_FEATURE(kGrowthCampaignsTriggerAtLoadComplete,
             "GrowthCampaignsTriggerAtLoadComplete",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by app open event is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByAppOpen,
             "GrowthCampaignsTriggerByAppOpen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by url navigation is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByBrowser,
             "GrowthCampaignsTriggerByBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by any event is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByEvent,
             "GrowthCampaignsTriggerByEvent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by recording an event.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByRecordEvent,
             "GrowthCampaignsTriggerByRecordEvent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the growth nudge's triggering and the nudge widget
// invisibility and inactivation event should be observed to conditionally
// cancel the nudge.
BASE_FEATURE(kGrowthCampaignsObserveTriggeringWidgetChange,
             "GrowthCampaignsObserveTriggeringWidgetChange",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables internals page of ChromeOS growth framework.
BASE_FEATURE(kGrowthInternals,
             "GrowthInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing the menu tabs in chrome://healthd-internals for displaying
// information from `cros_healthd`.
BASE_FEATURE(kHealthdInternalsTabs,
             "HealthdInternalsTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Help app will render the App Detail Page and entry point.
BASE_FEATURE(kHelpAppAppDetailPage,
             "HelpAppAppDetailPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Help app will render the Apps List page and entry point.
BASE_FEATURE(kHelpAppAppsList,
             "HelpAppAppsList",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the logic that auto triggers the install dialog during the web app
// install flow initiated from the Help App.
BASE_FEATURE(kHelpAppAutoTriggerInstallDialog,
             "HelpAppAutoTriggerInstallDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the home page of the Help App will show a section containing
// articles about apps.
BASE_FEATURE(kHelpAppHomePageAppArticles,
             "HelpAppHomePageAppArticles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable showing search results from the help app in the launcher.
BASE_FEATURE(kHelpAppLauncherSearch,
             "HelpAppLauncherSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new onboarding experience in the Help App.
BASE_FEATURE(kHelpAppOnboardingRevamp,
             "HelpAppOnboardingRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables opening the Help App's What's New page immediately instead of showing
// a notification to open the help app.
BASE_FEATURE(kHelpAppOpensInsteadOfReleaseNotesNotification,
             "HelpAppOpensInsteadOfReleaseNotesNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable showing the welcome tips page in the help app. This feature
// is dependent on the 'ScalableIph' feature being enabled as well.
BASE_FEATURE(kHelpAppWelcomeTips,
             "HelpAppWelcomeTips",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable ChromeOS hibernation features.
BASE_FEATURE(kHibernate, "Hibernate", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables image search for productivity launcher.
BASE_FEATURE(kProductivityLauncherImageSearch,
             "ProductivityLauncherImageSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
BASE_FEATURE(kHiddenNetworkWarning,
             "HiddenNetworkWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy). This feature also enables "contextual nudges" for
// gesture education.
BASE_FEATURE(kHideShelfControlsInTabletMode,
             "HideShelfControlsInTabletMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, add Hindi Inscript keyboard layout.
BASE_FEATURE(kHindiInscriptLayout,
             "HindiInscriptLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables suggestions in the pinned files section of Holding Space.
BASE_FEATURE(kHoldingSpaceSuggestions,
             "HoldingSpaceSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHomeButtonQuickAppAccess,
             "HomeButtonQuickAppAccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a call-to-action label beside the home button.
BASE_FEATURE(kHomeButtonWithText,
             "HomeButtonWithText",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows the user to cycle between windows of an app using Alt + `.
BASE_FEATURE(kSameAppWindowCycle,
             "SameAppWindowCycle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Make Sanitize available. This feature provides a "soft reset" option in CrOS
// settings. This soft reset will disable extensions and reset some of the
// settings to default.
BASE_FEATURE(kSanitize, "CrosSanitize", base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, `SmbService` is created on user session startup task completed.
BASE_FEATURE(kSmbServiceIsCreatedOnUserSessionStartUpTaskCompleted,
             "SmbServiceIsCreatedOnUserSessionStartUpTaskCompleted",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, smbprovider is started on-demand.
BASE_FEATURE(kSmbproviderdOnDemand,
             "SmbproviderdOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the snooping protection prototype is enabled.
BASE_FEATURE(kSnoopingProtection,
             "SnoopingProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the split keyboard refactor cleanup.
BASE_FEATURE(kSplitKeyboardRefactor,
             "SplitKeyboardRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to start AssistantAudioDecoder service on demand (at query
// response time).
BASE_FEATURE(kStartAssistantAudioDecoderOnDemand,
             "StartAssistantAudioDecoderOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, used to configure the heuristic rules for some advanced IME
// features (e.g. auto-correct).
BASE_FEATURE(kImeRuleConfig, "ImeRuleConfig", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables IME downloader experiment logic.
BASE_FEATURE(kImeDownloaderExperiment,
             "ImeDownloaderExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use the updated logic for downloading IME artifacts.
BASE_FEATURE(kImeDownloaderUpdate,
             "ImeDownloaderUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, use the updated parameters for the decoder.
BASE_FEATURE(kImeFstDecoderParamsUpdate,
             "ImeFstDecoderParamsUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled use experimental US English IME language model.
BASE_FEATURE(kImeUsEnglishExperimentalModel,
             "ImeUsEnglishExperimentalModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled use the updated US English IME language models.
BASE_FEATURE(kImeUsEnglishModelUpdate,
             "ImeUsEnglishModelUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker GIF support
BASE_FEATURE(kImeSystemEmojiPickerGIFSupport,
             "SystemEmojiPickerGIFSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker jelly support
BASE_FEATURE(kImeSystemEmojiPickerJellySupport,
             "SystemEmojiPickerJellySupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker mojo based emoji search
BASE_FEATURE(kImeSystemEmojiPickerMojoSearch,
             "SystemEmojiPickerMojoSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable system emoji picker global emoji variant grouping
BASE_FEATURE(kImeSystemEmojiPickerVariantGrouping,
             "SystemEmojiPickerVariantGrouping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables debug info UI for Korean input method's internal-mode switch.
BASE_FEATURE(kImeKoreanModeSwitchDebug,
             "ImeKoreanModeSwitchDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a change in the Korean input method whereby right-Alt location solely
// functions as the internal-mode switch, i.e. not concurrently as Alt modifier.
BASE_FEATURE(kImeKoreanOnlyModeSwitchOnRightAlt,
             "ImeKoreanOnlyModeSwitchOnRightAlt",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a change in the IME switching logic such that the mojo connection
// status is tracked via a global boolean instead of checking if the runner is
// idle.
BASE_FEATURE(kImeSwitchCheckConnectionStatus,
             "ImeSwitchCheckConnectionStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show new improved UI for cryptohome errors that happened
// during login. UI contains links to help center and might provide actions
// that can be taken to resolve the problem.
BASE_FEATURE(kImprovedLoginErrorHandling,
             "ImprovedLoginErrorHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show new management disclosure UI page instead of the
// management warning bubble.
BASE_FEATURE(kImprovedManagementDisclosure,
             "ImprovedManagementDisclosure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Instant Hotspot on Nearby. b/303121363.
BASE_FEATURE(kInstantHotspotOnNearby,
             "InstantHotspotOnNearby",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Instant Hotspot rebrand/feature improvements. b/290075504.
BASE_FEATURE(kInstantHotspotRebrand,
             "InstantHotspotRebrand",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Instant Tethering on ChromeOS.
BASE_FEATURE(kInstantTethering,
             "InstantTethering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// Controls the launched locales.
BASE_FEATURE(kInternalServerSideSpeechRecognition,
             "InternalServerSideSpeechRecognition",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature overrides the `InternalServerSideSpeechRecognition` that is exposed
// via chrome://flags. This flag is used as a kill switch to disable the feature
// in case that the feature introduced unexpected server load.
// TODO(b/265957535) Clean up this flag after launch.
BASE_FEATURE(kInternalServerSideSpeechRecognitionControl,
             "InternalServerSideSpeechRecognitionControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// The supported locales for this feature are specified using the locales
// filter in finch config.
BASE_FEATURE(kInternalServerSideSpeechRecognitionByFinch,
             "InternalServerSideSpeechRecognitionByFinch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// The supported locales for this feature are specified using the locales
// filter in finch config. The languages controlled by this feature use the
// S3 USM_RNNT model.
BASE_FEATURE(kInternalServerSideSpeechRecognitionUSMModelFinch,
             "InternalServerSideSpeechRecognitionUSMModelFinch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending `client-info` values to IPP printers on ChromeOS.
BASE_FEATURE(kIppClientInfo, "IppClientInfo", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables new experimental IPP-first setup path for USB printers on ChromeOS.
// Used in finch experiment.
BASE_FEATURE(kIppFirstSetupForUsbPrinters,
             "IppFirstSetupForUsbPrinters",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Kiosk sessions with Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppKiosk,
             "IsolatedWebAppKiosk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables correct handling of the function key row in Japanese.
BASE_FEATURE(kJapaneseFunctionRow,
             "JapaneseFunctionRow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OS Settings page for japanese.
BASE_FEATURE(kJapaneseOSSettings,
             "JapaneseOSSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the "Remember password" button in the Kerberos "Add account"
// dialog should be checked by default.
BASE_FEATURE(kKerberosRememberPasswordByDefault,
             "KerberosRememberPasswordByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables IME button in the floating accessibility menu for the Kiosk session.
BASE_FEATURE(kKioskEnableImeButton,
             "KioskEnableImeButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables System Web Apps for the Kiosk session.
BASE_FEATURE(kKioskEnableSystemWebApps,
             "KioskEnableSystemWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, wayland logging is enabled for Lacros.
BASE_FEATURE(kLacrosWaylandLogging,
             "LacrosWaylandLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, it is allowed to migrate data from lacros back to ash, provided
// that other conditions are also met (e.g. the policy is enabled, or the
// command line flag is passed).
BASE_FEATURE(kLacrosProfileBackwardMigration,
             "LacrosProfileBackwardMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automatic downloading and installing fonts via language packs, based
// on the user's preferences.
BASE_FEATURE(kLanguagePacksFonts,
             "LanguagePacksFonts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables loading in fonts via language packs on login, even after a download.
const base::FeatureParam<bool> kLanguagePacksFontsLoadAfterDownloadDuringLogin =
    {&kLanguagePacksFonts, "load_after_download_during_login", true};

// If enabled, the Language Pack corresponding to the application locale is
// downloaded and installed during OOBE. This pre-fetching is aimed at improving
// user experience so that they have language resources available as early as
// possible.
BASE_FEATURE(kLanguagePacksInOobe,
             "LanguagePacksInOobe",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the UI and relative logic to manage Language Packs in Settings.
// This feature allows users to install/remove languages and input methods
// via the corresponding Settings page.
BASE_FEATURE(kLanguagePacksInSettings,
             "LanguagePacksInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using lacros's extension printers from ash when lacros is enabled.
BASE_FEATURE(kLacrosExtensionPrinting,
             "LacrosExtensionPrinting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, launcher continue section will suggest drive files based on
// recency, instead of fetching them using drive's ItemSuggest API.
BASE_FEATURE(kLauncherContinueSectionWithRecents,
             "LauncherContinueSectionWithRecents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Same as `kLauncherContinueSectionWithRecents`, but used to enable the feature
// via finch, while ensuring minimum Chrome version - i.e. to avoid finch config
// from enabling the feature on versions where
// LauncherContinueSectionWithRecents was first added.
BASE_FEATURE(kLauncherContinueSectionWithRecentsRollout,
             "LauncherContinueSectionWithRecentsRollout125",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses short intervals for launcher nudge for testing if enabled.
BASE_FEATURE(kLauncherNudgeShortInterval,
             "LauncherNudgeShortInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the launcher nudge prefs will be reset at the start of each new
// user session.
BASE_FEATURE(kLauncherNudgeSessionReset,
             "LauncherNudgeSessionReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the launcher will only provide results based on the user control.
BASE_FEATURE(kLauncherSearchControl,
             "LauncherSearchControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Segmentation flag for local image search.
BASE_FEATURE(kFeatureManagementLocalImageSearch,
             "FeatureManagementLocalImageSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lobster feature.
BASE_FEATURE(kLobster, "Lobster", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lobster dogfood.
BASE_FEATURE(kLobsterDogfood,
             "LobsterDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables / Disables the lobster feature from the feature management module.
BASE_FEATURE(kFeatureManagementLobster,
             "FeatureManagementLobster",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables local image search on core devices.
BASE_FEATURE(kLocalImageSearchOnCore,
             "LocalImageSearchOnCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables cross device supported reports within the feedback tool.
// (This feature is only available for dogfooders)
BASE_FEATURE(kLinkCrossDeviceDogfoodFeedback,
             "LinkCrossDeviceDogFoodFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables nearby-internals logs to be automatically saved to disk and attached
// to feedback reports.
BASE_FEATURE(kLinkCrossDeviceInternals,
             "LinkCrossDeviceInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
BASE_FEATURE(kLockScreenHideSensitiveNotificationsSupport,
             "LockScreenHideSensitiveNotificationsSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
BASE_FEATURE(kLockScreenInlineReply,
             "LockScreenInlineReply",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notifications on the lock screen.
BASE_FEATURE(kLockScreenNotifications,
             "LockScreenNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to allow MAC address randomization to be enabled for WiFi networks.
BASE_FEATURE(kMacAddressRandomization,
             "MacAddressRandomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Mahi on PDF contents in the Media App.
BASE_FEATURE(kMediaAppPdfMahi,
             "MediaAppPdfMahi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable the requirement of a minimum chrome version on the
// device through the policy DeviceMinimumVersion. If the requirement is
// not met and the warning time in the policy has expired, the user is
// restricted from using the session.
BASE_FEATURE(kMinimumChromeVersion,
             "MinimumChromeVersion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
BASE_FEATURE(kMojoDBusRelay,
             "MojoDBusRelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to split left and right modifiers in settings.
BASE_FEATURE(kModifierSplit, "ModifierSplit", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the ability to potentially override the secret key based on some
// factors.
BASE_FEATURE(kModifierSplitDeviceEnabled,
             "ModifierSplitDeviceEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to split left and right modifiers in settings for dogfood.
BASE_FEATURE(kModifierSplitDogfood,
             "ModifierSplitDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to split left and right modifiers in settings.
BASE_FEATURE(kMouseImposterCheck,
             "MouseImposterCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the full apps list in Phone Hub bubble.
BASE_FEATURE(kEcheLauncher, "EcheLauncher", base::FEATURE_ENABLED_BY_DEFAULT);

// Switch full apps list in Phone Hub from grid view to list view.
BASE_FEATURE(kEcheLauncherListView,
             "EcheLauncherListView",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Switch the "More Apps" button in eche launcher to show small app icons
BASE_FEATURE(kEcheLauncherIconsInMoreAppsButton,
             "EcheLauncherIconsInMoreAppsButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Phone Hub recent apps loading and error views based on the
// connection status with the phone.
BASE_FEATURE(kEcheNetworkConnectionState,
             "EcheNetworkConnectionState",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Time limit before re-attempting a background connection to check if the
// network is suitable.
const base::FeatureParam<base::TimeDelta>
    kEcheBackgroundConnectionAttemptThrottleTimeout{
        &kEcheNetworkConnectionState,
        "EcheBackgroundConnectionAttemptThrottleTimeout", base::Seconds(10)};

// Time limit before requiring a new connection check to show apps UI.
const base::FeatureParam<base::TimeDelta> kEcheConnectionStatusResetTimeout{
    &kEcheNetworkConnectionState, "EcheConnectionStatusResetTimeout",
    base::Minutes(10)};

BASE_FEATURE(kEcheShorterScanningDutyCycle,
             "EcheShorterScanningDutyCycle",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kEcheScanningCycleOnTime{
    &kEcheShorterScanningDutyCycle, "EcheScanningCycleOnTime",
    base::Seconds(30)};

const base::FeatureParam<base::TimeDelta> kEcheScanningCycleOffTime{
    &kEcheShorterScanningDutyCycle, "EcheScanningCycleOffTime",
    base::Seconds(30)};

// Enables events from multiple calendars to be displayed in the Quick
// Settings Calendar.
BASE_FEATURE(kMultiCalendarSupport,
             "MultiCalendarSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables multi-zone rgb keyboard customization.
BASE_FEATURE(kMultiZoneRgbKeyboard,
             "MultiZoneRgbKeyboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Nearby Connections to specificy KeepAlive interval and timeout while
// also making the Nearby Connections WebRTC defaults longer.
BASE_FEATURE(kNearbyKeepAliveFix,
             "NearbyKeepAliveFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Nearby Presence for scanning and discovery of nearby devices.
BASE_FEATURE(kNearbyPresence,
             "NearbyPresence",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a limit on the number of notifications that can show.
BASE_FEATURE(kNotificationLimit,
             "NotificationLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Notifier Collision to allow popup notifications and tray bubbles not
// overlap when showing on a display.
BASE_FEATURE(kNotifierCollision,
             "NotifierCollision",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether new Lockscreen reauth layout is shown or not.
BASE_FEATURE(kNewLockScreenReauthLayout,
             "NewLockScreenReauthLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature Management flag for the Sys UI holdback experiment, used to avoid
// certain devices.
BASE_FEATURE(kFeatureManagementShouldExcludeFromSysUiHoldback,
             "FeatureManagementShouldExcludeFromSysUiHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Drive integration.
BASE_FEATURE(kSysUiShouldHoldbackDriveIntegration,
             "SysUiShouldHoldbackDriveIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Forcus Mode.
BASE_FEATURE(kSysUiShouldHoldbackFocusMode,
             "SysUiShouldHoldbackFocusMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Forest.
BASE_FEATURE(kSysUiShouldHoldbackForest,
             "SysUiShouldHoldbackForest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for GIF Recording.
BASE_FEATURE(kSysUiShouldHoldbackGifRecording,
             "SysUiShouldHoldbackGifRecording",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Task Management
// Glanceables.
BASE_FEATURE(kSysUiShouldHoldbackTaskManagement,
             "SysUiShouldHoldbackTaskManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Night Light feature.
BASE_FEATURE(kNightLight, "NightLight", base::FEATURE_ENABLED_BY_DEFAULT);

// Extracts controller logic from child views of `NotificationCenterView` to
// place it in a new `NotificationCenterController` class.
BASE_FEATURE(kNotificationCenterController,
             "NotificationCenterController",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled notification expansion animation.
BASE_FEATURE(kNotificationExpansionAnimation,
             "NotificationExpansionAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables dragging the image from a notification by mouse or gesture.
BASE_FEATURE(kNotificationImageDrag,
             "NotificationImageDrag",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables notification scroll bar in UnifiedSystemTray.
BASE_FEATURE(kNotificationScrollBar,
             "NotificationScrollBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notifications to be shown within context menus.
BASE_FEATURE(kNotificationsInContextMenu,
             "NotificationsInContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable on-device grammar check service.
BASE_FEATURE(kOnDeviceGrammarCheck,
             "OnDeviceGrammarCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the device supports on-device speech recognition.
// Forwarded to LaCrOS as BrowserInitParams::is_ondevice_speech_supported.
BASE_FEATURE(kOnDeviceSpeechRecognition,
             "OnDeviceSpeechRecognition",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the OneDrive upload flow should immediately prompt the user to
// re-authenticate without first showing a notification.
BASE_FEATURE(kOneDriveUploadImmediateReauth,
             "OneDriveUploadImmediateReauth",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the new UI for pinned notifications will be enabled.
// go/ongoing-ui
BASE_FEATURE(kOngoingProcesses,
             "OngoingProcesses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, enrollment screen will allow for automatically adding the
// authenticated user to the device.
BASE_FEATURE(kOobeAddUserDuringEnrollment,
             "OobeAddUserDuringEnrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, CHOBOE Screen will be shown during the new user onboarding flow.
BASE_FEATURE(kOobeChoobe, "OobeChoobe", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, CrOS events for OOBE and onboarding flow will be recorded.
BASE_FEATURE(kOobeCrosEvents,
             "OobeCrosEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Drive Pinning Screen will be shown during
// the new user onboarding flow.
BASE_FEATURE(kOobeDrivePinning,
             "OobeDrivePinning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled , Personalized Onboarding + App Recommendations
// will be shown if eligible during user onboarding flow.
BASE_FEATURE(kOobePersonalizedOnboarding,
             "OobePersonalizedOnboarding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Consumer Software Screen will be shown during OOBE.
BASE_FEATURE(kOobeSoftwareUpdate,
             "OobeSoftwareUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Gaia info screen in OOBE.
BASE_FEATURE(kOobeGaiaInfoScreen,
             "OobeGaiaInfoScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, TouchPadScreen will be shown in CHOOBE.
// enabling this without enabling OobeChoobe flag will have no effect
BASE_FEATURE(kOobeTouchpadScroll,
             "OobeTouchpadScrollDirection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOobeDisplaySize,
             "OobeDisplaySize",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, InputMethodsScreen will be shown in CHOOBE.
BASE_FEATURE(kOobeInputMethods,
             "OobeInputMethods",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SplitModifierKeyboardInfoScreen will be shown in OOBE.
BASE_FEATURE(kOobeSplitModifierKeyboardInfo,
             "OobeSplitModifierKeyboardInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE Jelly features.
BASE_FEATURE(kOobeJelly, "OobeJelly", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE Jelly modal features.
BASE_FEATURE(kOobeJellyModal,
             "OobeJellyModal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lazy loading in OOBE's WebUI by prioritizing the first screen.
BASE_FEATURE(kOobeLazyLoading,
             "OobeLazyLoading",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE perks discovery feature.
BASE_FEATURE(kOobePerksDiscovery,
             "OobePerksDiscovery",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE ai intro feature.
BASE_FEATURE(kFeatureManagementOobeAiIntro,
             "FeatureManagementOobeAiIntro",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OOBE gemini intro feature for testing.
BASE_FEATURE(kOobeGeminiIntroForTesting,
             "OobeGeminiIntroForTesting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE gemini intro feature.
BASE_FEATURE(kFeatureManagementOobeGeminiIntro,
             "FeatureManagementOobeGeminiIntro",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables boot animation feature.
BASE_FEATURE(kFeatureManagementOobeSimon,
             "FeatureManagementOobeSimon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Skipping the assistant setup screen in OOBE.
BASE_FEATURE(kOobeSkipAssistant,
             "OobeSkipAssistant",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the OOBE QuickStart flow.
BASE_FEATURE(kOobeQuickStart,
             "OobeQuickStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the OOBE QuickStart flow on the login screen.
BASE_FEATURE(kOobeQuickStartOnLoginScreen,
             "OobeQuickStartOnLoginScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables search customizable shortcuts in launcher.
BASE_FEATURE(kSearchCustomizableShortcutsInLauncher,
             "SearchCustomizableShortcutsInLauncher",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca for ARC apps.
BASE_FEATURE(kOrcaArc, "OrcaArc", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables elaborate for Orca.
BASE_FEATURE(kOrcaElaborate, "OrcaElaborate", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables emojify for Orca.
BASE_FEATURE(kOrcaEmojify, "OrcaEmojify", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca for managed users.
BASE_FEATURE(kOrcaForManagedUsers,
             "kOrcaForManagedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables formalize for Orca.
BASE_FEATURE(kOrcaFormalize, "OrcaFormalize", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables proofread for Orca.
BASE_FEATURE(kOrcaProofread, "OrcaProofread", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables rephrase for Orca.
BASE_FEATURE(kOrcaRephrase, "OrcaRephrase", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables shorten for Orca.
BASE_FEATURE(kOrcaShorten, "OrcaShorten", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables elaborate for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeElaborate,
             "OrcaInternationalizeElaborate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables emojify for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeEmojify,
             "OrcaInternationalizeEmojify",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables formalize for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeFormalize,
             "OrcaInternationalizeFormalize",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables proofread for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeProofread,
             "OrcaInternationalizeProofread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables rephrase for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeRephrase,
             "OrcaInternationalizeRephrase",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables shorten for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeShorten,
             "OrcaInternationalizeShorten",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Africaans support for Orca.
BASE_FEATURE(kOrcaAfrikaans, "OrcaAfrikaans", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Danish support for Orca.
BASE_FEATURE(kOrcaDanish, "OrcaDanish", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Dutch support for Orca.
BASE_FEATURE(kOrcaDutch, "OrcaDutch", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Finnish support for Orca.
BASE_FEATURE(kOrcaFinnish, "OrcaFinnish", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables French support for Orca.
BASE_FEATURE(kOrcaFrench, "OrcaFrench", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables German support for Orca.
BASE_FEATURE(kOrcaGerman, "OrcaGerman", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Italian support for Orca.
BASE_FEATURE(kOrcaItalian, "OrcaItalian", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Japanese support for Orca.
BASE_FEATURE(kOrcaJapanese, "OrcaJapanese", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Norwegian support for Orca.
BASE_FEATURE(kOrcaNorwegian, "OrcaNorwegian", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Polish support for Orca.
BASE_FEATURE(kOrcaPolish, "OrcaPolish", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Portugese support for Orca.
BASE_FEATURE(kOrcaPortugese, "OrcaPortugese", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Spanish support for Orca.
BASE_FEATURE(kOrcaSpanish, "OrcaSpanish", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Swedish support for Orca.
BASE_FEATURE(kOrcaSwedish, "OrcaSwedish", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca dragging support.
BASE_FEATURE(kOrcaDraggingSupport,
             "OrcaDraggingSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca capability check.
BASE_FEATURE(kOrcaUseAccountCapabilities,
             "OrcaUseAccountCapabilities",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca on Workspace.
BASE_FEATURE(kOrcaForceFetchContextOnGetEditorPanelContext,
             "OrcaForceFetchContextOnGetEditorPanelContext",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we force fetching input context
BASE_FEATURE(kOrcaOnWorkspace,
             "OrcaOnWorkspace",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables new Orca service connection logic.
BASE_FEATURE(kOrcaServiceConnection,
             "OrcaServiceConnection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Orca will only be available in English locales.
BASE_FEATURE(kOrcaOnlyInEnglishLocales,
             "OrcaOnlyInEnglishLocales",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Orca resizing support.
BASE_FEATURE(kOrcaResizingSupport,
             "OrcaResizingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Orca on Demo mode.
BASE_FEATURE(kOrcaSupportDemoMode,
             "OrcaSupportDemoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, a new App Notifications subpage will appear in CrOS Apps section.
BASE_FEATURE(kOsSettingsAppNotificationsPage,
             "OsSettingsAppNotificationsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, OsSyncConsent Revamp will be shown.
// enabling this without enabling Lacros flag will have no effect
BASE_FEATURE(kOsSyncConsentRevamp,
             "OsSyncConsentRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the os feedback dialog will be used on OOBE and the login
// screeen.
BASE_FEATURE(kOsFeedbackDialog,
             "OsFeedbackDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DNS dialog in should be deprecated in Security and Privacy
// Settings page when the user toggles off the DNS button.
BASE_FEATURE(kOsSettingsDeprecateDnsDialog,
             "OsSettingsDeprecateDnsDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the wayfinding improvements for the ChromeOS Settings revamp
BASE_FEATURE(kOsSettingsRevampWayfinding,
             "OsSettingsRevampWayfinding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables staying in overview when navigating between desks using a swipe
// gesture or keyboard shortcut.
BASE_FEATURE(kOverviewDeskNavigation,
             "OverviewDeskNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Jelly colors and components to appear in the Parent Access Widget
// if jelly-colors is also enabled.
BASE_FEATURE(kParentAccessJelly,
             "ParentAccessJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature allows usage of passwordless flow in GAIA.
// (This feature is only available for consumer users)
BASE_FEATURE(kPasswordlessGaiaForConsumers,
             "PasswordlessGaiaForConsumers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a notification warning users that their Thunderbolt device is not
// supported on their CrOS device.
// TODO(crbug.com/40199811): Revisit this flag when there is a way to query
// billboard devices correctly.
BASE_FEATURE(kPcieBillboardNotification,
             "PcieBillboardNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Limits the items on the shelf to the ones associated with windows the
// currently active desk.
BASE_FEATURE(kPerDeskShelf, "PerDeskShelf", base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within ChromeOS.
BASE_FEATURE(kPhoneHub, "PhoneHub", base::FEATURE_ENABLED_BY_DEFAULT);

// Retry generating attestation certificates when there are failures.
BASE_FEATURE(kPhoneHubAttestationRetries,
             "PhoneHubAttestationRetries",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Camera Roll feature in Phone Hub, which allows users to access
// recent photos and videos taken on a connected Android device
BASE_FEATURE(kPhoneHubCameraRoll,
             "PhoneHubCameraRoll",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of seconds to wait before users can download the same photo
// from Camera Roll again.
const base::FeatureParam<base::TimeDelta> kPhoneHubCameraRollThrottleInterval{
    &kPhoneHubCameraRoll, "PhoneHubCameraRollThrottleInterval",
    base::Seconds(2)};

// Enable PhoneHub features setup error handling, which handles different
// setup response from remote phone device.
BASE_FEATURE(kPhoneHubFeatureSetupErrorHandling,
             "PhoneHubFeatureSetupErrorHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the incoming/ongoing call notification feature in Phone Hub.
BASE_FEATURE(kPhoneHubCallNotification,
             "PhoneHubCallNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubMonochromeNotificationIcons,
             "PhoneHubMonochromeNotificationIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determine whether we use revamped notifier to notify users to start
// onboarding to Phone Hub.
BASE_FEATURE(kPhoneHubOnboardingNotifierRevamp,
             "PhoneHubOnboardingNotifierRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Should we show nudge or notification to the user.
const base::FeatureParam<bool> kPhoneHubOnboardingNotifierUseNudge{
    &kPhoneHubOnboardingNotifierRevamp, "use_nudge", true};

const base::FeatureParam<
    PhoneHubNotifierTextGroup>::Option phone_hub_notifier_text_groups[] = {
    {PhoneHubNotifierTextGroup::kNotifierTextGroupA, "notifier_with_text_A"},
    {PhoneHubNotifierTextGroup::kNotifierTextGroupB, "notifier_with_text_B"},
};
// What text should we show to the user.
const base::FeatureParam<PhoneHubNotifierTextGroup> kPhoneHubNotifierTextGroup{
    &kPhoneHubOnboardingNotifierRevamp, "notifier_text_group",
    PhoneHubNotifierTextGroup::kNotifierTextGroupB,
    &phone_hub_notifier_text_groups};

// The length of time passing till we display nudge to users again
const base::FeatureParam<base::TimeDelta> kPhoneHubNudgeDelay{
    &kPhoneHubOnboardingNotifierRevamp, "nudge_delay", base::Hours(24)};

// Number of times nudge should be shown to user.
const base::FeatureParam<int> kPhoneHubNudgeTotalAppearancesAllowed{
    &kPhoneHubOnboardingNotifierRevamp, "nudge_total_appearances_allowed", 3};

// Determines up to how many minutes into user session multdevice setup
// notification can be shown.
const base::FeatureParam<base::TimeDelta>
    kMultiDeviceSetupNotificationTimeLimit{
        &kPhoneHubOnboardingNotifierRevamp,
        "MultiDeviceSetupNotificationTimitLimit", base::Minutes(5)};

BASE_FEATURE(kPhoneHubPingOnBubbleOpen,
             "PhoneHubPingOnBubbleOpen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of seconds to wait for ping response before disconnecting
const base::FeatureParam<base::TimeDelta> kPhoneHubPingTimeout{
    &kPhoneHubPingOnBubbleOpen, "PhoneHubPingTimeout", base::Seconds(5)};

BASE_FEATURE(kPhoneHubShortQuickActionPodsTitles,
             "PhoneHubShortQuickActionPodsTitles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new picker feature.
BASE_FEATURE(kPicker, "Picker", base::FEATURE_ENABLED_BY_DEFAULT);

// Always show the feature tour for Picker for debugging purposes.
BASE_FEATURE(kPickerAlwaysShowFeatureTour,
             "PickerAlwaysShowFeatureTour",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables filtering of link suggestions.
BASE_FEATURE(kPickerFilterLinks,
             "PickerFilterLinks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables GIF search in Picker.
BASE_FEATURE(kPickerGifs, "PickerGifs", base::FEATURE_DISABLED_BY_DEFAULT);

// Uses grid view for recent local files.
BASE_FEATURE(kPickerGrid, "PickerGrid", base::FEATURE_ENABLED_BY_DEFAULT);

// Show more recent files beyond 30 days.
BASE_FEATURE(kPickerRecentFiles,
             "PickerRecentFiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPipDoubleTapToResize,
             "PipDoubleTapToResize",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the tuck feature for Picture in Picture.
BASE_FEATURE(kPipTuck, "PipTuck", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
BASE_FEATURE(kPreferConstantFrameRate,
             "PreferConstantFrameRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, ChromeOS print preview app is available. Enabling does not
// replace the existing Chrome print preview UI, and will require an additional
// flag and pref configured to facilitate. See b/323421684 for more information.
BASE_FEATURE(kPrintPreviewCrosApp,
             "PrintPreviewCrosApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a bubble-based launcher in clamshell mode. Changes the suggestions
// that appear in the launcher in both clamshell and tablet modes. Removes pages
// from the apps grid. This feature was previously named "AppListBubble".
// https://crbug.com/1204551
BASE_FEATURE(kProductivityLauncher,
             "ProductivityLauncher",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Projector for managed users.
BASE_FEATURE(kProjectorManagedUser,
             "ProjectorManagedUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Projector app launches in debug mode, with more detailed
// error messages.
BASE_FEATURE(kProjectorAppDebug,
             "ProjectorAppDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Constrols whether fallback implementation is enabled when streaming
// connection fails for server side speech recognition.
BASE_FEATURE(kProjectorServerSideRecognitionFallbackImpl,
             "ProjectorServerSideRecognititionFallbackImpl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Projector use custom thumbnail in gallery page.
BASE_FEATURE(kProjectorCustomThumbnail,
             "kProjectorCustomThumbnail",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to ignore policy setting for enabling Projector for managed
// users.
BASE_FEATURE(kProjectorManagedUserIgnorePolicy,
             "ProjectorManagedUserIgnorePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show pseduo transcript that is shorter than the
// threshold.
BASE_FEATURE(kProjectorShowShortPseudoTranscript,
             "ProjectorShowShortPseudoTranscript",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to update the indexable text when metadata file gets
// uploaded.
BASE_FEATURE(kProjectorUpdateIndexableText,
             "ProjectorUpdateIndexableText",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable features that are not ready to enable by
// default but ready for internal testing.
BASE_FEATURE(kProjectorBleedingEdgeExperience,
             "ProjectorBleedingEdgeExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the transcript muting feature is enabled.
BASE_FEATURE(kProjectorMuting,
             "ProjectorMuting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether higher version transcripts should be redirected to PWA.
BASE_FEATURE(kProjectorRedirectToPwa,
             "ProjectorRedirectToPwa",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether projector V2 is enabled.
BASE_FEATURE(kProjectorV2, "ProjectorV2", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the transcript chapter title feature is enabled.
BASE_FEATURE(kProjectorTranscriptChapterTitle,
             "ProjectorTranscriptChapterTitle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use USM for serverside speech recognition for projector.
BASE_FEATURE(kProjectorUseUSMForS3,
             "ProjectorUseUSMForS3",
             base::FEATURE_DISABLED_BY_DEFAULT);

// controls whether projector uses dynamic colors.
BASE_FEATURE(kProjectorDynamicColors,
             "ProjectorDynamicColors",
             base::FEATURE_DISABLED_BY_DEFAULT);

// controls whether the projector app uses updated styles and ui components.
BASE_FEATURE(kProjectorGm3, "ProjectorGm3", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show promise icons during app installations.
BASE_FEATURE(kPromiseIcons, "PromiseIcons", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show promise icons during web app installations.
BASE_FEATURE(kPromiseIconsForWebApps,
             "PromiseIconsForWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the quick dim prototype is enabled.
BASE_FEATURE(kQuickDim, "QuickDim", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the smart reader feature is enabled.
BASE_FEATURE(kSmartReader, "SmartReader", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSavedDeskUiRevamp,
             "SavedDeskUiRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuickAppAccessTestUI,
             "QuickAppAccessTestUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables fingerprint quick unlock.
// Note, that this feature is set from session manager via
// command-line flag.
BASE_FEATURE(kQuickUnlockFingerprint,
             "QuickUnlockFingerprint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
// Controls whether the PIN auto submit backfill operation should be performed.
BASE_FEATURE(kQuickUnlockPinAutosubmitBackfill,
             "QuickUnlockPinAutosubmitBackfill",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Release Notes notifications on non-stable ChromeOS
// channels. Used for testing.
BASE_FEATURE(kReleaseNotesNotificationAllChannels,
             "ReleaseNotesNotificationAllChannels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes the user always eligible to see the release notes notification.
// Normally there are conditions that prevent the notification from appearing.
// For example: channel, profile type, and whether or not the notification had
// already been shown this milestone.
BASE_FEATURE(kReleaseNotesNotificationAlwaysEligible,
             "ReleaseNotesNotificationAlwaysEligible",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables rendering ARC notifications using ChromeOS notification framework
BASE_FEATURE(kRenderArcNotificationsByChrome,
             "RenderArcNotificationsByChrome",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the OS to unpin apps that were pinned by PinnedLauncherApps policy
// but are no longer a part of it from shelf under specific conditions.
BASE_FEATURE(kRemoveStalePolicyPinnedAppsFromShelf,
             "RemoveStalePolicyPinnedAppsFromShelf",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reset audio I/O selection improvement pref, used for testing purpose.
BASE_FEATURE(kResetAudioSelectionImprovementPref,
             "ResetAudioSelectionImprovementPref",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, will reset all shortcut customizations on startup.
BASE_FEATURE(kResetShortcutCustomizations,
             "ResetShortcutCustomizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS scalable IPH.
BASE_FEATURE(kScalableIph, "ScalableIph", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables debug feature of ChromeOS Scalable Iph.
BASE_FEATURE(kScalableIphDebug,
             "ScalableIphDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set all ScalableIph client side config to tracking only config.
BASE_FEATURE(kScalableIphTrackingOnly,
             "ScalableIphTrackingOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use client side config.
BASE_FEATURE(kScalableIphClientConfig,
             "ScalableIphClientConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds a shelf pod button that appears whenever the shelf has limited space and
// acts as an entrypoint to other shelf pod buttons to prevent overflow.
BASE_FEATURE(kScalableShelfPods,
             "ScalableShelfPods",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the scanner dogfood update.
BASE_FEATURE(kScannerDogfood,
             "ScannerDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the scanner update.
BASE_FEATURE(kScannerUpdate,
             "ScannerUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen feature in the personalization app.
BASE_FEATURE(kSeaPen, "SeaPen", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureManagementSeaPen,
             "FeatureManagementSeaPen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen text input feature in the personalization app.
BASE_FEATURE(kSeaPenTextInput,
             "SeaPenTextInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen feature for ChromeOS demo mode.
BASE_FEATURE(kSeaPenDemoMode,
             "SeaPenDemoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sea pen feature with next templates.
BASE_FEATURE(kSeaPenUseExptTemplate,
             "SeaPenUseExptTemplate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen features for enterprise users controlled by the policy.
BASE_FEATURE(kSeaPenEnterprise,
             "SeaPenEnterprise",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automated control of the refresh rate for the internal display.
BASE_FEATURE(kSeamlessRefreshRateSwitching,
             "SeamlessRefreshRateSwitching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable kSecondaryAccountAllowedInArc policy.
BASE_FEATURE(kSecondaryAccountAllowedInArcPolicy,
             "SecondaryAccountAllowedInArcPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
BASE_FEATURE(kSeparateNetworkIcons,
             "SeparateNetworkIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables separate password and PIN fields on the login screen when PIN
// autosubmit is off, instead of a combined "Password or PIN" field.
BASE_FEATURE(kSeparatePasswordAndPinOnLogin,
             "SeparatePasswordAndPinOnLogin",
             base::FEATURE_ENABLED_BY_DEFAULT);

// With this feature enabled, the shortcut app badge is painted in the UI
// instead of being part of the shortcut app icon.
BASE_FEATURE(kSeparateWebAppShortcutBadgeIcon,
             "SeparateWebAppShortcutBadgeIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
BASE_FEATURE(kSessionManagerLongKillTimeout,
             "SessionManagerLongKillTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the session manager daemon will abort the browser if its
// liveness checker detects a hang, i.e. the browser fails to acknowledge and
// respond sufficiently to periodic pings.  IMPORTANT NOTE: the feature name
// here must match exactly the name of the feature in the open-source ChromeOS
// file session_manager_service.cc.
BASE_FEATURE(kSessionManagerLivenessCheck,
             "SessionManagerLivenessCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Removes notifier settings from quick settings view.
BASE_FEATURE(kSettingsAppNotificationSettings,
             "SettingsAppNotificationSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether theme changes should be animated for the Settings app.
BASE_FEATURE(kSettingsAppThemeChangeAnimation,
             "SettingsAppThemeChangeAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should track auto-hide preferences separately between clamshell
// and tablet.
BASE_FEATURE(kShelfAutoHideSeparation,
             "ShelfAutoHideSeparation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables launcher nudge that animates the home button to guide users to open
// the launcher.
BASE_FEATURE(kShelfLauncherNudge,
             "ShelfLauncherNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the OS update page in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMAOsUpdate,
             "ShimlessRMAOsUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables 3p diagnostics in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMA3pDiagnostics,
             "ShimlessRMA3pDiagnostics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables 3p diagnostics dev mode in the Shimless RMA flow. This will skip some
// checks to allow developers to use dev-signed extensions for development
// purpose.
BASE_FEATURE(kShimlessRMA3pDiagnosticsDevMode,
             "ShimlessRMA3pDiagnosticsDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Shimless diagnostics IWAs can access user permission through
// requesting permission at install time.
BASE_FEATURE(kShimlessRMA3pDiagnosticsAllowPermissionPolicy,
             "ShimlessRMA3pDiagnosticsAllowPermissionPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, system shortcuts will utilize state machiens instead of
// keeping track of entire history of keys pressed.
BASE_FEATURE(kShortcutStateMachines,
             "ShortcutStateMachines",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables a toggle to enable Bluetooth debug logs.
BASE_FEATURE(kShowBluetoothDebugLogToggle,
             "ShowBluetoothDebugLogToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows live caption in the video conference tray.
BASE_FEATURE(kShowLiveCaptionInVideoConferenceTray,
             "ShowLiveCaptionInVideoConferenceTray",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows the Play Store icon in Demo Mode.
BASE_FEATURE(kShowPlayInDemoMode,
             "ShowPlayInDemoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether sharing user name should be shown in the continue section for drive
// files shown because they have been recently shared with the user.
BASE_FEATURE(kShowSharingUserInLauncherContinueSection,
             "ShowSharingUserInLauncherContinueSection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling/disabling the Sunfish feature.
BASE_FEATURE(kSunfishFeature,
             "SunfishFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Suppresses the first end of life warning shown 180 days before the AUE date.
BASE_FEATURE(kSuppressFirstEolWarning,
             "SuppressFirstEolWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow the system to suspend to disk via hibernate.
BASE_FEATURE(kSuspendToDisk,
             "CrOSSuspendToDisk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the suspend state machine to better handle suspend accelerators.
BASE_FEATURE(kSuspendStateMachine,
             "SuspendStateMachine",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kHibernateAfterTimeHours{
    &kSuspendToDisk, "HibernateAfterTimeHours", 8};

// Enables custom Demo Mode behavior on feature-aware devices, as controlled by
// the feature management module.
BASE_FEATURE(kFeatureManagementFeatureAwareDeviceDemoMode,
             "FeatureManagementFeatureAwareDeviceDemoMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled the demo mode session to close GMSCore windows which disrupt
// the attract loop videos.
BASE_FEATURE(kDemoModeGMSCoreWindowCloser,
             "DemoModeGMSCoreWindowCloser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disable the demo mode app orientation locked in landscape.
BASE_FEATURE(kDemoModeAppLandscapeLocked,
             "DemoModeAppLandscapeLocked",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Moves toasts to the bottom-side corner where the status area is instead of
// the center when enabled.
BASE_FEATURE(kSideAlignedToasts,
             "SideAlignedToasts",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses experimental component version for smart dim.
BASE_FEATURE(kSmartDimExperimentalComponent,
             "SmartDimExperimentalComponent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Deprecates Sign in with Smart Lock feature. Hides Smart Lock at the sign in
// screen, removes the Smart Lock subpage in settings, and shows a one-time
// notification for users who previously had this feature enabled.
BASE_FEATURE(kSmartLockSignInRemoved,
             "SmartLockSignInRemoved",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables using the system input engine for physical typing in
// Japanese.
BASE_FEATURE(kSystemJapanesePhysicalTyping,
             "SystemJapanesePhysicalTyping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables live captions for sounds produced outside of the browser (e.g. by
// Android or linux apps).
BASE_FEATURE(kSystemLiveCaption,
             "SystemLiveCaption",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Restricts the system live caption feature to the intended target population.
BASE_FEATURE(kFeatureManagementSystemLiveCaption,
             "FeatureManagementSystemLiveCaption",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces uses of `SystemNudge` with the new `AnchoredNudge` component.
BASE_FEATURE(kSystemNudgeMigration,
             "SystemNudgeMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Device End Of Lifetime incentive notifications.
BASE_FEATURE(kSystemShortcutBehavior,
             "SystemShortcutBehavior",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<
    SystemShortcutBehaviorParam>::Option system_shortcut_behavior_options[] = {
    {SystemShortcutBehaviorParam::kIgnoreCommonVdiShortcutList,
     "ignore_common_vdi_shortcuts"},
    {SystemShortcutBehaviorParam::kIgnoreCommonVdiShortcutListFullscreenOnly,
     "ignore_common_vdi_shortcut_fullscreen_only"},
    {SystemShortcutBehaviorParam::kAllowSearchBasedPassthrough,
     "allow_search_based_passthrough"},
    {SystemShortcutBehaviorParam::kAllowSearchBasedPassthroughFullscreenOnly,
     "allow_search_based_passthrough_fullscreen_only"}};
const base::FeatureParam<SystemShortcutBehaviorParam>
    kSystemShortcutBehaviorParam{
        &kSystemShortcutBehavior, "behavior_type",
        SystemShortcutBehaviorParam::kNormalShortcutBehavior,
        &system_shortcut_behavior_options};

// Enables or disables the shadows of system tray bubbles.
BASE_FEATURE(kSystemTrayShadow,
             "SystemTrayShadow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ChromeOS system-proxy daemon, only for system services. This
// means that system services like tlsdate, update engine etc. can opt to be
// authenticated to a remote HTTP web proxy via system-proxy.
BASE_FEATURE(kSystemProxyForSystemServices,
             "SystemProxyForSystemServices",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI to allow Chromebook hotspot functionality for experimental
// carriers, modem and modem FW.
BASE_FEATURE(kTetheringExperimentalFunctionality,
             "TetheringExperimentalFunctionality",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Telemetry Extension.
BASE_FEATURE(kTelemetryExtension,
             "TelemetryExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Terminal System App to load from Downloads for developer testing.
// Only works in dev and canary channels.
BASE_FEATURE(kTerminalDev, "TerminalDev", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental feature for resizing tiling windows.
BASE_FEATURE(kTilingWindowResize,
             "TilingWindowResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable listening to prefs for virtual keyboard policy in login
// screen.
BASE_FEATURE(kTouchVirtualKeyboardPolicyListenPrefsAtLogin,
             "TouchVirtualKeyboardPolicyListenPrefsAtLogin",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the TrafficCountersHandler class to auto-reset traffic counters
// and shows Data Usage in the Celluar Settings UI.
BASE_FEATURE(kTrafficCountersEnabled,
             "TrafficCountersEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables traffic counters for WiFi networks.
BASE_FEATURE(kTrafficCountersForWiFiTesting,
             "TrafficCountersForWiFiTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables trilinear filtering.
BASE_FEATURE(kTrilinearFiltering,
             "TrilinearFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Device Trust connector client code on unmanaged devices
BASE_FEATURE(kUnmanagedDeviceDeviceTrustConnectorEnabled,
             "UnmanagedDeviceDeviceTrustConnectorEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables firmware updates for valid firmwares uploaded to lvfs.
BASE_FEATURE(kUpstreamTrustedReportsFirmware,
             "UpstreamTrustedReportsFirmware",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the Android staging SM-DS server when fetching pending eSIM profiles.
BASE_FEATURE(kUseAndroidStagingSmds,
             "UseAndroidStagingSmds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the AnnotatedAccountId for mapping between User and BrowserContext
// (a.k.a. browser's Profile).
BASE_FEATURE(kUseAnnotatedAccountId,
             "UseAnnotatedAccountId",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This features toggles which implementation is used for authentication UIs on
// ChromeOS settings or PasswordManager. When the feature is enabled,
// `AuthPanel` is used as an authentication UI.
BASE_FEATURE(kUseAuthPanelInSession,
             "UseAuthPanelInSession",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature toggles which dhcpcd version is used for IPv4 provisioning.
// If it is enabled, the legacy dhcpcd7 is used, otherwise the latest dhcpcd is
// used. Note that IPv6 (DHCPv6-PD) always uses the latest dhcpcd.
BASE_FEATURE(kUseLegacyDHCPCD,
             "UseLegacyDHCPCD",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This features controls whether or not passwordless setup is enabled, such as
// having a pin-only config.
BASE_FEATURE(kAllowPasswordlessSetup,
             "AllowPasswordlessSetup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This features controls whether or not pin will be setup as timeout based
// lockout or attempt based lockout.
BASE_FEATURE(kAllowPinTimeoutSetup,
             "AllowPinTimeoutSetup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This features controls whether or not we'll show the legacy WebAuthNDialog,
// that lives in ash/in_session_auth/auth_dialog_contents_view or
// the new dialog that's also shared with Settings and Password Manager,
// that lives in ash/auth/view/active_session_auth_view
BASE_FEATURE(kWebAuthNAuthDialogMerge,
             "WebAuthNAuthDialogMerge",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
BASE_FEATURE(kUseMessagesStagingUrl,
             "UseMessagesStagingUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use ML Service for non-Longform handwriting in CrOS 1P Virtual Keyboard on
// all boards. When this flag is OFF, such usage exists on certain boards only.
BASE_FEATURE(kUseMlServiceForNonLongformHandwritingOnAllBoards,
             "UseMlServiceForNonLongformHandwritingOnAllBoards",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLiveCaptionUserMicrophone,
             "LiveCaptionUserMicrophone",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Remap search+click to right click instead of the legacy alt+click on
// ChromeOS.
BASE_FEATURE(kUseSearchClickForRightClick,
             "UseSearchClickForRightClick",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Stork production SM-DS server when fetching pending eSIM profiles.
BASE_FEATURE(kUseStorkSmdsServerAddress,
             "UseStorkSmdsServerAddress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the staging server as part of the Wallpaper App to verify
// additions/removals of wallpapers.
BASE_FEATURE(kUseWallpaperStagingUrl,
             "UseWallpaperStagingUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables user activity prediction for power management on
// ChromeOS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
BASE_FEATURE(kUserActivityPrediction,
             "UserActivityPrediction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts the video conference feature to the intended
// target population,
BASE_FEATURE(kFeatureManagementVideoConference,
             "FeatureManagementVideoConference",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the vc background replace is enabled.
BASE_FEATURE(kVcBackgroundReplace,
             "VCBackgroundReplace",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the birch model provides lost video conference tab
// suggestions.
BASE_FEATURE(kBirchVideoConferenceSuggestions,
             "BirchVideoConferenceSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to resize thumbnail in VcBackgroundApp.
BASE_FEATURE(kVcResizeThumbnail,
             "VcResizeThumbnail",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the DLC downloading UI for video conferencing tiles is
// enabled.
BASE_FEATURE(kVcDlcUi, "VcDlcUi", base::FEATURE_ENABLED_BY_DEFAULT);

// This is only used as a way to disable portrait relighting.
BASE_FEATURE(kVcPortraitRelight,
             "VcPortraitRelight",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables alternative inference backends for running ChromeOS video
// conferencing portrait relighing models.
BASE_FEATURE(kVcRelightingInferenceBackend,
             "VcRelightingInferenceBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is only used as a way to disable stopAllScreenShare.
BASE_FEATURE(kVcStopAllScreenShare,
             "VcStopAllScreenShare",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable the fake effects for ChromeOS video conferencing controls
// UI. Only meaningful in the emulator.
BASE_FEATURE(kVcControlsUiFakeEffects,
             "VcControlsUiFakeEffects",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative inference backends for running ChromeOS video
// conferencing segmentation models.
BASE_FEATURE(kVcSegmentationInferenceBackend,
             "VcSegmentationInferenceBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative segmentation models for ChromeOS video
// conferencing blur or relighting.
BASE_FEATURE(kVcSegmentationModel,
             "VCSegmentationModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Studio Look and VC settings for ChromeOS video
// conferencing.
BASE_FEATURE(kVcStudioLook, "VcStudioLook", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables mic indicator inside VC tray title header
BASE_FEATURE(kVcTrayMicIndicator,
             "VCTrayMicIndicator",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables sidetone toggle inside VC tray title header
BASE_FEATURE(kVcTrayTitleHeader,
             "VCTrayTitleHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative light intensity for ChromeOS video
// conferencing relighting.
BASE_FEATURE(kVcLightIntensity,
             "VCLightIntensity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables web API support for ChromeOS video conferencing.
BASE_FEATURE(kVcWebApi, "VcWebApi", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables versioned wallpaper info.
BASE_FEATURE(kVersionedWallpaperInfo,
             "VersionedWallpaperInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable global preferences for skin tone and gender in the virtual
// keyboard emoji picker.
BASE_FEATURE(kVirtualKeyboardGlobalEmojiPreferences,
             "VirtualKeyboardGlobalEmojiPreferences",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to allow enabling wake on WiFi features in shill.
BASE_FEATURE(kWakeOnWifiAllowed,
             "WakeOnWifiAllowed",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable "daily" refresh wallpaper to refresh every ten seconds for testing.
BASE_FEATURE(kWallpaperFastRefresh,
             "WallpaperFastRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable using google photos shared albums for wallpaper.
BASE_FEATURE(kWallpaperGooglePhotosSharedAlbums,
             "WallpaperGooglePhotosSharedAlbums",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new Welcome Experience for first-time peripheral connections.
BASE_FEATURE(kWelcomeExperience,
             "WelcomeExperience",
             base::FEATURE_ENABLED_BY_DEFAULT);

// kWelcomeExperienceTestUnsupportedDevices enables the new device Welcome
// Experience to be tested on external devices that are not officially
// supported. When enabled, users will be able to initiate and complete
// the enhanced Welcome Experience flow using these unsupported external
// devices. This flag is intended for testing purposes and should be disabled
// disabled in production environments.
BASE_FEATURE(kWelcomeExperienceTestUnsupportedDevices,
             "WelcomeExperienceTestUnsupportedDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Welcome Tour that walks new users through ChromeOS System UI.
BASE_FEATURE(kWelcomeTour, "WelcomeTour", base::FEATURE_ENABLED_BY_DEFAULT);

// Whether ChromeVox is supported in the Welcome Tour that walks new users
// through ChromeOS System UI.
BASE_FEATURE(kWelcomeTourChromeVoxSupported,
             "WelcomeTourChromeVoxSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the Welcome Tour is enabled counterfactually as part of an experiment
// arm. When this is enabled, the Welcome Tour V1 will be shown.
BASE_FEATURE(kWelcomeTourCounterfactualArm,
             "WelcomeTourCounterfactualArm",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces user eligibility for the Welcome Tour that walks new users through
// ChromeOS System UI. Enabling this flag has no effect unless `kWelcomeTour` is
// also enabled.
BASE_FEATURE(kWelcomeTourForceUserEligibility,
             "WelcomeTourForceUserEligibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Welcome Tour holdback is enabled as part of an experiment arm.
// When this is enabled, neither version of Welcome Tour version will be shown.
BASE_FEATURE(kWelcomeTourHoldbackArm,
             "WelcomeTourHoldbackArm",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Welcome Tour V2 that has different strings and steps than V1.
// Enabling this flag has no effect unless `kWelcomeTour` is also enabled.
BASE_FEATURE(kWelcomeTourV2,
             "WelcomeTourV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable MAC Address Randomization on WiFi connection.
BASE_FEATURE(kWifiConnectMacAddressRandomization,
             "WifiConnectMacAddressRandomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control whether the Wi-Fi concurrency Shill API is used when enable station
// Wi-Fi or tethering in Chrome Ash.
BASE_FEATURE(kWifiConcurrency,
             "WifiConcurrency",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control whether the WiFi Direct is enabled. When enabled, it will allow
// the nearby share feature to utilize WiFi P2P for sharing data.
BASE_FEATURE(kWifiDirect, "WiFiDirect", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable the syncing of deletes of Wi-Fi configurations.
// This only controls sending delete events to the Chrome Sync server.
BASE_FEATURE(kWifiSyncAllowDeletes,
             "WifiSyncAllowDeletes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable syncing of Wi-Fi configurations between
// ChromeOS and a connected Android phone.
BASE_FEATURE(kWifiSyncAndroid,
             "WifiSyncAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to apply incoming Wi-Fi configuration delete events from
// the Chrome Sync server.
BASE_FEATURE(kWifiSyncApplyDeletes,
             "WifiSyncApplyDeletes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that splits windows by dragging one window
// over another window.
BASE_FEATURE(kWindowSplitting,
             "WindowSplitting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that lets users easily layout, resize and
// position their windows using only mouse and touch gestures.
BASE_FEATURE(kWmMode, "WmMode", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that overrides the specific holdback
// experiments on the M-129.
BASE_FEATURE(kIgnoreM129Holdback,
             "IgnoreM129Holdback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for 28 day device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClient28DayActiveCheckMembership,
             "DeviceActiveClient28DayActiveCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn cohort device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnCohortCheckMembership,
             "DeviceActiveClientChurnCohortCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn observation
// device active pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnObservationCheckMembership,
             "DeviceActiveClientChurnObservationCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables attaching first active week and last powerwash week to
// the churn observation check in ping.
BASE_FEATURE(kDeviceActiveClientChurnObservationNewDeviceMetadata,
             "DeviceActiveClientChurnObservationNewDeviceMetadata",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables forced reboots when DeviceScheduledReboot policy is set.
BASE_FEATURE(kDeviceForceScheduledReboot,
             "DeviceForceScheduledReboot",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum delay added to reboot time when DeviceScheduledReboot policy is set.
const base::FeatureParam<base::TimeDelta> kDeviceForceScheduledRebootMaxDelay{
    &kDeviceForceScheduledReboot, "MaxDelay", base::Minutes(2)};

// Enables settings to be split per device.
BASE_FEATURE(kInputDeviceSettingsSplit,
             "InputDeviceSettingsSplit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables bug fix for dead keys where there's no input field.
// When enabled, keys are forwarded without dead key processing when there's no
// input field.
BASE_FEATURE(kInputMethodDeadKeyFixForNoInputField,
             "InputMethodDeadKeyFixForNoInputField",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables bug fix for dead keys.
// When enabled, dead keys correctly emit the 'Dead' event on key down.
BASE_FEATURE(kInputMethodDeadKeyFix,
             "InputMethodDeadKeyFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables bug fix for right alt keys for Korean input method.
// When enabled, Right Alt will immediately toggle between Korean and English on
// key down.
BASE_FEATURE(kInputMethodKoreanRightAltKeyDownFix,
             "InputMethodKoreanRightAltKeyDownFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables peripheral customization to be split per device.
BASE_FEATURE(kPeripheralCustomization,
             "PeripheralCustomization",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables peripherals logging.
BASE_FEATURE(kEnablePeripheralsLogging,
             "PeripheralsLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable peripheral notification to notify users when a input device is
// connected to the user's chromebook for the first time.
BASE_FEATURE(kPeripheralNotification,
             "PeripheralNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable fast ink for software cursor. Fast ink provides a low-latency
// cursor with possible tearing artifacts.
BASE_FEATURE(kEnableFastInkForSoftwareCursor,
             "EnableFastInkForSoftwareCursor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the feature deferring vm_concierge startup until all the critical
// login tasks are finished.
BASE_FEATURE(kDeferConciergeStartup,
             "DeferConciergeStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool AreHelpAppWelcomeTipsEnabled() {
  return base::FeatureList::IsEnabled(kHelpAppWelcomeTips) &&
         base::FeatureList::IsEnabled(kScalableIph);
}

bool ArePromiseIconsEnabled() {
  return base::FeatureList::IsEnabled(kPromiseIcons);
}

bool ArePromiseIconsForWebAppsEnabled() {
  return base::FeatureList::IsEnabled(kPromiseIconsForWebApps) &&
         ArePromiseIconsEnabled();
}

bool AreSideAlignedToastsEnabled() {
  // Side aligned toasts are launching together with Notifier Collision.
  // TODO(b/342455518): Remove `kSideAlignedToasts` and its usage and just use
  // kNotifierCollision to avoid confusions.
  return IsNotifierCollisionEnabled() ||
         base::FeatureList::IsEnabled(kSideAlignedToasts);
}

bool ForceOnDeviceAppControlsForAllRegions() {
  return base::FeatureList::IsEnabled(kForceOnDeviceAppControlsForAllRegions);
}

bool IsAudioHFPMicSRToggleEnabled() {
  return base::FeatureList::IsEnabled(kAudioHFPMicSRToggle);
}

bool IsAudioSelectionImprovementEnabled() {
  return base::FeatureList::IsEnabled(kAudioSelectionImprovement);
}

bool Is16DesksEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagement16Desks);
}

bool IsAdaptiveChargingEnabled() {
  return base::FeatureList::IsEnabled(kAdaptiveCharging);
}

bool IsAdaptiveChargingForTestingEnabled() {
  return base::FeatureList::IsEnabled(kAdaptiveChargingForTesting);
}

bool IsOnDeviceAppControlsEnabled() {
  return base::FeatureList::IsEnabled(kOnDeviceAppControls);
}

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAllowScrollSettingsEnabled() {
  return IsInputDeviceSettingsSplitEnabled() &&
         base::FeatureList::IsEnabled(kAllowScrollSettings);
}

bool IsAltClickAndSixPackCustomizationEnabled() {
  return IsInputDeviceSettingsSplitEnabled() &&
         base::FeatureList::IsEnabled(kAltClickAndSixPackCustomization);
}

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
}

bool IsAmbientModeManagedScreensaverEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeManagedScreensaver);
}

bool IsAmbientModePhotoPreviewEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModePhotoPreviewFeature);
}

bool IsAnnotatorModeEnabled() {
  return base::FeatureList::IsEnabled(kAnnotatorMode);
}

bool IsAllowApnModificationPolicyEnabled() {
  return base::FeatureList::IsEnabled(kAllowApnModificationPolicy);
}

bool IsApnRevampAndAllowApnModificationPolicyEnabled() {
  return IsApnRevampEnabled() && IsAllowApnModificationPolicyEnabled();
}

bool IsApnRevampEnabled() {
  return base::FeatureList::IsEnabled(kApnRevamp);
}

bool IsApnRevampAndPoliciesEnabled() {
  return IsApnRevampEnabled() && chromeos::features::IsApnPoliciesEnabled();
}

bool IsAppNotificationsPageEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppNotificationsPage);
}

bool IsArcFuseBoxFileSharingEnabled() {
  return base::FeatureList::IsEnabled(kArcFuseBoxFileSharing);
}

bool IsAssistantNativeIconsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantNativeIcons);
}

bool IsAutoNightLightEnabled() {
  return base::FeatureList::IsEnabled(kAutoNightLight);
}

bool IsBackgroundBlurEnabled() {
  bool enabled_by_feature_flag =
      base::FeatureList::IsEnabled(kEnableBackgroundBlur);
#if defined(ARCH_CPU_ARM_FAMILY)
  // Enable background blur on Mali when GPU rasterization is enabled.
  // See crbug.com/996858 for the condition.
  return enabled_by_feature_flag &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableTabletMode);
#else
  return enabled_by_feature_flag;
#endif
}

bool IsBatterySaverAvailable() {
  return base::FeatureList::IsEnabled(kBatterySaver);
}

bool IsBatterySaverAlwaysOn() {
  return base::FeatureList::IsEnabled(kBatterySaverAlwaysOn);
}

bool IsBirchCoralEnabled() {
  return base::FeatureList::IsEnabled(kBirchCoral);
}

bool IsBirchWeatherEnabled() {
  return base::FeatureList::IsEnabled(kBirchWeather);
}

bool IsBluetoothDisconnectWarningEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothDisconnectWarning);
}

bool IsBluetoothQualityReportEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothQualityReport);
}

bool IsBocaEnabled() {
  return base::FeatureList::IsEnabled(kBoca);
}

bool IsBocaConsumerEnabled() {
  return base::FeatureList::IsEnabled(kBocaConsumer);
}

bool IsBocaExtensionConsumerEnabled() {
  return base::FeatureList::IsEnabled(kBocaExtensionConsumer);
}

bool IsBrightnessControlInSettingsEnabled() {
  return base::FeatureList::IsEnabled(kEnableBrightnessControlInSettings);
}

bool IsBubbleCornerRadiusUpdateEnabled() {
  return base::FeatureList::IsEnabled(kEnableBubbleCornerRadiusUpdate);
}

bool IsCaptureModeEducationEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeEducation);
}

bool IsCaptureModeEducationBypassLimitsEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeEducationBypassLimits);
}

bool IsChangePasswordFactorSetupEnabled() {
  return base::FeatureList::IsEnabled(kChangePasswordFactorSetup);
}

bool IsCheckPasswordsAgainstCryptohomeHelperEnabled() {
  return base::FeatureList::IsEnabled(kCheckPasswordsAgainstCryptohomeHelper);
}

bool IsClipboardHistoryLongpressEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryLongpress);
}

bool IsClipboardHistoryUrlTitlesEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryUrlTitles) &&
         chromeos::features::IsClipboardHistoryRefreshEnabled();
}

bool IsContinuousOverviewScrollAnimationEnabled() {
  return base::FeatureList::IsEnabled(kContinuousOverviewScrollAnimation) &&
         chromeos::features::IsJellyEnabled();
}

bool IsCoralFeatureEnabled() {
  return base::FeatureList::IsEnabled(kCoralFeature);
}

bool IsCryptauthAttestationSyncingEnabled() {
  return base::FeatureList::IsEnabled(kCryptauthAttestationSyncing);
}

bool IsConsumerAutoUpdateToggleAllowed() {
  return base::FeatureList::IsEnabled(kConsumerAutoUpdateToggleAllowed);
}

bool IsCopyClientKeysCertsToChapsEnabled() {
  return !IsNssDbClientCertsRollbackEnabled() &&
         base::FeatureList::IsEnabled(kCopyClientKeysCertsToChaps);
}

bool IsCrosPrivacyHubAppPermissionsEnabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubAppPermissions);
}

bool IsCrosPrivacyHubAppPermissionsV2Enabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubAppPermissionsV2);
}

bool IsCrosPrivacyHubLocationEnabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHub);
}

bool IsCrossDeviceFeatureSuiteAllowed() {
  if (switches::IsRevenBranding()) {
    return false;
  }

  return base::FeatureList::IsEnabled(kAllowCrossDeviceFeatureSuite);
}

bool IsCrosSwitcherEnabled() {
  return base::FeatureList::IsEnabled(kCrosSwitcher);
}

bool IsDeskBarWindowOcclusionOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kDeskBarWindowOcclusionOptimization);
}

bool IsDeskButtonEnabled() {
  return base::FeatureList::IsEnabled(kDeskButton);
}

bool IsDeskTemplateSyncEnabled() {
  return base::FeatureList::IsEnabled(kDeskTemplateSync);
}

bool IsDisplayPerformanceModeEnabled() {
  return base::FeatureList::IsEnabled(kDisplayPerformanceMode);
}

bool IsInputDeviceSettingsSplitEnabled() {
  return base::FeatureList::IsEnabled(kInputDeviceSettingsSplit);
}

bool IsPeripheralCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kPeripheralCustomization) &&
         IsInputDeviceSettingsSplitEnabled();
}

bool IsPeripheralsLoggingEnabled() {
  return base::FeatureList::IsEnabled(kEnablePeripheralsLogging);
}

bool IsDisplayAlignmentAssistanceEnabled() {
  return base::FeatureList::IsEnabled(kDisplayAlignAssist);
}

bool IsDriveFsMirroringEnabled() {
  return base::FeatureList::IsEnabled(kDriveFsMirroring);
}

int GetDriveFsBulkPinningQueueSize() {
  return base::GetFieldTrialParamByFeatureAsInt(kDriveFsBulkPinningExperiment,
                                                "queue_size", 5);
}

bool IsEapGtcWifiAuthenticationEnabled() {
  return base::FeatureList::IsEnabled(kEapGtcWifiAuthentication);
}

bool IsAudioSourceFetcherResamplingEnabled() {
  // TODO(b/245617354): Once ready, enable this feature under
  // kProjectorBleedingEdgeExperience flag as well.
  return base::FeatureList::IsEnabled(kAudioSourceFetcherResampling);
}

bool IsDemoModeGMSCoreWindowCloserEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeGMSCoreWindowCloser);
}

bool IsDemoModeAppLandscapeLockedEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeAppLandscapeLocked);
}

bool IsEcheSWAEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsEcheSWADebugModeEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWADebugMode);
}

bool IsEcheSWAMeasureLatencyEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWAMeasureLatency);
}

bool IsEcheMetricsRevampEnabled() {
  return base::FeatureList::IsEnabled(kEcheMetricsRevamp);
}

bool IsEOLIncentiveEnabled() {
  return base::FeatureList::IsEnabled(kEolIncentive);
}

bool IsExperimentalRgbKeyboardPatternsEnabled() {
  return base::FeatureList::IsEnabled(kExperimentalRgbKeyboardPatterns);
}

bool IsExtendedUpdatesOptInFeatureEnabled() {
  return base::FeatureList::IsEnabled(kExtendedUpdatesOptInFeature);
}

bool IsExternalKeyboardInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableExternalKeyboardsInDiagnostics);
}

bool IsFamilyLinkOnSchoolDeviceEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOnSchoolDevice);
}

bool IsFastInkForSoftwareCursorEnabled() {
  return base::FeatureList::IsEnabled(kEnableFastInkForSoftwareCursor);
}

bool IsFastPairEnabled() {
  return base::FeatureList::IsEnabled(kFastPair);
}

bool IsFastPairBleRotationEnabled() {
  return base::FeatureList::IsEnabled(kFastPairBleRotation);
}

bool IsFastPairDebugMetadataEnabled() {
  return base::FeatureList::IsEnabled(kFastPairDebugMetadata);
}

bool IsFastPairDevicesBluetoothSettingsEnabled() {
  return base::FeatureList::IsEnabled(kFastPairDevicesBluetoothSettings);
}

bool IsFastPairHandshakeLongTermRefactorEnabled() {
  return base::FeatureList::IsEnabled(kFastPairHandshakeLongTermRefactor);
}

bool IsFastPairHIDEnabled() {
  return base::FeatureList::IsEnabled(kFastPairHID);
}

bool IsFastPairKeyboardsEnabled() {
  return base::FeatureList::IsEnabled(kFastPairKeyboards);
}

bool IsFastPairSavedDevicesNicknamesEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevicesNicknames);
}

bool IsFastPairPwaCompanionEnabled() {
  return base::FeatureList::IsEnabled(kFastPairPwaCompanion);
}

bool IsFastPairPreventNotificationsForRecentlyLostDeviceEnabled() {
  return base::FeatureList::IsEnabled(
      kFastPairPreventNotificationsForRecentlyLostDevice);
}

bool IsFastPairSavedDevicesEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevices);
}

bool IsFastPairSavedDevicesStrictOptInEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevicesStrictOptIn);
}

bool IsFederatedServiceEnabled() {
  return base::FeatureList::IsEnabled(kFederatedService);
}

bool IsFederatedServiceScheduleTasksEnabled() {
  return IsFederatedServiceEnabled() &&
         base::FeatureList::IsEnabled(kFederatedServiceScheduleTasks);
}

bool IsFederatedStringsServiceEnabled() {
  return base::FeatureList::IsEnabled(kFederatedService) &&
         base::FeatureList::IsEnabled(kFederatedStringsService);
}

bool IsFederatedStringsServiceScheduleTasksEnabled() {
  return IsFederatedStringsServiceEnabled() &&
         base::FeatureList::IsEnabled(kFederatedStringsServiceScheduleTasks);
}

bool IsFileManagerFuseBoxDebugEnabled() {
  return base::FeatureList::IsEnabled(kFuseBoxDebug);
}

bool IsFilesConflictDialogEnabled() {
  return base::FeatureList::IsEnabled(kFilesConflictDialog);
}

bool IsFilesLocalImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kFilesLocalImageSearch);
}

bool IsFingerprintAuthFactorEnabled() {
  return base::FeatureList::IsEnabled(kFingerprintAuthFactor);
}

bool IsFirmwareUpdateUIV2Enabled() {
  return base::FeatureList::IsEnabled(kFirmwareUpdateUIV2);
}

bool IsFlexAutoEnrollmentEnabled() {
  return switches::IsRevenBranding() &&
         base::FeatureList::IsEnabled(kFlexAutoEnrollment);
}

bool IsFlexFirmwareUpdateEnabled() {
  return switches::IsRevenBranding() &&
         base::FeatureList::IsEnabled(kFlexFirmwareUpdate);
}

bool IsFloatingSsoAllowed() {
  return base::FeatureList::IsEnabled(kFloatingSso);
}

bool IsFloatingWorkspaceEnabled() {
  return base::FeatureList::IsEnabled(kFloatingWorkspace);
}

bool IsFloatingWorkspaceV2Enabled() {
  return base::FeatureList::IsEnabled(kFloatingWorkspaceV2);
}

bool IsFocusModeEnabled() {
  // If the holdback feature flag is enabled, the feature should be disabled,
  // but only if the device is eligible for the study. Exclusion happens
  // via hardware overlay, so it needs to be checked separately from the finch
  // controlled holdback feature flag.
  const bool device_excluded_from_holdback_study = base::FeatureList::IsEnabled(
      kFeatureManagementShouldExcludeFromSysUiHoldback);
  if (IsSysUiShouldHoldbackFocusModeEnabled() &&
      !device_excluded_from_holdback_study) {
    return false;
  }

  return base::FeatureList::IsEnabled(kFocusMode);
}

bool IsFocusModeYTMEnabled() {
  return base::FeatureList::IsEnabled(kFocusModeYTM);
}

bool ShouldForceEnableServerSideSpeechRecognitionForDev() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(
      kForceEnableServerSideSpeechRecognitionForDev);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING);
}

bool IsForceReSyncDriveEnabled() {
  return base::FeatureList::IsEnabled(kForceReSyncDrive);
}

bool IsForestFeatureEnabled() {
  // If the holdback feature flag is enabled, the feature should be disabled,
  // but only if the device is eligible for the study. Exclusion happens
  // via hardware overlay, so it needs to be checked separately from the finch
  // controlled holdback feature flag.
  const bool device_excluded_from_holdback_study = base::FeatureList::IsEnabled(
      kFeatureManagementShouldExcludeFromSysUiHoldback);
  if (IsSysUiShouldHoldbackForestEnabled() &&
      !device_excluded_from_holdback_study) {
    return false;
  }

  return base::FeatureList::IsEnabled(kForestFeature);
}

bool IsFullscreenAfterUnlockAllowed() {
  return base::FeatureList::IsEnabled(kFullscreenAfterUnlockAllowed);
}

bool IsFullscreenAlertBubbleEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenAlertBubble);
}

bool IsBlockFwupdClientEnabled() {
  return base::FeatureList::IsEnabled(kBlockFwupdClient);
}

bool IsGaiaRecordAccountCreationEnabled() {
  return base::FeatureList::IsEnabled(kGaiaRecordAccountCreation);
}

bool IsGifRecordingEnabled() {
  const bool device_enrolled_in_holdback =
      !base::FeatureList::IsEnabled(
          kFeatureManagementShouldExcludeFromSysUiHoldback) &&
      base::FeatureList::IsEnabled(kSysUiShouldHoldbackGifRecording);
  return !device_enrolled_in_holdback &&
         base::FeatureList::IsEnabled(kGifRecording);
}

bool IsGraduationEnabled() {
  return base::FeatureList::IsEnabled(kGraduation);
}

bool IsFeatureManagementGrowthFrameworkEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementGrowthFramework);
}

bool IsGrowthFrameworkEnabled() {
  return base::FeatureList::IsEnabled(kGrowthFramework);
}

bool IsGrowthCampaignsNudgeParentToAppWindow() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsNudgeParentToAppWindow);
}

bool IsGrowthCampaignsCrOSEventsEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsCrOSEvents);
}

bool IsGrowthCampaignsExperimentTagTargetingEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsExperimentTagTargeting);
}

bool IsGrowthCampaignsInConsumerSessionEnabled() {
  return IsGrowthFrameworkEnabled() &&
         base::FeatureList::IsEnabled(kGrowthCampaignsInConsumerSession);
}

bool IsGrowthCampaignsInDemoModeEnabled() {
  return IsGrowthFrameworkEnabled() &&
         base::FeatureList::IsEnabled(kGrowthCampaignsInDemoMode);
}

bool IsGrowthCampaignsShowNudgeInsideWindowBoundsEnabled() {
  return base::FeatureList::IsEnabled(
      kGrowthCampaignsShowNudgeInsideWindowBounds);
}

bool IsGrowthCampaignsTriggerAtLoadComplete() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsTriggerAtLoadComplete);
}

bool IsGrowthCampaignsTriggerByAppOpenEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsTriggerByAppOpen);
}

bool IsGrowthCampaignsTriggerByBrowserEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsTriggerByBrowser);
}

bool IsGrowthCampaignsTriggerByEventEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsTriggerByEvent);
}

bool IsGrowthCampaignsTriggerByRecordEventEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsTriggerByRecordEvent);
}

bool IsGrowthCampaignsObserveTriggeringWidgetChangeEnabled() {
  return base::FeatureList::IsEnabled(
      kGrowthCampaignsObserveTriggeringWidgetChange);
}

bool IsGrowthInternalsEnabled() {
  return base::FeatureList::IsEnabled(kGrowthInternals);
}

bool IsGlanceablesTimeManagementClassroomStudentViewEnabled() {
  return base::FeatureList::IsEnabled(
      kGlanceablesTimeManagementClassroomStudentView);
}

bool IsGlanceablesTimeManagementTasksViewEnabled() {
  const bool device_enrolled_in_holdback =
      !base::FeatureList::IsEnabled(
          kFeatureManagementShouldExcludeFromSysUiHoldback) &&
      base::FeatureList::IsEnabled(kSysUiShouldHoldbackTaskManagement);
  if (device_enrolled_in_holdback) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGlanceablesTimeManagementTasksView);
}

bool IsGlanceablesTimeManagementTasksViewAssignedTasksEnabled() {
  return base::FeatureList::IsEnabled(
      kGlanceablesTimeManagementTasksViewAssignedTasks);
}

bool AreAnyGlanceablesTimeManagementViewsEnabled() {
  return IsGlanceablesTimeManagementClassroomStudentViewEnabled() ||
         IsGlanceablesTimeManagementTasksViewEnabled();
}

bool AreHealthdInternalsTabsEnabled() {
  return base::FeatureList::IsEnabled(kHealthdInternalsTabs);
}

bool IsHibernateEnabled() {
  return base::FeatureList::IsEnabled(kHibernate);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode);
}

bool IsHoldingSpaceSuggestionsEnabled() {
  // If the holdback feature flag is enabled, the feature should be disabled,
  // but only if the device is eligible for the study. Exclusion happens
  // via hardware overlay, so it needs to be checked separately from the finch
  // controlled holdback feature flag.
  const bool device_excluded_from_holdback_study = base::FeatureList::IsEnabled(
      kFeatureManagementShouldExcludeFromSysUiHoldback);
  if (IsSysUiShouldHoldbackDriveIntegrationEnabled() &&
      !device_excluded_from_holdback_study) {
    return false;
  }

  return base::FeatureList::IsEnabled(kHoldingSpaceSuggestions);
}

bool IsHomeButtonQuickAppAccessEnabled() {
  return base::FeatureList::IsEnabled(kHomeButtonQuickAppAccess) ||
         base::FeatureList::IsEnabled(kQuickAppAccessTestUI);
}

bool IsHomeButtonWithTextEnabled() {
  return base::FeatureList::IsEnabled(kHomeButtonWithText);
}

bool IsHostnameSettingEnabled() {
  return base::FeatureList::IsEnabled(kEnableHostnameSetting);
}

bool IsInstantHotspotRebrandEnabled() {
  return base::FeatureList::IsEnabled(kInstantHotspotRebrand);
}

bool IsSnoopingProtectionEnabled() {
  return base::FeatureList::IsEnabled(kSnoopingProtection) &&
         switches::HasHps();
}

bool IsStartAssistantAudioDecoderOnDemandEnabled() {
  return base::FeatureList::IsEnabled(kStartAssistantAudioDecoderOnDemand);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsInternalServerSideSpeechRecognitionEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(b/245614967): Once ready, enable this feature under
  // kProjectorBleedingEdgeExperience flag as well.
  return IsInternalServerSideSpeechRecognitionControlEnabled() &&
         (ShouldForceEnableServerSideSpeechRecognitionForDev() ||
          base::FeatureList::IsEnabled(kInternalServerSideSpeechRecognition));
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsInternalServerSideSpeechRecognitionControlEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(
      kInternalServerSideSpeechRecognitionControl);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsInternalServerSideSpeechRecognitionEnabledByFinch() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(
             kInternalServerSideSpeechRecognitionByFinch) ||
         base::FeatureList::IsEnabled(
             kInternalServerSideSpeechRecognitionUSMModelFinch);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsIppClientInfoEnabled() {
  return base::FeatureList::IsEnabled(kIppClientInfo);
}

bool IsIsolatedWebAppKioskEnabled() {
  return base::FeatureList::IsEnabled(kIsolatedWebAppKiosk);
}

bool IsJellyEnabledForShortcutCustomization() {
  return chromeos::features::IsJellyEnabled();
}

bool IsKerberosRememberPasswordByDefaultEnabled() {
  return base::FeatureList::IsEnabled(kKerberosRememberPasswordByDefault);
}

bool IsKeyboardBacklightControlInSettingsEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableKeyboardBacklightControlInSettings);
}

bool IsKeyboardBacklightToggleEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardBacklightToggle);
}

bool IsKeyboardRewriterFixEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardRewriterFix);
}

bool IsLanguagePacksInOobeEnabled() {
  return base::FeatureList::IsEnabled(kLanguagePacksInOobe);
}

bool IsLacrosExtensionPrintingEnabled() {
  return base::FeatureList::IsEnabled(kLacrosExtensionPrinting);
}

bool IsLauncherContinueSectionWithRecentsEnabled() {
  // If the holdback feature flag is enabled, the feature should be disabled,
  // but only if the device is eligible for the study. Exclusion happens
  // via hardware overlay, so it needs to be checked separately from the finch
  // controlled holdback feature flag.
  const bool device_excluded_from_holdback_study = base::FeatureList::IsEnabled(
      kFeatureManagementShouldExcludeFromSysUiHoldback);
  if (IsSysUiShouldHoldbackDriveIntegrationEnabled() &&
      !device_excluded_from_holdback_study) {
    return false;
  }

  return base::FeatureList::IsEnabled(kLauncherContinueSectionWithRecents) ||
         base::FeatureList::IsEnabled(
             kLauncherContinueSectionWithRecentsRollout);
}

bool IsLauncherNudgeShortIntervalEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeShortInterval);
}

bool IsLauncherNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeSessionReset);
}

bool IsLauncherSearchControlEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSearchControl) &&
         (base::FeatureList::IsEnabled(kFeatureManagementLocalImageSearch) ||
          base::FeatureList::IsEnabled(kLocalImageSearchOnCore));
}

bool IsLinkCrossDeviceDogfoodFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceDogfoodFeedback);
}

bool IsLinkCrossDeviceInternalsEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceInternals);
}

bool IsLobsterEnabled() {
  return base::FeatureList::IsEnabled(kLobster) &&
         base::FeatureList::IsEnabled(kFeatureManagementLobster);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsGameDashboardEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboard);
}

bool IsGameDashboardGamepadSupportEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardGamepadSupport);
}

bool IsGameDashboardGamePWAsEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardGamePWAs);
}

bool IsGameDashboardGamesInTestEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardGamesInTest);
}

bool AreGameDashboardUtilitiesEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardUtilities);
}

bool IsLockScreenInlineReplyEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenInlineReply);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsProductivityLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kProductivityLauncherImageSearch) &&
         (base::FeatureList::IsEnabled(kFeatureManagementLocalImageSearch) ||
          base::FeatureList::IsEnabled(kLocalImageSearchOnCore));
}

bool IsMacAddressRandomizationEnabled() {
  return base::FeatureList::IsEnabled(kMacAddressRandomization);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsMultiCalendarSupportEnabled() {
  return base::FeatureList::IsEnabled(kMultiCalendarSupport);
}

bool IsMultiZoneRgbKeyboardEnabled() {
  return base::FeatureList::IsEnabled(kMultiZoneRgbKeyboard);
}

bool IsEcheLauncherEnabled() {
  return base::FeatureList::IsEnabled(kEcheLauncher) &&
         base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsEcheLauncherIconsInMoreAppsButtonEnabled() {
  return base::FeatureList::IsEnabled(kEcheLauncherIconsInMoreAppsButton);
}

bool IsEcheLauncherListViewEnabled() {
  return IsEcheLauncherEnabled() &&
         base::FeatureList::IsEnabled(kEcheLauncherListView);
}

bool IsEcheNetworkConnectionStateEnabled() {
  return base::FeatureList::IsEnabled(kEcheNetworkConnectionState) &&
         base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsEcheShorterScanningDutyCycleEnabled() {
  return base::FeatureList::IsEnabled(kEcheShorterScanningDutyCycle);
}

bool AreEphemeralNetworkPoliciesEnabled() {
  return base::FeatureList::IsEnabled(kEphemeralNetworkPolicies);
}

bool CanEphemeralNetworkPoliciesBeEnabledByPolicy() {
  return base::FeatureList::IsEnabled(kEphemeralNetworkPoliciesEnabledPolicy);
}

bool IsNearbyKeepAliveFixEnabled() {
  return base::FeatureList::IsEnabled(kNearbyKeepAliveFix);
}

bool IsNearbyPresenceEnabled() {
  return base::FeatureList::IsEnabled(kNearbyPresence);
}

bool IsNotificationLimitEnabled() {
  return base::FeatureList::IsEnabled(kNotificationLimit);
}

bool IsNotifierCollisionEnabled() {
  return base::FeatureList::IsEnabled(kNotifierCollision);
}

bool IsOAuthIppEnabled() {
  return base::FeatureList::IsEnabled(kEnableOAuthIpp);
}

bool IsNewLockScreenReauthLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewLockScreenReauthLayout);
}

bool IsNotificationCenterControllerEnabled() {
  return base::FeatureList::IsEnabled(kNotificationCenterController) ||
         // Ongoing processes must launch together with the new
         // `NotificationCenterController`.
         base::FeatureList::IsEnabled(kOngoingProcesses);
}

bool IsNotificationExpansionAnimationEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExpansionAnimation);
}

bool IsNotificationImageDragEnabled() {
  return base::FeatureList::IsEnabled(kNotificationImageDrag);
}

bool IsNotificationScrollBarEnabled() {
  return base::FeatureList::IsEnabled(kNotificationScrollBar);
}

bool IsNotificationsInContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsInContextMenu);
}

bool IsNssDbClientCertsRollbackEnabled() {
  return base::FeatureList::IsEnabled(kEnableNssDbClientCertsRollback);
}

bool AreOngoingProcessesEnabled() {
  return base::FeatureList::IsEnabled(kOngoingProcesses);
}

bool IsOobeChromeVoxHintEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeChromeVoxHint);
}

bool IsOobeGaiaInfoScreenEnabled() {
  return base::FeatureList::IsEnabled(kOobeGaiaInfoScreen);
}

bool IsKioskLoginScreenEnabled() {
  return base::FeatureList::IsEnabled(kEnableKioskLoginScreen);
}

bool IsOobeJellyEnabled() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kOobeJelly);
}

bool IsModifierSplitEnabled() {
  return IsInputDeviceSettingsSplitEnabled() &&
         base::FeatureList::IsEnabled(kModifierSplit);
}

bool IsModifierSplitDogfoodEnabled() {
  return base::FeatureList::IsEnabled(kModifierSplitDogfood);
}

bool IsMouseImposterCheckEnabled() {
  return base::FeatureList::IsEnabled(kMouseImposterCheck) &&
         IsInputDeviceSettingsSplitEnabled();
}

bool IsSplitKeyboardRefactorEnabled() {
  return base::FeatureList::IsEnabled(kSplitKeyboardRefactor) &&
         IsModifierSplitEnabled();
}

bool IsOobeAiIntroEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementOobeAiIntro);
}

bool IsOobeGeminiIntroEnabled() {
  return base::FeatureList::IsEnabled(kOobeGeminiIntroForTesting) ||
         base::FeatureList::IsEnabled(kFeatureManagementOobeGeminiIntro);
}

bool IsOobeJellyModalEnabled() {
  return IsOobeJellyEnabled() && base::FeatureList::IsEnabled(kOobeJellyModal);
}

bool IsBootAnimationEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementOobeSimon);
}

bool IsOobeAddUserDuringEnrollmentEnabled() {
  return base::FeatureList::IsEnabled(kOobeAddUserDuringEnrollment);
}

bool IsOobeSkipAssistantEnabled() {
  return base::FeatureList::IsEnabled(kOobeSkipAssistant);
}

bool IsOobeChoobeEnabled() {
  return base::FeatureList::IsEnabled(kOobeChoobe);
}

bool IsOobeCrosEventsEnabled() {
  return base::FeatureList::IsEnabled(kOobeCrosEvents);
}

bool IsOobePersonalizedOnboardingEnabled() {
  return base::FeatureList::IsEnabled(kOobePersonalizedOnboarding);
}

bool IsOobeSoftwareUpdateEnabled() {
  return base::FeatureList::IsEnabled(kOobeSoftwareUpdate);
}

bool IsOobeLazyLoadingEnabled() {
  return base::FeatureList::IsEnabled(kOobeLazyLoading);
}

bool IsOobePerksDiscoveryEnabled() {
  return base::FeatureList::IsEnabled(kOobePerksDiscovery);
}

bool IsOobeQuickStartEnabled() {
  return IsCrossDeviceFeatureSuiteAllowed() &&
         base::FeatureList::IsEnabled(kOobeQuickStart);
}

bool IsOobeQuickStartOnLoginScreenEnabled() {
  return IsOobeQuickStartEnabled() &&
         base::FeatureList::IsEnabled(kOobeQuickStartOnLoginScreen);
}

bool IsOobeTouchpadScrollEnabled() {
  return IsOobeChoobeEnabled() &&
         base::FeatureList::IsEnabled(kOobeTouchpadScroll);
}

bool IsOobeDisplaySizeEnabled() {
  return IsOobeChoobeEnabled() &&
         base::FeatureList::IsEnabled(kOobeDisplaySize);
}

bool IsOobeInputMethodsEnabled() {
  return IsOobeChoobeEnabled() &&
         base::FeatureList::IsEnabled(kOobeInputMethods);
}

bool IsOobeSplitModifierKeyboardInfoEnabled() {
  return base::FeatureList::IsEnabled(kOobeSplitModifierKeyboardInfo);
}

bool IsOsFeedbackDialogEnabled() {
  return base::FeatureList::IsEnabled(kOsFeedbackDialog);
}

bool IsOsSettingsDeprecateDnsDialogEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsDeprecateDnsDialog);
}

bool IsOsSettingsRevampWayfindingEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsRevampWayfinding);
}

bool IsOsSyncConsentRevampEnabled() {
  return base::FeatureList::IsEnabled(kOsSyncConsentRevamp);
}

bool IsOverviewDeskNavigationEnabled() {
  return base::FeatureList::IsEnabled(kOverviewDeskNavigation);
}

bool IsParentAccessJellyEnabled() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kParentAccessJelly);
}

bool IsPasswordlessGaiaEnabledForConsumers() {
  return base::FeatureList::IsEnabled(kPasswordlessGaiaForConsumers);
}

bool IsPcieBillboardNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPcieBillboardNotification);
}

bool IsPerDeskShelfEnabled() {
  return base::FeatureList::IsEnabled(kPerDeskShelf);
}

bool IsPeripheralNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPeripheralNotification) &&
         IsPeripheralCustomizationEnabled();
}

bool IsPhoneHubCameraRollEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCameraRoll);
}

bool IsPhoneHubMonochromeNotificationIconsEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubMonochromeNotificationIcons);
}

bool IsPhoneHubOnboardingNotifierRevampEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubOnboardingNotifierRevamp);
}

bool IsPhoneHubFeatureSetupErrorHandlingEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubFeatureSetupErrorHandling);
}

bool IsPhoneHubPingOnBubbleOpenEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubPingOnBubbleOpen);
}

bool IsPhoneHubEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHub);
}

bool IsPhoneHubAttestationRetriesEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubAttestationRetries);
}

bool IsPhoneHubCallNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCallNotification);
}

bool IsPhoneHubShortQuickActionPodsTitlesEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubShortQuickActionPodsTitles);
}

bool IsPickerUpdateEnabled() {
  return base::FeatureList::IsEnabled(kPicker);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsPipDoubleTapToResizeEnabled() {
  return base::FeatureList::IsEnabled(kPipDoubleTapToResize);
}

bool IsPipTuckEnabled() {
  return base::FeatureList::IsEnabled(kPipTuck);
}

bool IsPrinterPreviewCrosAppEnabled() {
  return base::FeatureList::IsEnabled(kPrintPreviewCrosApp);
}

bool IsProductivityLauncherEnabled() {
  return base::FeatureList::IsEnabled(kProductivityLauncher);
}

bool IsProjectorManagedUserEnabled() {
  return base::FeatureList::IsEnabled(kProjectorManagedUser);
}

bool IsProjectorAppDebugMode() {
  return base::FeatureList::IsEnabled(kProjectorAppDebug);
}

bool IsProjectorCustomThumbnailEnabled() {
  return base::FeatureList::IsEnabled(kProjectorCustomThumbnail);
}

bool IsProjectorManagedUserIgnorePolicyEnabled() {
  return base::FeatureList::IsEnabled(kProjectorManagedUserIgnorePolicy);
}

bool IsProjectorShowShortPseudoTranscript() {
  return base::FeatureList::IsEnabled(kProjectorShowShortPseudoTranscript);
}

bool IsProjectorUpdateIndexableTextEnabled() {
  return base::FeatureList::IsEnabled(kProjectorUpdateIndexableText);
}

bool IsProjectorServerSideRecognitionFallbackImplEnabled() {
  return base::FeatureList::IsEnabled(
      kProjectorServerSideRecognitionFallbackImpl);
}

bool IsProjectorMutingEnabled() {
  return base::FeatureList::IsEnabled(kProjectorMuting);
}

bool IsProjectorRedirectToPwaEnabled() {
  return base::FeatureList::IsEnabled(kProjectorRedirectToPwa);
}

bool IsProjectorV2Enabled() {
  return base::FeatureList::IsEnabled(kProjectorV2);
}

bool IsProjectorTranscriptChapterTitleEnabled() {
  return base::FeatureList::IsEnabled(kProjectorTranscriptChapterTitle);
}

bool IsProjectorUseUSMForS3Enabled() {
  return base::FeatureList::IsEnabled(kProjectorUseUSMForS3);
}

bool IsProjectorDynamicColorsEnabled() {
  // For Projector, Gm3 requires dynamic colors.
  return base::FeatureList::IsEnabled(kProjectorDynamicColors) ||
         base::FeatureList::IsEnabled(kProjectorGm3);
}

bool IsProjectorGm3Enabled() {
  return base::FeatureList::IsEnabled(kProjectorGm3);
}

bool IsQuickDimEnabled() {
  return base::FeatureList::IsEnabled(kQuickDim) && switches::HasHps();
}

bool IsRenderArcNotificationsByChromeEnabled() {
  return base::FeatureList::IsEnabled(kRenderArcNotificationsByChrome);
}

bool IsRemoveStalePolicyPinnedAppsFromShelfEnabled() {
  return base::FeatureList::IsEnabled(kRemoveStalePolicyPinnedAppsFromShelf);
}

bool IsResetAudioSelectionImprovementPrefEnabled() {
  return base::FeatureList::IsEnabled(kResetAudioSelectionImprovementPref);
}

bool IsResetShortcutCustomizationsEnabled() {
  return base::FeatureList::IsEnabled(kResetShortcutCustomizations);
}

bool IsSameAppWindowCycleEnabled() {
  return base::FeatureList::IsEnabled(kSameAppWindowCycle);
}

bool IsSamlNotificationOnPasswordChangeSuccessEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableSamlNotificationOnPasswordChangeSuccess);
}

bool IsSavedDeskUiRevampEnabled() {
  return IsForestFeatureEnabled() &&
         base::FeatureList::IsEnabled(kSavedDeskUiRevamp);
}

bool IsScalableIphEnabled() {
  return base::FeatureList::IsEnabled(kScalableIph);
}

bool IsScalableIphDebugEnabled() {
  return base::FeatureList::IsEnabled(kScalableIphDebug);
}

bool IsScalableIphTrackingOnlyEnabled() {
  return base::FeatureList::IsEnabled(kScalableIphTrackingOnly);
}

bool IsScalableIphClientConfigEnabled() {
  return base::FeatureList::IsEnabled(kScalableIphClientConfig);
}

bool IsScalableShelfPodsEnabled() {
  return base::FeatureList::IsEnabled(kScalableShelfPods);
}

bool IsScannerEnabled() {
  return base::FeatureList::IsEnabled(kScannerUpdate);
}

bool IsSeaPenDemoModeEnabled() {
  return IsSeaPenEnabled() && base::FeatureList::IsEnabled(kSeaPenDemoMode);
}

bool IsSeaPenEnabled() {
  return base::FeatureList::IsEnabled(kSeaPen) &&
         base::FeatureList::IsEnabled(kFeatureManagementSeaPen);
}

bool IsSeaPenTextInputEnabled() {
  return IsSeaPenEnabled() && base::FeatureList::IsEnabled(kSeaPenTextInput);
}

bool IsSeaPenUseExptTemplateEnabled() {
  return IsSeaPenEnabled() &&
         base::FeatureList::IsEnabled(kSeaPenUseExptTemplate);
}

bool IsSeaPenEnterpriseEnabled() {
  return IsSeaPenEnabled() && base::FeatureList::IsEnabled(kSeaPenEnterprise);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
}

bool IsSeparatePasswordAndPinOnLoginEnabled() {
  return base::FeatureList::IsEnabled(kSeparatePasswordAndPinOnLogin);
}

bool IsSeparateWebAppShortcutBadgeIconEnabled() {
  return base::FeatureList::IsEnabled(kSeparateWebAppShortcutBadgeIcon);
}

bool IsSettingsAppNotificationSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSettingsAppNotificationSettings);
}

bool IsSettingsAppThemeChangeAnimationEnabled() {
  return base::FeatureList::IsEnabled(kSettingsAppThemeChangeAnimation);
}

bool IsShelfLauncherNudgeEnabled() {
  return base::FeatureList::IsEnabled(kShelfLauncherNudge);
}

bool IsShimlessRMAOsUpdateEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAOsUpdate);
}

bool IsShimlessRMA3pDiagnosticsEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMA3pDiagnostics);
}

bool IsShimlessRMA3pDiagnosticsDevModeEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMA3pDiagnosticsDevMode);
}

bool IsShimlessRMA3pDiagnosticsAllowPermissionPolicyEnabled() {
  return base::FeatureList::IsEnabled(
      kShimlessRMA3pDiagnosticsAllowPermissionPolicy);
}

bool IsShowSharingUserInLauncherContinueSectionEnabled() {
  return IsLauncherContinueSectionWithRecentsEnabled() &&
         base::FeatureList::IsEnabled(
             kShowSharingUserInLauncherContinueSection);
}

bool IsSmartReaderEnabled() {
  return base::FeatureList::IsEnabled(kSmartReader);
}

bool IsSunfishFeatureEnabled() {
  return base::FeatureList::IsEnabled(kSunfishFeature);
}

bool IsSuspendStateMachineEnabled() {
  return base::FeatureList::IsEnabled(kSuspendStateMachine);
}

bool IsSystemLiveCaptionEnabled() {
  // TODO(b/295244553): Once `kSystemLiveCaption` is enabled by default, switch
  // to `&&`.
  return base::FeatureList::IsEnabled(kSystemLiveCaption) ||
         base::FeatureList::IsEnabled(kFeatureManagementSystemLiveCaption);
}

bool IsSystemNudgeMigrationEnabled() {
  return base::FeatureList::IsEnabled(kSystemNudgeMigration);
}

bool IsSystemTrayShadowEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayShadow);
}

bool IsSysUiShouldHoldbackDriveIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kSysUiShouldHoldbackDriveIntegration) &&
         !base::FeatureList::IsEnabled(kIgnoreM129Holdback);
}

bool IsSysUiShouldHoldbackFocusModeEnabled() {
  return base::FeatureList::IsEnabled(kSysUiShouldHoldbackFocusMode) &&
         !base::FeatureList::IsEnabled(kIgnoreM129Holdback);
}

bool IsSysUiShouldHoldbackForestEnabled() {
  return base::FeatureList::IsEnabled(kSysUiShouldHoldbackForest) &&
         !base::FeatureList::IsEnabled(kIgnoreM129Holdback);
}

bool IsTetheringExperimentalFunctionalityEnabled() {
  return base::FeatureList::IsEnabled(kTetheringExperimentalFunctionality);
}

bool IsTilingWindowResizeEnabled() {
  return base::FeatureList::IsEnabled(kTilingWindowResize);
}

bool IsTimeOfDayScreenSaverEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementTimeOfDayScreenSaver) &&
         IsTimeOfDayWallpaperEnabled();
}

bool IsTimeOfDayWallpaperEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementTimeOfDayWallpaper);
}

bool IsTouchscreenMappingExperienceEnabled() {
  return base::FeatureList::IsEnabled(kEnableTouchscreenMappingExperience);
}

bool IsTouchpadInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableTouchpadsInDiagnosticsApp);
}

bool IsTouchscreenInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableTouchscreensInDiagnosticsApp);
}

bool IsTrafficCountersEnabled() {
  return base::FeatureList::IsEnabled(kTrafficCountersEnabled);
}

bool IsTrafficCountersForWiFiTestingEnabled() {
  return IsTrafficCountersEnabled() &&
         base::FeatureList::IsEnabled(kTrafficCountersForWiFiTesting);
}

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsUnmanagedDeviceDeviceTrustConnectorFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      kUnmanagedDeviceDeviceTrustConnectorEnabled);
}

bool ShouldUseAndroidStagingSmds() {
  return base::FeatureList::IsEnabled(kUseAndroidStagingSmds);
}

bool ShouldUseStorkSmds() {
  return base::FeatureList::IsEnabled(kUseStorkSmdsServerAddress);
}

bool IsUserEducationEnabled() {
  return IsWelcomeTourEnabled();
}

bool IsLiveCaptionUserMicrophoneEnabled() {
  return base::FeatureList::IsEnabled(kLiveCaptionUserMicrophone);
}

bool IsUpstreamTrustedReportsFirmwareEnabled() {
  return base::FeatureList::IsEnabled(kUpstreamTrustedReportsFirmware);
}

bool IsVideoConferenceEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementVideoConference);
}

bool IsBirchVideoConferenceSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kBirchVideoConferenceSuggestions);
}

bool IsStopAllScreenShareEnabled() {
  return base::FeatureList::IsEnabled(kVcStopAllScreenShare) &&
         IsVideoConferenceEnabled();
}

bool IsVcBackgroundReplaceEnabled() {
  return base::FeatureList::IsEnabled(kVcBackgroundReplace) &&
         IsVideoConferenceEnabled();
}

bool IsVcResizeThumbnailEnabled() {
  return base::FeatureList::IsEnabled(kVcResizeThumbnail);
}

bool IsVcDlcUiEnabled() {
  return base::FeatureList::IsEnabled(kVcDlcUi) && IsVideoConferenceEnabled();
}

bool IsVcPortraitRelightEnabled() {
  return base::FeatureList::IsEnabled(kVcPortraitRelight) &&
         IsVideoConferenceEnabled();
}

bool IsVcControlsUiFakeEffectsEnabled() {
  return base::FeatureList::IsEnabled(kVcControlsUiFakeEffects);
}

bool IsVcStudioLookEnabled() {
  return base::FeatureList::IsEnabled(kVcStudioLook);
}

bool IsVcTrayMicIndicatorEnabled() {
  return base::FeatureList::IsEnabled(kVcTrayMicIndicator);
}

bool IsVcTrayTitleHeaderEnabled() {
  return base::FeatureList::IsEnabled(kVcTrayTitleHeader);
}

bool IsVcWebApiEnabled() {
  return base::FeatureList::IsEnabled(kVcWebApi) && IsVideoConferenceEnabled();
}

bool IsVersionWallpaperInfoEnabled() {
  return base::FeatureList::IsEnabled(kVersionedWallpaperInfo);
}

bool IsWallpaperFastRefreshEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperFastRefresh);
}

bool IsWallpaperGooglePhotosSharedAlbumsEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperGooglePhotosSharedAlbums);
}

bool IsWelcomeExperienceEnabled() {
  return IsPeripheralCustomizationEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeExperience);
}

bool IsWelcomeExperienceTestUnsupportedDevicesEnabled() {
  return IsWelcomeExperienceEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeExperienceTestUnsupportedDevices);
}

bool IsWelcomeTourChromeVoxSupported() {
  return IsWelcomeTourEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeTourChromeVoxSupported);
}

bool IsWelcomeTourCounterfactuallyEnabled() {
  return IsWelcomeTourEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeTourCounterfactualArm);
}

bool IsWelcomeTourEnabled() {
  return base::FeatureList::IsEnabled(kWelcomeTour);
}

bool IsWelcomeTourForceUserEligibilityEnabled() {
  return IsWelcomeTourEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeTourForceUserEligibility);
}

bool IsWelcomeTourHoldbackEnabled() {
  return IsWelcomeTourEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeTourHoldbackArm);
}

bool IsWelcomeTourV2Enabled() {
  return IsWelcomeTourEnabled() && base::FeatureList::IsEnabled(kWelcomeTourV2);
}

bool IsWifiConcurrencyEnabled() {
  return base::FeatureList::IsEnabled(kWifiConcurrency);
}

bool IsWifiDirectEnabled() {
  return base::FeatureList::IsEnabled(kWifiDirect);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
}

bool IsWindowSplittingEnabled() {
  return base::FeatureList::IsEnabled(kWindowSplitting);
}

bool IsWmModeEnabled() {
  return base::FeatureList::IsEnabled(kWmMode);
}

bool IsSearchCustomizableShortcutsInLauncherEnabled() {
  return base::FeatureList::IsEnabled(kSearchCustomizableShortcutsInLauncher);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool IsFeatureAwareDeviceDemoModeEnabled() {
  return base::FeatureList::IsEnabled(
      kFeatureManagementFeatureAwareDeviceDemoMode);
}

bool ShouldUseKcerClientCertStore() {
  return base::FeatureList::IsEnabled(kUseKcerClientCertStore);
}

bool ShouldUseV1DeviceSync() {
  return false;
}

bool ShouldUseV2DeviceSync() {
  return true;
}

bool IsUseAuthPanelInSessionEnabled() {
  return base::FeatureList::IsEnabled(kUseAuthPanelInSession);
}

bool IsAllowPasswordlessSetupEnabled() {
  return base::FeatureList::IsEnabled(kAllowPasswordlessSetup);
}

bool IsAllowPinTimeoutSetupEnabled() {
  return base::FeatureList::IsEnabled(kAllowPinTimeoutSetup);
}

bool IsWebAuthNAuthDialogMergeEnabled() {
  return base::FeatureList::IsEnabled(kWebAuthNAuthDialogMerge);
}

bool ShouldEnterOverviewFromWallpaper() {
  return base::FeatureList::IsEnabled(kEnterOverviewFromWallpaper);
}

bool UseMixedFileLauncherContinueSection() {
  return (base::FeatureList::IsEnabled(kLauncherContinueSectionWithRecents) &&
          base::GetFieldTrialParamByFeatureAsBool(
              features::kLauncherContinueSectionWithRecents,
              "mix_local_and_drive", false)) ||
         (base::FeatureList::IsEnabled(
              kLauncherContinueSectionWithRecentsRollout) &&
          base::GetFieldTrialParamByFeatureAsBool(
              features::kLauncherContinueSectionWithRecentsRollout,
              "mix_local_and_drive", false));
}

}  // namespace ash::features
