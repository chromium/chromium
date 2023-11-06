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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the logic to show the notifications for Adaptive Charging features.
// This is intended to be used by developers to test the UI aspect of the
// feature.
BASE_FEATURE(kAdaptiveChargingForTesting,
             "AdaptiveChargingForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adjusts portrait mode split view to avoid the input field in the bottom
// window being occluded by the virtual keyboard.
BASE_FEATURE(kAdjustSplitViewForVK,
             "AdjustSplitViewForVK",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the advanced documentScan APIs for document scanners
// are available.
BASE_FEATURE(kAdvancedDocumentScanAPI,
             "AdvancedDocumentScanAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
BASE_FEATURE(kAllowAmbientEQ,
             "AllowAmbientEQ",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Cross-Device features, e.g. Nearby Share, Smart Lock, Fast Pair, etc.
// This flag is used to disable Cross-Device on platforms where we cannot yet
// guarantee a good experience with the stock Bluetooth hardware (e.g. Reven /
// ChromeOS Flex).
BASE_FEATURE(kAllowCrossDeviceFeatureSuite,
             "AllowCrossDeviceFeatureSuite",
#if BUILDFLAG(IS_REVEN)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_REVEN)
);

// Allows DevTools to open from the context menu and shortcut keys in Ash if
// Lacros is the only browser.
BASE_FEATURE(kAllowDevtoolsInSystemUI,
             "AllowDevtoolsInSystemUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows network connections which use EAP methods that validate the
// server certificate to use the default server CA certificate without
// verifying the servers identity.
BASE_FEATURE(kAllowEapDefaultCasWithoutSubjectVerification,
             "AllowEapDefaultCasWithoutSubjectVerification",
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

// Controls whether the support for suppressing text messages is enabled.
BASE_FEATURE(kSuppressTextMessages,
             "SuppressTextMessages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAltClickAndSixPackCustomization,
             "AltClickAndSixPackCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable AutoEnrollment for Kiosk in OOBE
BASE_FEATURE(kAutoEnrollmentKioskInOobe,
             "AutoEnrollmentKioskInOobe",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow Dev channel to use Prod server feature.
BASE_FEATURE(kAmbientModeDevUseProdFeature,
             "ChromeOSAmbientModeDevChannelUseProdServer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Ambient mode album selection with photo previews.
BASE_FEATURE(kAmbientModePhotoPreviewFeature,
             "ChromeOSAmbientModePhotoPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to throttle the frame rate of Lottie animations in ambient
// mode. The slower frame rate may lead to power consumption savings, but also
// may decrease the animation's smoothness if not done properly.
BASE_FEATURE(kAmbientModeThrottleAnimation,
             "ChromeOSAmbientModeThrottleAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the logic for managed screensaver is enabled or not.
BASE_FEATURE(kAmbientModeManagedScreensaver,
             "ChromeOSAmbientModeManagedScreensaver",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApnRevamp, "ApnRevamp", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppLaunchAutomation,
             "AppLaunchAutomation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable ARC ADB sideloading support.
BASE_FEATURE(kArcAdbSideloadingFeature,
             "ArcAdbSideloading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether files shared from ARC apps to Web Apps should be shared
// through the FuseBox service.
BASE_FEATURE(kArcFuseBoxFileSharing,
             "ArcFuseBoxFileSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable support for ARC Input Overlay Beta.
BASE_FEATURE(kArcInputOverlayBeta,
             "ArcInputOverlayBeta",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable support for ARC Input Overlay Alpha v2.
BASE_FEATURE(kArcInputOverlayAlphaV2,
             "ArcInputOverlayAlphaV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
BASE_FEATURE(kArcManagedAdbSideloadingSupport,
             "ArcManagedAdbSideloadingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable enhanced assistive emoji suggestions.
BASE_FEATURE(kAssistEmojiEnhanced,
             "AssistEmojiEnhanced",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable assistive multi word suggestions.
BASE_FEATURE(kAssistMultiWord,
             "AssistMultiWord",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable assistive multi word suggestions on an expanded
// list of surfaces.
BASE_FEATURE(kAssistMultiWordExpanded,
             "AssistMultiWordExpanded",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementOrca,
             "FeatureManagementOrca",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantNativeIcons,
             "AssistantNativeIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether document scanners are discovered with the synchronous
// ListScanners API or the new asynchronous StartScannerDiscovery API.
BASE_FEATURE(kAsynchronousScannerDiscovery,
             "AsynchronousScannerDiscovery",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables warning in the quick settings when NBS device is in use.
BASE_FEATURE(kAudioHFPNbsWarning,
             "AudioHFPNbsWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the ui to show the toggle for controlling hfp-mic-sr.
BASE_FEATURE(kAudioHFPMicSRToggle,
             "AudioHFPMicSRToggle",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutocorrectUseReplaceSurroundingText,
             "AutocorrectUseReplaceSurroundingText",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the autozoom nudge shown prefs will be reset at the start of
// each new user session.
BASE_FEATURE(kAutozoomNudgeSessionReset,
             "AutozoomNudgeSessionReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables loading avatar images from the cloud on ChromeOS.
BASE_FEATURE(kAvatarsCloudMigration,
             "AvatarsCloudMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Make Battery Saver available.
BASE_FEATURE(kBatterySaver,
             "CrosBatterySaver",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
BASE_FEATURE(kBluetoothFixA2dpPacketSize,
             "BluetoothFixA2dpPacketSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Bluetooth Quality Report feature.
BASE_FEATURE(kBluetoothQualityReport,
             "BluetoothQualityReport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Bluetooth WBS microphone be selected as default
// audio input option.
BASE_FEATURE(kBluetoothWbsDogfood,
             "BluetoothWbsDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRobustAudioDeviceSelectLogic,
             "RobustAudioDeviceSelectLogic",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable Big GL when using Borealis.
BASE_FEATURE(kBorealisBigGl, "BorealisBigGl", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable dGPU when using Borealis.
BASE_FEATURE(kBorealisDGPU, "BorealisDGPU", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable experimental disk management changes for Borealis.
BASE_FEATURE(kBorealisDiskManagement,
             "BorealisDiskManagement",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable borealis on this device. This won't necessarily allow it, since you
// might fail subsequent checks.
BASE_FEATURE(kBorealisPermitted,
             "BorealisPermitted",
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

// Enable the "provision" flag when mounting Borealis' stateful disk.
// TODO(b/288361720): This is temporary while we test the 'provision'
// mount option. Once we're satisfied things are stable, we'll make this
// the default and remove this feature/flag.
BASE_FEATURE(kBorealisProvision,
             "BorealisProvision",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable storage ballooning for Borealis. This takes precedence over
// kBorealisDiskManagement.
BASE_FEATURE(kBorealisStorageBallooning,
             "BorealisStorageBallooning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the new WebUI installer instead of views installer.
BASE_FEATURE(kBorealisWebUIInstaller,
             "BorealisWebUIInstaller",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable calendar jelly.
BASE_FEATURE(kCalendarJelly, "CalendarJelly", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables to allow time-lapse video recording in the camera app.
BASE_FEATURE(kCameraAppTimeLapse,
             "CameraAppTimeLapse",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the camera effects are supported by hardware.
// Note that this feature can be overridden by login_manager based on
// whether a per-board build sets the USE camera_feature_effects flag.
// Refer to: chromiumos/src/platform2/login_manager/chrome_setup.cc
BASE_FEATURE(kCameraEffectsSupportedByHardware,
             "CameraEffectsSupportedByHardware",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
BASE_FEATURE(kCameraPrivacySwitchNotifications,
             "CameraPrivacySwitchNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the capture mode advanced audio settings are enabled.
BASE_FEATURE(kCaptureModeAudioMixing,
             "CaptureModeAudioMixing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the tour that walks new users through the Capture Mode feature.
BASE_FEATURE(kCaptureModeTour,
             "CaptureModeTour",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allow eSIM installation bypass the non-cellular internet
// connectivity check.
BASE_FEATURE(kCellularBypassESimInstallationConnectivityCheck,
             "CellularBypassESimInstallationConnectivityCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the Cellular Carrier Lock manager will start on every boot to
// check state of lock configuration and setup the modem if needed.
// This flag will be removed before launch (http://launch/4211912).
BASE_FEATURE(kCellularCarrierLock,
             "CellularCarrierLock",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kCellularCarrierLockLastConfig{
    &kCellularCarrierLock, "LastConfigDateDelta", -2};

// If enabled, use second the Euicc that is exposed by Hermes in Cellular Setup
// and Settings.
BASE_FEATURE(kCellularUseSecondEuicc,
             "CellularUseSecondEuicc",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Multiple scraped passwords should be checked against password in
// cryptohome.
BASE_FEATURE(kCheckPasswordsAgainstCryptohomeHelper,
             "CheckPasswordsAgainstCryptohomeHelper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, an educational footer may appear at the bottom of the clipboard
// history menu. Whether the educational footer appears is based on user/device
// state, such as when the user last used the clipboard history menu or when a
// clipboard history nudge was last seen.
BASE_FEATURE(kClipboardHistoryFooter,
             "ClipboardHistoryFooter",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, clipboard history explicitly pastes into `content::WebContents`
// instead of using synthetic key events.
BASE_FEATURE(kClipboardHistoryWebContentsPaste,
             "ClipboardHistoryWebContentsPaste",
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

// Adds location access control to Privacy Hub.
BASE_FEATURE(kCrosPrivacyHub,
             "CrosPrivacyHub",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables app permissions view inside Priacy Hub.
BASE_FEATURE(kCrosPrivacyHubAppPermissions,
             "CrosPrivacyHubAppPermissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Privacy Hub with only the camera and the microphone access control.
BASE_FEATURE(kCrosPrivacyHubV0,
             "CrosPrivacyHubV0",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables syncing attestation certificates to cryptauth for use by Cross Device
// features, including Eche and Phone Hub.
BASE_FEATURE(kCryptauthAttestationSyncing,
             "CryptauthAttestationSyncing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables contextual nudges for gesture education.
BASE_FEATURE(kContextualNudges,
             "ContextualNudges",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables Crostini IME support.
BASE_FEATURE(kCrostiniImeSupport,
             "CrostiniImeSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Crostini Qt application IME support.
BASE_FEATURE(kCrostiniQtImeSupport,
             "CrostiniQtImeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini upgrade to LXD v5.
BASE_FEATURE(kCrostiniUseLxd5,
             "CrostiniUseLxd5",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Virtual Keyboard support.
BASE_FEATURE(kCrostiniVirtualKeyboardSupport,
             "CrostiniVirtualKeyboardSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables support for third party VMs.
BASE_FEATURE(kBruschetta, "Bruschetta", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Captive Portal Error Page changes, which shows a suggestion in
// the Chrome error page on ChromeOS when behind a captive portal.
BASE_FEATURE(kCaptivePortalErrorPage,
             "CaptivePortalErrorPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables always using device-activity-status data to filter
// eligible host phones.
BASE_FEATURE(kCryptAuthV2AlwaysUseActiveEligibleHosts,
             "kCryptAuthV2AlwaysUseActiveEligibleHosts",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is disabled via the
// feature flag kDisableCryptAuthV1DeviceSync.
BASE_FEATURE(kCryptAuthV2DeviceSync,
             "CryptAuthV2DeviceSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the CryptAuth v2 Enrollment flow.
BASE_FEATURE(kCryptAuthV2Enrollment,
             "CryptAuthV2Enrollment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the cryptohome recovery feature:
// - Enable recovery via the recovery service.
// - New UI for Cryptohome recovery and Gaia password changed screen.
// - Adds a "recover user" button to the error bubble that opens when the
//   user fails to enter their correct password.
// - Enables the UI to enable or disable cryptohome recovery in the settings
// page. Also guards the wiring of cryptohome recovery settings to the
// cryptohome backend.
BASE_FEATURE(kCryptohomeRecovery,
             "CryptohomeRecovery",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Deprecates Ctrl+Alt+/ as shortcut to open Keyboard shortcuts app, shows
// a notification if that shortcut is pressed, and replaces that shortcut with
// Ctrl+Search+S to open the app.
BASE_FEATURE(kDeprecateOldKeyboardShortcutsAccelerator,
             "DeprecateOldKeyboardShortcutsAccelerator",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds a desk button to the shelf that the user can use to navigate between
// desks.
BASE_FEATURE(kDeskButton, "DeskButton", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables indicators to hint where displays are connected.
BASE_FEATURE(kDisplayAlignAssist,
             "DisplayAlignAssist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable DNS over HTTPS(DoH) with identifiers.Only available on ChromeOS.
BASE_FEATURE(kDnsOverHttpsWithIdentifiers,
             "DnsOverHttpsWithIdentifiers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable experiment to support identifiers in the existing policy
// DnsOverHttpsTemplates. When this option is enabled, a hard-coded salt value
// is used for hashing the identifiers in the template URI. Only available on
// ChromeOS.
// TODO(acostinas, srad, b/233845305) Remove when policy is added to DPanel.
BASE_FEATURE(kDnsOverHttpsWithIdentifiersReuseOldPolicy,
             "DnsOverHttpsWithIdentifiersReuseOldPolicy",
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

// Enables support for the dropdown panel.
BASE_FEATURE(kDropdownPanel,
             "DropdownPanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables exporting of the selected Preferences so that they can be accessed
// early in the sign-in flow, before loading Profile.
BASE_FEATURE(kEnableEarlyPrefs,
             "EnableEarlyPrefs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables external keyboard testers in the diagnostics app.
BASE_FEATURE(kEnableExternalKeyboardsInDiagnostics,
             "EnableExternalKeyboardsInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables loading debug daemon logs for feedback in parallel to reduce client
// side wait time.
BASE_FEATURE(kEnableGetDebugdLogsInParallel,
             "EnableGetDebugdLogsInParallel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the device hostname.
BASE_FEATURE(kEnableHostnameSetting,
             "EnableHostnameSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the diagnostics app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kDiagnosticsAppJelly,
             "kDiagnosticsAppJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables keyboard backlight toggle.
BASE_FEATURE(kEnableKeyboardBacklightToggle,
             "EnableKeyboardBacklightToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Login WebUI was always loaded for legacy reasons even when it was not needed.
// When enabled, it will make login WebUI loaded only before showing it.
BASE_FEATURE(kEnableLazyLoginWebUILoading,
             "EnableLazyLoginWebUILoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables Kiosk enrollment option in OOBE.
BASE_FEATURE(kEnableKioskEnrollmentInOobe,
             "EnableKioskEnrollmentInOobe",
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

// Enables per-desk Z order for all-desk windows.
BASE_FEATURE(kEnablePerDeskZOrder,
             "EnablePerDeskZOrder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables RFC8925 (prefer IPv6-only on an IPv6-only-capable network).
BASE_FEATURE(kEnableRFC8925,
             "EnableRFC8925",
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

// Enables WiFi QoS to detect and prioritize selected egress network traffic
// using WiFi QoS/WMM in congested WiFi environments.
BASE_FEATURE(kEnableWifiQos,
             "EnableWifiQos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enforces Ash extension keep-list. Only the extensions/Chrome apps in the
// keep-list are enabled in Ash.
BASE_FEATURE(kEnforceAshExtensionKeeplist,
             "EnforceAshExtensionKeeplist",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables to check KeyEvent flag to see if the event is consumed by IME
// or not (=decides using heuristics based on key code etc.).
BASE_FEATURE(kExoConsumedByImeByFlag,
             "ExoConsumedByImeByFlag",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using an extended Wayland API for ConfirmCompositionText.
BASE_FEATURE(kExoExtendedConfirmComposition,
             "ExoExtendedConfirmComposition",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables to check KeyEvent flag to see if the event is consumed by IME
// or not (=decides using heuristics based on key code etc.).
BASE_FEATURE(kExoSurroundingTextOffset,
             "ExoSurroundingTextOffset",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows RGB Keyboard to test new animations/patterns.
BASE_FEATURE(kExperimentalRgbKeyboardPatterns,
             "ExperimentalRgbKeyboardPatterns",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the System Web App (SWA) of Face ML.
// This app needs both CrOS and hardware support (Face Auth Camera and System
// Face Auth Service), therefore we only enable it on these eligible devices.
BASE_FEATURE(kFaceMLApp, "FaceMLApp", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables prototype support for Fast Pair HID.
BASE_FEATURE(kFastPairHID, "FastPairHID", base::FEATURE_DISABLED_BY_DEFAULT);

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
// device after Fast Pair.
BASE_FEATURE(kFastPairPwaCompanion,
             "FastPairPwaCompanion",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The URI for the Fast Pair web companion.
const base::FeatureParam<std::string> kFastPairPwaCompanionInstallUri{
    &kFastPairPwaCompanion, "pwa-companion-install-uri",
    /*default*/ ""};

// (optional) The app ID for the installed Fast Pair web companion.
// e.g. ncmjhecbjeaamljdfahankockkkdmedg
const base::FeatureParam<std::string> kFastPairPwaCompanionAppId{
    &kFastPairPwaCompanion, "pwa-companion-app-id",
    /*default*/ ""};

// (optional) The Play Store link to download the Fast Pair web companion.
// e.g.
// https://play.google.com/store/apps/details?id=com.google.android.apps.photos
const base::FeatureParam<std::string> kFastPairPwaCompanionPlayStoreUri{
    &kFastPairPwaCompanion, "pwa-companion-play-store-uri",
    /*default*/ ""};

// Sets Fast Pair scanning to low power mode.
BASE_FEATURE(kFastPairLowPower,
             "FastPairLowPower",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The amount of seconds we should scan while in low power mode before stopping.
const base::FeatureParam<double> kFastPairLowPowerActiveSeconds{
    &kFastPairLowPower, "active-seconds", 2};

// The amount of seconds we should pause scanning while in low power mode.
const base::FeatureParam<double> kFastPairLowPowerInactiveSeconds{
    &kFastPairLowPower, "inactive-seconds", 3};

// Allows Fast Pair to use software scanning on devices which don't support
// hardware offloading of BLE scans.
BASE_FEATURE(kFastPairSoftwareScanning,
             "FastPairSoftwareScanning",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, shows a system update notification. This flag is intended to be
// controlled by the feature management module.
BASE_FEATURE(kFeatureManagementUpdateNotification,
             "FeatureManagementUpdateNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUpdateNotification,
             "UpdateNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows borealis on certain boards whose features are determined by
// FeatureManagement. This feature does not apply to all boards, and does not
// guarantee borealis will be available (due to additional hardware checks).
BASE_FEATURE(kFeatureManagementBorealis,
             "FeatureManagementBorealis",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated service to schedule tasks. If disabled, federated
// service works as a simple example receiver and storage.
// This is useful when we want to disable the federated tasks only and allow the
// customers to report examples, because e.g. the tensorflow graphs cost too
// much resources while example storage is supposed to be cheap and safe.
BASE_FEATURE(kFederatedServiceScheduleTasks,
             "FederatedServiceScheduleTasks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated strings service.
BASE_FEATURE(kFederatedStringsService,
             "FederatedStringsService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated strings service to schedule tasks.
BASE_FEATURE(kFederatedStringsServiceScheduleTasks,
             "FederatedStringsServiceScheduleTasks",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFederatedTimezoneCodePhh,
             "FederatedTimezoneCodePhh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables scheduling of launcher query federated analytics tasks.
BASE_FEATURE(kFederatedLauncherQueryAnalyticsTask,
             "FederatedLauncherQueryAnalyticsTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental UI features in Files app.
BASE_FEATURE(kFilesAppExperimental,
             "FilesAppExperimental",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the files transfer conflict dialog in Files app.
BASE_FEATURE(kFilesConflictDialog,
             "FilesConflictDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables shortcut icons to be shown in Google Drive when an item is a
// shortcut.
BASE_FEATURE(kFilesDriveShortcuts,
             "FilesDriveShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable inline sync status in Files app.
BASE_FEATURE(kFilesInlineSyncStatus,
             "FilesInlineSyncStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable inline sync status in Files app to consume events from the new more
// robust ProgressEvents (enables inline sync status for downsync events).
BASE_FEATURE(kFilesInlineSyncStatusProgressEvents,
             "FilesInlineSyncStatusProgressEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables V2 of search functionality in files.
BASE_FEATURE(kFilesSearchV2, "FilesSearchV2", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables partitioning of removable disks in file manager.
BASE_FEATURE(kFilesSinglePartitionFormat,
             "FilesSinglePartitionFormat",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable files app trash for Drive.
BASE_FEATURE(kFilesTrashDrive,
             "FilesTrashDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the new Google Drive page in OS Settings.
BASE_FEATURE(kFilesGoogleDriveSettingsPage,
             "FilesGoogleDriveSettingsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the firmware update app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kFirmwareUpdateJelly,
             "FirmwareUpdateJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables first party Vietnamese input method.
BASE_FEATURE(kFirstPartyVietnameseInput,
             "FirstPartyVietnameseInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Floating Workspace feature on ChromeOS
BASE_FEATURE(kFloatingWorkspace,
             "FloatingWorkspace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables chrome.fileSystemProvider file systems in Files app Recents view.
BASE_FEATURE(kFSPsInRecents,
             "FSPsInRecents",
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
        base::Seconds(15)};

// Time interval to capture current desk as desk template and upload template to
// server.
constexpr base::FeatureParam<base::TimeDelta>
    kFloatingWorkspaceV2PeriodicJobIntervalInSeconds{
        &kFloatingWorkspaceV2, "PeriodicJobIntervalInSeconds",
        base::Seconds(30)};

// Enables or disables Focus Mode feature on ChromeOS.
BASE_FEATURE(kFocusMode, "FocusMode", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, makes the Projector app use server side speech
// recognition instead of on-device speech recognition.
BASE_FEATURE(kForceEnableServerSideSpeechRecognitionForDev,
             "ForceEnableServerSideSpeechRecognitionForDev",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Drive to forcibly resync office files. Operations such as copy,
// move, ZIP on MS Office files call on the Drive to resync the files.
BASE_FEATURE(kForceReSyncDrive,
             "ForceReSyncDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow keeping full screen mode after unlock.
BASE_FEATURE(kFullscreenAfterUnlockAllowed,
             "FullscreenAfterUnlockAllowed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, there will be an alert bubble showing up when the device
// returns from low brightness (e.g., sleep, closed cover) without a lock screen
// and the active window is in fullscreen.
// TODO(https://crbug.com/1107185): Remove this after the feature is launched.
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

// Enable a notification to provide an option to open Gallery app for a
// downloaded pdf file.
BASE_FEATURE(kGalleryAppPdfEditNotification,
             "GalleryAppPdfEditNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Button label text used for the above kGalleryAppPdfEditNotification.
const base::FeatureParam<std::string> kGalleryAppPdfEditNotificationText{
    &kGalleryAppPdfEditNotification, "text", ""};

// Enables glanceables on time management surface.
BASE_FEATURE(kGlanceablesV2,
             "GlanceablesV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables glanceables on time management surface for trusted testers.
BASE_FEATURE(kGlanceablesV2TrustedTesters,
             "GlanceablesV2TrustedTesters",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables glanceables on time management surface for classroom teachers.
BASE_FEATURE(kGlanceablesV2ClassroomTeacherView,
             "GlanceablesV2ClassroomTeacherView",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing error messages for glanceables bubbles.
BASE_FEATURE(kGlanceablesV2ErrorMessage,
             "GlanceablesV2ErrorMessage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ability to add or edit tasks in tasks glanceable bubble.
BASE_FEATURE(kGlanceablesV2TasksAddEdit,
             "GlanceablesV2TasksAddEdit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Gaia reauth endpoint for all online reauth flows on login screen.
// Note that the reauth endpoint is used when the user is a child user or in
// potential recovery flows, regardless of the flag value.
BASE_FEATURE(kGaiaReauthEndpoint,
             "GaiaReauthEndpoint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Game Dashboard.
BASE_FEATURE(kGameDashboard,
             "GameDashboard",
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
BASE_FEATURE(kGifRecording, "GifRecording", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the displaying of animated gifs in ash.
BASE_FEATURE(kGifRendering, "GifRendering", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a Files banner about Google One offer.
BASE_FEATURE(kGoogleOneOfferFilesBanner,
             "GoogleOneOfferFilesBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables editing with handwriting gestures within the virtual keyboard.
BASE_FEATURE(kHandwritingGestureEditing,
             "HandwritingGestureEditing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new on-device recognition for legacy handwriting input.
BASE_FEATURE(kHandwritingLegacyRecognition,
             "HandwritingLegacyRecognition",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables downloading the handwriting libraries via DLC.
BASE_FEATURE(kHandwritingLibraryDlc,
             "HandwritingLibraryDlc",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the logic that auto triggers the install dialog during the web app
// install flow initiated from the Help App.
BASE_FEATURE(kHelpAppAutoTriggerInstallDialog,
             "HelpAppAutoTriggerInstallDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the Help App Discover tab notifications on non-stable
// ChromeOS channels. Used for testing.
BASE_FEATURE(kHelpAppDiscoverTabNotificationAllChannels,
             "HelpAppDiscoverTabNotificationAllChannels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable showing search results from the help app in the launcher.
BASE_FEATURE(kHelpAppLauncherSearch,
             "HelpAppLauncherSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
BASE_FEATURE(kHiddenNetworkWarning,
             "HiddenNetworkWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables hiding of ARC media notifications. If this is enabled, all ARC
// notifications that are of the media type will not be shown. This
// is because they will be replaced by native media session notifications.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
BASE_FEATURE(kHideArcMediaNotifications,
             "HideArcMediaNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy).
BASE_FEATURE(kHideShelfControlsInTabletMode,
             "HideShelfControlsInTabletMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, add Hindi Inscript keyboard layout.
BASE_FEATURE(kHindiInscriptLayout,
             "HindiInscriptLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Camera app integration with holding space.
BASE_FEATURE(kHoldingSpaceCameraAppIntegration,
             "HoldingSpaceCameraAppIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables holding space icon to be permanently displayed with extended file
// expiration to increase predictability of the feature.
BASE_FEATURE(kHoldingSpacePredictability,
             "HoldingSpacePredictability",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables refresh of holding space UI to better convey the relationship with
// the Files app to simplify feature comprehension.
BASE_FEATURE(kHoldingSpaceRefresh,
             "HoldingSpaceRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables suggestions in the pinned files section of Holding Space.
BASE_FEATURE(kHoldingSpaceSuggestions,
             "HoldingSpaceSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the tour that walks new users through the Holding Space feature.
BASE_FEATURE(kHoldingSpaceTour,
             "HoldingSpaceTour",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kHoldingSpaceTourDropToPinEnabled{
    &kHoldingSpaceTour, "drop-to-pin", false};

const base::FeatureParam<bool> kHoldingSpaceTourEnabledCounterfactually{
    &kHoldingSpaceTour, "is-counterfactual", false};

BASE_FEATURE(kHomeButtonQuickAppAccess,
             "HomeButtonQuickAppAccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a call-to-action label beside the home button.
BASE_FEATURE(kHomeButtonWithText,
             "HomeButtonWithText",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control whether the hotspot tethering is enabled. When enabled, it will allow
// the Chromebook to share its cellular internet connection to other devices.
BASE_FEATURE(kHotspot, "Hotspot", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows the user to cycle between windows of an app using Alt + `.
BASE_FEATURE(kSameAppWindowCycle,
             "SameAppWindowCycle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the snooping protection prototype is enabled.
BASE_FEATURE(kSnoopingProtection,
             "SnoopingProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to start AssistantAudioDecoder service on demand (at query
// response time).
BASE_FEATURE(kStartAssistantAudioDecoderOnDemand,
             "StartAssistantAudioDecoderOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable a new header bar for the ChromeOS virtual keyboard.
BASE_FEATURE(kVirtualKeyboardNewHeader,
             "VirtualKeyboardNewHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, used to configure the heuristic rules for some advanced IME
// features (e.g. auto-correct).
BASE_FEATURE(kImeRuleConfig, "ImeRuleConfig", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, use the updated logic for downloading IME artifacts.
BASE_FEATURE(kImeDownloaderUpdate,
             "ImeDownloaderUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, use the updated parameters for the decoder.
BASE_FEATURE(kImeFstDecoderParamsUpdate,
             "ImeFstDecoderParamsUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled use the updated US English IME language models.
BASE_FEATURE(kImeUsEnglishModelUpdate,
             "ImeUsEnglishModelUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker falling back to clipboard.
BASE_FEATURE(kImeSystemEmojiPickerClipboard,
             "SystemEmojiPickerClipboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable system emoji picker extension
BASE_FEATURE(kImeSystemEmojiPickerExtension,
             "SystemEmojiPickerExtension",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker GIF support
BASE_FEATURE(kImeSystemEmojiPickerGIFSupport,
             "SystemEmojiPickerGIFSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable system emoji picker jelly support
BASE_FEATURE(kImeSystemEmojiPickerJellySupport,
             "SystemEmojiPickerJellySupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable system emoji picker search extension
BASE_FEATURE(kImeSystemEmojiPickerSearchExtension,
             "SystemEmojiPickerSearchExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable a new UI for stylus writing on the virtual keyboard
BASE_FEATURE(kImeStylusHandwriting,
             "StylusHandwriting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to hide voice button in IME tray if accessibility mic icon
// is already shown in the shelf.
BASE_FEATURE(kImeTrayHideVoiceButton,
             "ImeTrayHideVoiceButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables debug info UI for Korean input method's internal-mode switch.
BASE_FEATURE(kImeKoreanModeSwitchDebug,
             "ImeKoreanModeSwitchDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show new improved UI for cryptohome errors that happened
// during login. UI contains links to help center and might provide actions
// that can be taken to resolve the problem.
BASE_FEATURE(kImprovedLoginErrorHandling,
             "ImprovedLoginErrorHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Instant Hotspot rebrand/feature improvements. crbug/290075504.
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

// Enables sending `client-info` values to IPP printers on ChromeOS.
BASE_FEATURE(kIppClientInfo, "IppClientInfo", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables new experimental IPP-first setup path for USB printers on ChromeOS.
// Used in finch experiment.
BASE_FEATURE(kIppFirstSetupForUsbPrinters,
             "IppFirstSetupForUsbPrinters",
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

// Enables to use lacros-chrome as the only web browser on ChromeOS.
// This may not be allowed depending on user types and/or policies.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking
// the feature directly.
BASE_FEATURE(kLacrosOnly, "LacrosOnly", base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, wayland logging is enabled for Lacros.
BASE_FEATURE(kLacrosWaylandLogging,
             "LacrosWaylandLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Emergency switch to turn off profile migration.
BASE_FEATURE(kLacrosProfileMigrationForceOff,
             "LacrosProfileMigrationForceOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, it is allowed to migrate data from lacros back to ash, provided
// that other conditions are also met (e.g. the policy is enabled, or the
// command line flag is passed).
BASE_FEATURE(kLacrosProfileBackwardMigration,
             "LacrosProfileBackwardMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new flow for license packaged devices with enterprise license.
BASE_FEATURE(kLicensePackagedOobeFlow,
             "LicensePackagedOobeFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables local password as an option for local authentication.
// (This feature is only available for consumer users)
BASE_FEATURE(kLocalPasswordForConsumers,
             "LocalPasswordForConsumers",
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

// Enables new flow for Education license packaged devices.
BASE_FEATURE(kEducationEnrollmentOobeFlow,
             "EducationEnrollmentOobeFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables notifications on the lock screen.
BASE_FEATURE(kLockScreenNotifications,
             "LockScreenNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lock screen media controls UI and use of media keys on the lock
// screen.
BASE_FEATURE(kLockScreenMediaControls,
             "LockScreenMediaControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Device Trust connector client code is enabled on the login
// screen.
BASE_FEATURE(kLoginScreenDeviceTrustConnectorEnabled,
             "LoginScreenDeviceTrustConnectorEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to allow MAC address randomization to be enabled for WiFi networks.
BASE_FEATURE(kMacAddressRandomization,
             "MacAddressRandomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables PDF signature saving and a selection tool in the media app.
BASE_FEATURE(kMediaAppPdfA11yOcr,
             "MediaAppPdfA11yOcr",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification of when a microphone-using app is launched while the
// microphone is muted.
BASE_FEATURE(kMicMuteNotifications,
             "MicMuteNotifications",
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

// Enables multi-zone rgb keyboard customization.
BASE_FEATURE(kMultiZoneRgbKeyboard,
             "MultiZoneRgbKeyboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables support for multilingual assistive typing on ChromeOS.
BASE_FEATURE(kMultilingualTyping,
             "MultilingualTyping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Nearby Connections to specificy KeepAlive interval and timeout while
// also making the Nearby Connections WebRTC defaults longer.
BASE_FEATURE(kNearbyKeepAliveFix,
             "NearbyKeepAliveFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Notifier Collision to allow popup notifications and tray bubbles not
// overlap when showing on a display.
BASE_FEATURE(kNotifierCollision,
             "NotifierCollision",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether new Lockscreen reauth layout is shown or not.
BASE_FEATURE(kNewLockScreenReauthLayout,
             "NewLockScreenReauthLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new downloads integration with System UI surfaces.
BASE_FEATURE(kSysUiDownloadsIntegrationV2,
             "SysUiDownloadsIntegrationV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of the new System Nudges. (go/cros-educationalnudge-spec)
BASE_FEATURE(kSystemNudgeV2,
             "SystemNudgeV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Night Light feature.
BASE_FEATURE(kNightLight, "NightLight", base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled notification expansion animation.
BASE_FEATURE(kNotificationExpansionAnimation,
             "NotificationExpansionAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shorten notification timeouts to 6 seconds.
BASE_FEATURE(kNotificationExperimentalShortTimeouts,
             "NotificationExperimentalShortTimeouts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables dragging the image from a notification by mouse or gesture.
BASE_FEATURE(kNotificationImageDrag,
             "NotificationImageDrag",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, CHOBOE Screen will be shown during the new user onboarding flow.
BASE_FEATURE(kOobeChoobe, "OobeChoobe", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Drive Pinning Screen will be shown during
// the new user onboarding flow.
BASE_FEATURE(kOobeDrivePinning,
             "OobeDrivePinning",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables the ChromeOS OOBE HID Detection Revamp, which updates
// the OOBE HID detection screen UI and related infrastructure. See
// https://crbug.com/1299099.
BASE_FEATURE(kOobeHidDetectionRevamp,
             "OobeHidDetectionRevamp",
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

// Enables OOBE Simon features.
BASE_FEATURE(kFeatureManagementOobeSimon,
             "FeatureManagementOobeSimon",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOobeSimon, "OobeSimon", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Skipping the assistant setup screen in OOBE.
BASE_FEATURE(kOobeSkipAssistant,
             "OobeSkipAssistant",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the Oobe quick start flow.
BASE_FEATURE(kOobeQuickStart,
             "OobeQuickStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnlyShowNewShortcutsApp,
             "OnlyShowNewShortcutsApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables search customizable shortcuts in launcher.
BASE_FEATURE(kSearchCustomizableShortcutsInLauncher,
             "SearchCustomizableShortcutsInLauncher",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a new App Notifications subpage will appear in CrOS Apps section.
BASE_FEATURE(kOsSettingsAppNotificationsPage,
             "OsSettingsAppNotificationsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, OsSyncConsent Revamp will be shown.
// enabling this without enabling Lacros flag will have no effect
BASE_FEATURE(kOsSyncConsentRevamp,
             "OsSyncConsentRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the os feedback dialog will be used on OOBE and the login
// screeen.
BASE_FEATURE(kOsFeedbackDialog,
             "OsFeedbackDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the os feedback app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kOsFeedbackJelly,
             "OsFeedbackJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables app badging toggle to be displayed in app notification page in
// ChromeOS Settings.
BASE_FEATURE(kOsSettingsAppBadgingToggle,
             "OsSettingsAppBadgingToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the metrics toggle in settings user sync options page should be
// deprecated.
BASE_FEATURE(kOsSettingsDeprecateSyncMetricsToggle,
             "OsSettingsDeprecateSyncMetricsToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the wayfinding improvements for the ChromeOS Settings revamp
BASE_FEATURE(kOsSettingsRevampWayfinding,
             "OsSettingsRevampWayfinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the new Chrome component refresh for testing purposes in OS Settings.
BASE_FEATURE(kOsSettingsTestChromeRefresh,
             "OsSettingsTestChromeRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOverviewButton,
             "OverviewButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables user to provision PasspointARCSupport credentials.
BASE_FEATURE(kPasspointARCSupport,
             "PasspointARCSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables user to display Passpoint credentials in the UI.
BASE_FEATURE(kPasspointSettings,
             "PasspointSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature allows usage of passwordless flow in GAIA.
// (This feature is only available for consumer users)
BASE_FEATURE(kPasswordlessGaiaForConsumers,
             "PasswordlessGaiaForConsumers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a notification warning users that their Thunderbolt device is not
// supported on their CrOS device.
// TODO(crbug/1254930): Revisit this flag when there is a way to query billboard
// devices correctly.
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

// Determine should we display Beta badge for Eche.
BASE_FEATURE(kPhoneHubAppStreamingBetaBadge,
             "kPhoneHubAppStreamingBetaBadge",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

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
    PhoneHubNotifierTextGroup::kNotifierTextGroupA,
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

BASE_FEATURE(kPipDoubleTapToResize,
             "PipDoubleTapToResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables pinch to resize feature for Picture in Picture.
BASE_FEATURE(kPipPinchToResize,
             "PipPinchToResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the tilt feature for Picture in Picture.
BASE_FEATURE(kPipTilt, "PipTilt", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether policy provided trust anchors are allowed at the lock
// screen.
BASE_FEATURE(kPolicyProvidedTrustAnchorsAllowedAtLockScreen,
             "PolicyProvidedTrustAnchorsAllowedAtLockScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
BASE_FEATURE(kPreferConstantFrameRate,
             "PreferConstantFrameRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the observing of local printers to provide UI surfaces with live
// updates.
BASE_FEATURE(kLocalPrinterObserving,
             "LocalPrinterObserving",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the print management app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kPrintManagementJelly,
             "PrintManagementJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, improved messaging for printer setup displayed in print
// management app.
BASE_FEATURE(kPrintManagementSetupAssistance,
             "PrintManagementSetupAssistance",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new OS Printer Settings UI.
BASE_FEATURE(kPrinterSettingsRevamp,
             "PrinterSettingsRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing discovered printers in the Print Preview dialog.
BASE_FEATURE(kPrintPreviewDiscoveredPrinters,
             "PrintPreviewDiscoveredPrinters",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing printer status on the OS Printer Settings UI.
BASE_FEATURE(kPrinterSettingsPrinterStatus,
             "PrinterSettingsPrinterStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables to allocate more video capture buffers.
BASE_FEATURE(kMoreVideoCaptureBuffers,
             "MoreVideoCaptureBuffers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing notification and status area indicators when an app is
// using camera/microphone.
BASE_FEATURE(kPrivacyIndicators,
             "PrivacyIndicators",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Controls whether the transcript chapter title feature is enabled.
BASE_FEATURE(kProjectorTranscriptChapterTitle,
             "ProjectorTranscriptChapterTitle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable quick settings revamped view (go/qsrevamp).
BASE_FEATURE(kQsRevamp, "QsRevamp", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show toast notification when account switches.
BASE_FEATURE(kProjectorAccountSwitchNotification,
             "ProjectorAccountSwitchNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show promise icons during app installations.
BASE_FEATURE(kPromiseIcons, "PromiseIcons", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show promise icons during web app installations.
BASE_FEATURE(kPromiseIconsForWebApps,
             "PromiseIconsForWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the quick dim prototype is enabled.
BASE_FEATURE(kQuickDim, "QuickDim", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the smart reader feature is enabled.
BASE_FEATURE(kSmartReader, "SmartReader", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuickAppAccessTestUI,
             "QuickAppAccessTestUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables fingerprint quick unlock.
BASE_FEATURE(kQuickUnlockFingerprint,
             "QuickUnlockFingerprint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the PIN auto submit feature is enabled.
BASE_FEATURE(kQuickUnlockPinAutosubmit,
             "QuickUnlockPinAutosubmit",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables Release Notes suggestion chip on ChromeOS.
BASE_FEATURE(kReleaseNotesSuggestionChip,
             "ReleaseNotesSuggestionChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables rendering ARC notifications using ChromeOS notification framework
BASE_FEATURE(kRenderArcNotificationsByChrome,
             "RenderArcNotificationsByChrome",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the OS to unpin apps that were pinned by PinnedLauncherApps policy
// but are no longer a part of it from shelf under specific conditions.
BASE_FEATURE(kRemoveStalePolicyPinnedAppsFromShelf,
             "RemoveStalePolicyPinnedAppsFromShelf",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS scalable IPH.
BASE_FEATURE(kScalableIph, "ScalableIph", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables debug feature of ChromeOS Scalable Iph.
BASE_FEATURE(kScalableIphDebug,
             "ScalableIphDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the scanning app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kScanningAppJelly,
             "ScanningAppJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables screensaver customized running time.
BASE_FEATURE(kScreenSaverDuration,
             "ScreenSaverDuration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen feature in the personalization app.
BASE_FEATURE(kSeaPen, "SeaPen", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the system tray to show more information in larger screen.
BASE_FEATURE(kSeamlessRefreshRateSwitching,
             "SeamlessRefreshRateSwitching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
BASE_FEATURE(kSeparateNetworkIcons,
             "SeparateNetworkIcons",
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

// Stacks the hotseat app bar above the shelf button panels/system tray when
// there is not enough space for the app bar.
BASE_FEATURE(kShelfStackedHotseat,
             "ShelfStackedHotseat",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Shelf Palm Rejection in tablet mode by defining a pixel offset for
// the swipe gesture to show the extended hotseat. Limited to certain apps.
BASE_FEATURE(kShelfPalmRejectionSwipeOffset,
             "ShelfPalmRejectionSwipeOffset",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the OS update page in the shimless RMA flow.
BASE_FEATURE(kShimlessRMAOsUpdate,
             "ShimlessRMAOsUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables device compliance check in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMAComplianceCheck,
             "ShimlessRMAComplianceCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables 3p diagnostics in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMA3pDiagnostics,
             "ShimlessRMA3pDiagnostics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables 3p diagnostics dev mode in the Shimless RMA flow. This will skip some
// checks to allow developers to use dev-signed extensions for development
// purpose.
BASE_FEATURE(kShimlessRMA3pDiagnosticsDevMode,
             "ShimlessRMA3pDiagnosticsDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the shortcut customization app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kShortcutCustomizationJelly,
             "ShortcutCustomizationJelly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, system shortcuts will utilize state machiens instead of
// keeping track of entire history of keys pressed.
BASE_FEATURE(kShortcutStateMachines,
             "ShortcutStateMachines",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables a toggle to enable Bluetooth debug logs.
BASE_FEATURE(kShowBluetoothDebugLogToggle,
             "ShowBluetoothDebugLogToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows live caption in the video conference tray.
BASE_FEATURE(kShowLiveCaptionInVideoConferenceTray,
             "ShowLiveCaptionInVideoConferenceTray",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shows the Play Store icon in Demo Mode.
BASE_FEATURE(kShowPlayInDemoMode,
             "ShowPlayInDemoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Suppresses the first end of life warning shown 180 days before the AUE date.
BASE_FEATURE(kSuppressFirstEolWarning,
             "SuppressFirstEolWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow the system to suspend to disk via hibernate.
BASE_FEATURE(kSuspendToDisk,
             "CrOSSuspendToDisk",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kHibernateAfterTimeHours{
    &kSuspendToDisk, "HibernateAfterTimeHours", 8};

// Allow the system to transition to S4 rather than S5.
BASE_FEATURE(kSuspendToDiskAllowS4,
             "CrOSSuspendToDiskAllowS4",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the shutdown confirmation bubble from the login shelf view.
BASE_FEATURE(kShutdownConfirmationBubble,
             "ShutdownConfirmationBubble",
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

// Replaces Smart Lock UI in lock screen password box with new UI similar to
// fingerprint auth. Adds Smart Lock to "Lock screen and sign-in" section of
// settings.
BASE_FEATURE(kSmartLockUIRevamp,
             "SmartLockUIRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the consumer and enterprise support for provisioning eSIM profiles
// using Subscription Manager Discovery Service (SM-DS). This flag is a no-op
// unless the SmdsSupportEuiccUpload and SmdsDbusMigration flags are enabled.
// These additional flags are required since the functionality they impelemnt is
// required by this flag, such as:
//  - Using the newly added RefreshSmdxProfiles API.
//  - Tracking when an eSIM profile has already been installed for a
//    policy-defined cellular network.
BASE_FEATURE(kSmdsSupport, "SmdsSupport", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables tracking when a policy-defined cellular network configured to use
// SM-DS has already been applied and an eSIM profile for the network was
// installed.
BASE_FEATURE(kSmdsSupportEuiccUpload,
             "SmdsSupportEuiccUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the usage of DBus APIs that improve the stability around performing
// SM-DS scans.
BASE_FEATURE(kSmdsDbusMigration,
             "SmdsDbusMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the snap group feature is enabled or not.
BASE_FEATURE(kSnapGroup, "SnapGroup", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to create the snap group automatically when two windows are
// snapped if true. Otherwise, the user has to explicitly lock the two windows
// when both are snapped via cliking on the lock button when hovering the mouse
// over the shared edge of the two snapped windows.
constexpr base::FeatureParam<bool> kAutomaticallyLockGroup{
    &kSnapGroup, "AutomaticLockGroup", true};

// Resets the speak-on-mute opt-in nudge relevant prefs to their default
// settings, so the nudge can be shown again for debugging purposes.
BASE_FEATURE(kSpeakOnMuteOptInNudgePrefsReset,
             "SpeakOnMuteOptInNudgePrefsReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables battery indicator for styluses in the palette tray
BASE_FEATURE(kStylusBatteryStatus,
             "StylusBatteryStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the System Extensions platform.
BASE_FEATURE(kSystemExtensions,
             "SystemExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the managed device health service System Extensions type.
BASE_FEATURE(kSystemExtensionsManagedDeviceHealthServices,
             "SystemExtensionsManagedDeviceHealthServices",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the ability to play sounds for system services.
BASE_FEATURE(kSystemSounds, "SystemSounds", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the UI to show tab cluster info.
BASE_FEATURE(kTabClusterUI, "TabClusterUI", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Telemetry Extension.
BASE_FEATURE(kTelemetryExtension,
             "TelemetryExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the alternative emulator for the Terminal app.
BASE_FEATURE(kTerminalAlternativeEmulator,
             "TerminalAlternativeEmulator",
             base::FEATURE_ENABLED_BY_DEFAULT);
//
// Enables Terminal System App to load from Downloads for developer testing.
// Only works in dev and canary channels.
BASE_FEATURE(kTerminalDev, "TerminalDev", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables time of day screen saver.
BASE_FEATURE(kTimeOfDayScreenSaver,
             "TimeOfDayScreenSaver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables time of day wallpaper.
BASE_FEATURE(kTimeOfDayWallpaper,
             "TimeOfDayWallpaper",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables retrieving time of day screen saver assets from DLC, rather than from
// rootfs.
BASE_FEATURE(kTimeOfDayDlc, "TimeOfDayDlc", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the TrafficCountersHandler class to auto-reset traffic counters
// and shows Data Usage in the Celluar Settings UI.
BASE_FEATURE(kTrafficCountersEnabled,
             "TrafficCountersEnabled",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Android staging SM-DS server when fetching pending eSIM profiles.
BASE_FEATURE(kUseAndroidStagingSmds,
             "UseAndroidStagingSmds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses new AuthFactor-based API when communicating with cryptohome.
// This feature flag also affects usage of AuthSessions in QuickUnlock, but
// only in case when cryptohome is used as backend.
// This feature flag also affects usage of AuthSession on lock screen.
BASE_FEATURE(kUseAuthFactors,
             "UseAuthFactors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Uses AuthSessionStorage signleton class instead of Profile-keyed
// QuickUnlockStorage to store authenticated UserContext.
BASE_FEATURE(kUseAuthSessionStorage,
             "UseAuthSessionStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the login shelf view is placed in its own widget instead of
// sharing the shelf widget with other components.
BASE_FEATURE(kUseLoginShelfWidget,
             "UseLoginShelfWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
BASE_FEATURE(kUseMessagesStagingUrl,
             "UseMessagesStagingUrl",
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

// Controls whether the video conference feature is enabled.
BASE_FEATURE(kVideoConference,
             "VideoConference",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Restricts the video conference feature to the intended
// target population,
BASE_FEATURE(kFeatureManagementVideoConference,
             "FeatureManagementVideoConference",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the vc background replace is enabled.
BASE_FEATURE(kVcBackgroundReplace,
             "VCBackgroundReplace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is only used as a way to disable portrait relighting.
BASE_FEATURE(kVcPortraitRelight,
             "VcPortraitRelight",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This is only used as a way to disable stopAllScreenShare.
BASE_FEATURE(kVcStopAllScreenShare,
             "VcStopAllScreenShare",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable the fake effects for ChromeOS video conferencing controls
// UI. Only meaningful in the emulator.
BASE_FEATURE(kVcControlsUiFakeEffects,
             "VcControlsUiFakeEffects",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative segmentation models for ChromeOS video
// conferencing blur or relighting.
BASE_FEATURE(kVcSegmentationModel,
             "VCSegmentationModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative light intensity for ChromeOS video
// conferencing relighting.
BASE_FEATURE(kVcLightIntensity,
             "VCLightIntensity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables web API support for ChromeOS video conferencing.
BASE_FEATURE(kVcWebApi, "VcWebApi", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable round corners for virtual keyboard on ChromeOS.
BASE_FEATURE(kVirtualKeyboardRoundCorners,
             "VirtualKeyboardRoundCorners",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable Nacl for virtual keyboard on ChromeOS.
BASE_FEATURE(kVirtualKeyboardRemoveNacl,
             "VirtualKeyboardRemoveNacl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a per-boot host GPU cache generation for VMs. On default, the cache
// is generated per OS version.
BASE_FEATURE(kVmPerBootShaderCache,
             "VmPerBootShaderCache",
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

// Enable different wallpapers per desk.
BASE_FEATURE(kWallpaperPerDesk,
             "WallpaperPerDesk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable daily wallpaper refresh revamp.
BASE_FEATURE(kWallpaperRefreshRevamp,
             "WallpaperRefreshRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Welcome Tour that walks new users through ChromeOS System UI.
BASE_FEATURE(kWelcomeTour, "WelcomeTour", base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Welcome Tour that walks new users through ChromeOS System UI is
// enabled counterfactually as part of an experiment arm.
const base::FeatureParam<bool> kWelcomeTourEnabledCounterfactually{
    &kWelcomeTour, "is-counterfactual", false};

// Forces user eligibility for the Welcome Tour that walks new users through
// ChromeOS System UI. Enabling this flag has no effect unless `kWelcomeTour` is
// also enabled.
BASE_FEATURE(kWelcomeTourForceUserEligibility,
             "WelcomeTourForceUserEligibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable MAC Address Randomization on WiFi connection.
BASE_FEATURE(kWifiConnectMacAddressRandomization,
             "WifiConnectMacAddressRandomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables or disables Fresnel Device Active reporting on ChromeOS.
BASE_FEATURE(kDeviceActiveClient,
             "DeviceActiveClient",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckIn for the 28 day active device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClient28DayActiveCheckIn,
             "DeviceActiveClient28DayActiveCheckIn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for 28 day device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClient28DayActiveCheckMembership,
             "DeviceActiveClient28DayActiveCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for daily device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientDailyCheckMembership,
             "DeviceActiveClientDailyCheckMembership",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckIn for the churn cohort device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnCohortCheckIn,
             "DeviceActiveClientChurnCohortCheckIn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn cohort device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnCohortCheckMembership,
             "DeviceActiveClientChurnCohortCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckIn for the churn observation device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnObservationCheckIn,
             "DeviceActiveClientChurnObservationCheckIn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn observation
// device active pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnObservationCheckMembership,
             "DeviceActiveClientChurnObservationCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables forced reboots when DeviceScheduledReboot policy is set.
BASE_FEATURE(kDeviceForceScheduledReboot,
             "DeviceForceScheduledReboot",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum delay added to reboot time when DeviceScheduledReboot policy is set.
const base::FeatureParam<int> kDeviceForceScheduledRebootMaxDelay{
    &kDeviceForceScheduledReboot, "max-delay-in-seconds", 120};

// Enables settings to be split per device.
BASE_FEATURE(kInputDeviceSettingsSplit,
             "InputDeviceSettingsSplit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables bug fix for dead keys on the Terminal app.
// When enabled, dead keys correctly emit the 'Dead' event on key down for the
// Terminal.
BASE_FEATURE(kInputMethodDeadKeyFixForTerminal,
             "InputMethodDeadKeyFixForTerminal",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables whether to store UMA logs per-user and whether metrics
// consent is per-user.
BASE_FEATURE(kPerUserMetrics,
             "PerUserMetricsConsent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows Files App to find and execute tasks using App Service for ARC apps.
BASE_FEATURE(kArcFileTasksUseAppService,
             "ArcFileTasksUseAppService",
             base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////

bool AreContextualNudgesEnabled() {
  if (!IsHideShelfControlsInTabletModeEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kContextualNudges);
}

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool AreHelpAppWelcomeTipsEnabled() {
  return base::FeatureList::IsEnabled(kHelpAppWelcomeTips) &&
         base::FeatureList::IsEnabled(kScalableIph);
}

bool ArePolicyProvidedTrustAnchorsAllowedAtLockScreen() {
  return base::FeatureList::IsEnabled(
      kPolicyProvidedTrustAnchorsAllowedAtLockScreen);
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
  return IsNotifierCollisionEnabled() ||
         base::FeatureList::IsEnabled(kSideAlignedToasts);
}

bool AreSystemSoundsEnabled() {
  return base::FeatureList::IsEnabled(kSystemSounds);
}

bool IsAudioHFPNbsWarningEnabled() {
  return base::FeatureList::IsEnabled(kAudioHFPNbsWarning);
}

bool IsAudioHFPMicSRToggleEnabled() {
  return base::FeatureList::IsEnabled(kAudioHFPMicSRToggle);
}

bool IsAutoEnrollmentKioskInOobeEnabled() {
  return base::FeatureList::IsEnabled(kAutoEnrollmentKioskInOobe);
}

bool Is16DesksEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagement16Desks);
}

bool IsUpdateNotificationEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementUpdateNotification) &&
         base::FeatureList::IsEnabled(kUpdateNotification);
}

bool IsAdaptiveChargingEnabled() {
  return base::FeatureList::IsEnabled(kAdaptiveCharging);
}

bool IsAdaptiveChargingForTestingEnabled() {
  return base::FeatureList::IsEnabled(kAdaptiveChargingForTesting);
}

bool IsAdjustSplitViewForVKEnabled() {
  return base::FeatureList::IsEnabled(kAdjustSplitViewForVK);
}

bool IsAdvancedDocumentScanAPIEnabled() {
  return base::FeatureList::IsEnabled(kAdvancedDocumentScanAPI);
}

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAllowScrollSettingsEnabled() {
  return IsInputDeviceSettingsSplitEnabled() &&
         base::FeatureList::IsEnabled(kAllowScrollSettings);
}

bool IsEapDefaultCasWithoutSubjectVerificationAllowed() {
  return base::FeatureList::IsEnabled(
      kAllowEapDefaultCasWithoutSubjectVerification);
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

bool IsAmbientModeThrottleAnimationEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeThrottleAnimation);
}

bool IsApnRevampEnabled() {
  return base::FeatureList::IsEnabled(kApnRevamp);
}

bool IsAppNotificationsPageEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppNotificationsPage);
}

bool IsArcFuseBoxFileSharingEnabled() {
  return base::FeatureList::IsEnabled(kArcFuseBoxFileSharing);
}

bool IsArcInputOverlayBetaEnabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlayBeta) ||
         base::FeatureList::IsEnabled(kDropdownPanel);
}

bool IsArcInputOverlayAlphaV2Enabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlayAlphaV2);
}

bool IsAssistantNativeIconsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantNativeIcons);
}

bool IsAsynchronousScannerDiscoveryEnabled() {
  return base::FeatureList::IsEnabled(kAsynchronousScannerDiscovery);
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

bool IsBluetoothQualityReportEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothQualityReport);
}

bool IsCalendarJellyEnabled() {
  return base::FeatureList::IsEnabled(kCalendarJelly);
}

bool IsCaptureModeAudioMixingEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeAudioMixing);
}

bool IsCaptivePortalErrorPageEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalErrorPage);
}

bool IsCaptureModeTourEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeTour);
}

bool IsCellularCarrierLockEnabled() {
  return base::FeatureList::IsEnabled(kCellularCarrierLock);
}

bool IsCheckPasswordsAgainstCryptohomeHelperEnabled() {
  return base::FeatureList::IsEnabled(kCheckPasswordsAgainstCryptohomeHelper);
}

bool IsClipboardHistoryFooterEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryFooter);
}

bool IsClipboardHistoryLongpressEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryLongpress);
}

bool IsClipboardHistoryUrlTitlesEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryUrlTitles) &&
         chromeos::features::IsClipboardHistoryRefreshEnabled();
}

bool IsClipboardHistoryWebContentsPasteEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryWebContentsPaste);
}

bool IsContinuousOverviewScrollAnimationEnabled() {
  return base::FeatureList::IsEnabled(kContinuousOverviewScrollAnimation) &&
         chromeos::features::IsJellyEnabled();
}

bool IsCryptauthAttestationSyncingEnabled() {
  return base::FeatureList::IsEnabled(kCryptauthAttestationSyncing);
}

bool IsDeprecateOldKeyboardShortcutsAcceleratorEnabled() {
  return base::FeatureList::IsEnabled(
      kDeprecateOldKeyboardShortcutsAccelerator);
}

bool IsDnsOverHttpsWithIdentifiersReuseOldPolicyEnabled() {
  return base::FeatureList::IsEnabled(
      kDnsOverHttpsWithIdentifiersReuseOldPolicy);
}

bool IsDnsOverHttpsWithIdentifiersEnabled() {
  return base::FeatureList::IsEnabled(kDnsOverHttpsWithIdentifiers);
}

bool IsConsumerAutoUpdateToggleAllowed() {
  return base::FeatureList::IsEnabled(kConsumerAutoUpdateToggleAllowed);
}

bool IsCrosPrivacyHubAppPermissionsEnabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubAppPermissions) &&
         IsCrosPrivacyHubV0Enabled();
}

bool IsCrosPrivacyHubEnabled() {
  return IsCrosPrivacyHubAppPermissionsEnabled() ||
         IsCrosPrivacyHubLocationEnabled() || IsCrosPrivacyHubV0Enabled();
}

bool IsCrosPrivacyHubLocationEnabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHub) &&
         IsCrosPrivacyHubV0Enabled();
}

bool IsCrosPrivacyHubV0Enabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubV0) ||
         IsVideoConferenceEnabled();
}

bool IsCryptohomeRecoveryEnabled() {
  return base::FeatureList::IsEnabled(kCryptohomeRecovery);
}

bool IsDeskButtonEnabled() {
  return base::FeatureList::IsEnabled(kDeskButton);
}

bool IsDeskTemplateSyncEnabled() {
  return base::FeatureList::IsEnabled(kDeskTemplateSync);
}

bool IsInputDeviceSettingsSplitEnabled() {
  return base::FeatureList::IsEnabled(kInputDeviceSettingsSplit);
}

bool IsPeripheralCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kPeripheralCustomization) &&
         IsInputDeviceSettingsSplitEnabled();
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

bool IsInlineSyncStatusEnabled() {
  return base::FeatureList::IsEnabled(kFilesInlineSyncStatus);
}

bool IsInlineSyncStatusProgressEventsEnabled() {
  return base::FeatureList::IsEnabled(kFilesInlineSyncStatus) &&
         base::FeatureList::IsEnabled(kFilesInlineSyncStatusProgressEvents);
}

bool IsInlineSyncStatusOldEventsEnabled() {
  return base::FeatureList::IsEnabled(kFilesInlineSyncStatus) &&
         !base::FeatureList::IsEnabled(kFilesInlineSyncStatusProgressEvents);
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

bool IsExternalKeyboardInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableExternalKeyboardsInDiagnostics);
}

bool IsFaceMLSwaEnabled() {
  return base::FeatureList::IsEnabled(kFaceMLApp);
}

bool IsFamilyLinkOnSchoolDeviceEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOnSchoolDevice);
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

bool IsFastPairSavedDevicesNicknamesEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevicesNicknames);
}

bool IsFastPairLowPowerEnabled() {
  return base::FeatureList::IsEnabled(kFastPairLowPower);
}

bool IsFastPairPwaCompanionEnabled() {
  return base::FeatureList::IsEnabled(kFastPairPwaCompanion);
}

bool IsFastPairPreventNotificationsForRecentlyLostDeviceEnabled() {
  return base::FeatureList::IsEnabled(
      kFastPairPreventNotificationsForRecentlyLostDevice);
}

bool IsFastPairSoftwareScanningEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSoftwareScanning);
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

bool IsFilesSearchV2Enabled() {
  return base::FeatureList::IsEnabled(kFilesSearchV2);
}

bool IsFloatingWorkspaceEnabled() {
  return base::FeatureList::IsEnabled(kFloatingWorkspace);
}

bool IsFloatingWorkspaceV2Enabled() {
  return base::FeatureList::IsEnabled(kFloatingWorkspaceV2);
}

bool IsFocusModeEnabled() {
  return base::FeatureList::IsEnabled(kFocusMode);
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

bool IsFullscreenAfterUnlockAllowed() {
  return base::FeatureList::IsEnabled(kFullscreenAfterUnlockAllowed);
}

bool IsFullscreenAlertBubbleEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenAlertBubble);
}

bool IsBlockFwupdClientEnabled() {
  return base::FeatureList::IsEnabled(kBlockFwupdClient);
}

bool IsGaiaReauthEndpointEnabled() {
  return base::FeatureList::IsEnabled(kGaiaReauthEndpoint);
}

bool IsGalleryAppPdfEditNotificationEnabled() {
  return base::FeatureList::IsEnabled(kGalleryAppPdfEditNotification);
}

bool IsGifRecordingEnabled() {
  return base::FeatureList::IsEnabled(kGifRecording);
}

bool IsGifRenderingEnabled() {
  return base::FeatureList::IsEnabled(kGifRendering);
}

bool AreGlanceablesV2Enabled() {
  return base::FeatureList::IsEnabled(kGlanceablesV2);
}

bool AreGlanceablesV2EnabledForTrustedTesters() {
  return base::FeatureList::IsEnabled(kGlanceablesV2TrustedTesters);
}

bool IsGlanceablesV2ClassroomTeacherViewEnabled() {
  return base::FeatureList::IsEnabled(kGlanceablesV2ClassroomTeacherView);
}

bool IsGlanceablesV2ErrorMessageEnabled() {
  return base::FeatureList::IsEnabled(kGlanceablesV2ErrorMessage);
}

bool IsHibernateEnabled() {
  return base::FeatureList::IsEnabled(kHibernate);
}

bool IsHideArcMediaNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kHideArcMediaNotifications);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode);
}

bool IsHoldingSpaceCameraAppIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceCameraAppIntegration);
}

bool IsHoldingSpacePredictabilityEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpacePredictability);
}

bool IsHoldingSpaceRefreshEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceRefresh);
}

bool IsHoldingSpaceSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceSuggestions);
}

bool IsHoldingSpaceTourDropToPinEnabled() {
  return IsHoldingSpaceTourEnabled() && kHoldingSpaceTourDropToPinEnabled.Get();
}

bool IsHoldingSpaceTourEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceTour);
}

bool IsHoldingSpaceTourEnabledCounterfactually() {
  return IsHoldingSpaceTourEnabled() &&
         kHoldingSpaceTourEnabledCounterfactually.Get();
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

bool IsHotspotEnabled() {
  return base::FeatureList::IsEnabled(kHotspot);
}

bool IsScreenSaverDurationEnabled() {
  return base::FeatureList::IsEnabled(kScreenSaverDuration);
}

bool IsSnoopingProtectionEnabled() {
  return base::FeatureList::IsEnabled(kSnoopingProtection) &&
         switches::HasHps();
}

bool IsStartAssistantAudioDecoderOnDemandEnabled() {
  return base::FeatureList::IsEnabled(kStartAssistantAudioDecoderOnDemand);
}

bool IsImeTrayHideVoiceButtonEnabled() {
  return base::FeatureList::IsEnabled(kImeTrayHideVoiceButton);
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
      kInternalServerSideSpeechRecognitionByFinch);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsIppClientInfoEnabled() {
  return base::FeatureList::IsEnabled(kIppClientInfo);
}

bool IsJellyEnabledForDiagnosticsApp() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kDiagnosticsAppJelly);
}

bool IsJellyEnabledForFirmwareUpdate() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kFirmwareUpdateJelly);
}

bool IsJellyEnabledForOsFeedback() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kOsFeedbackJelly);
}

bool IsJellyEnabledForPrintManagement() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kPrintManagementJelly);
}

bool IsJellyEnabledForScanningApp() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kScanningAppJelly);
}

bool IsJellyEnabledForShortcutCustomization() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kShortcutCustomizationJelly);
}

bool IsKerberosRememberPasswordByDefaultEnabled() {
  return base::FeatureList::IsEnabled(kKerberosRememberPasswordByDefault);
}

bool IsKeyboardBacklightToggleEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardBacklightToggle);
}

bool IsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(kHandwritingLegacyRecognition);
}

bool IsLanguagePacksInOobeEnabled() {
  return base::FeatureList::IsEnabled(kLanguagePacksInOobe);
}

bool IsLauncherNudgeShortIntervalEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeShortInterval);
}

bool IsLauncherNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeSessionReset);
}

bool IsLauncherSearchControlEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSearchControl);
}

bool IsLicensePackagedOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kLicensePackagedOobeFlow);
}

bool IsLinkCrossDeviceDogfoodFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceDogfoodFeedback);
}

bool IsLinkCrossDeviceInternalsEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceInternals);
}

bool AreLocalPasswordsEnabledForConsumers() {
  return base::FeatureList::IsEnabled(kLocalPasswordForConsumers);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsEducationEnrollmentOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kEducationEnrollmentOobeFlow);
}

bool IsGameDashboardEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboard) ||
         base::FeatureList::IsEnabled(kDropdownPanel);
}

bool IsLockScreenInlineReplyEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenInlineReply);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsLoginScreenDeviceTrustConnectorFeatureEnabled() {
  return base::FeatureList::IsEnabled(kLoginScreenDeviceTrustConnectorEnabled);
}

bool IsProductivityLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kProductivityLauncherImageSearch);
}

bool IsMacAddressRandomizationEnabled() {
  return base::FeatureList::IsEnabled(kMacAddressRandomization);
}

bool IsMicMuteNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kMicMuteNotifications);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
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

bool IsNotifierCollisionEnabled() {
  return base::FeatureList::IsEnabled(kNotifierCollision);
}

bool IsOAuthIppEnabled() {
  return base::FeatureList::IsEnabled(kEnableOAuthIpp);
}

bool IsNewLockScreenReauthLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewLockScreenReauthLayout);
}

bool IsSuppressTextMessagesEnabled() {
  return base::FeatureList::IsEnabled(kSuppressTextMessages);
}

bool IsSysUiDownloadsIntegrationV2Enabled() {
  return base::FeatureList::IsEnabled(kSysUiDownloadsIntegrationV2);
}

bool IsSystemNudgeV2Enabled() {
  return base::FeatureList::IsEnabled(kSystemNudgeV2) ||
         IsScalableIphEnabled() ||
         IsVideoConferenceEnabled();  // System Nudge V2 is launching
                                      // together with the VC project.
}

bool IsNotificationExpansionAnimationEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExpansionAnimation);
}

bool IsNotificationExperimentalShortTimeoutsEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExperimentalShortTimeouts);
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

bool IsOobeChromeVoxHintEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeChromeVoxHint);
}

bool IsOobeGaiaInfoScreenEnabled() {
  return base::FeatureList::IsEnabled(kOobeGaiaInfoScreen);
}

bool IsOobeHidDetectionRevampEnabled() {
  return base::FeatureList::IsEnabled(kOobeHidDetectionRevamp);
}

bool IsKioskEnrollmentInOobeEnabled() {
  return base::FeatureList::IsEnabled(kEnableKioskEnrollmentInOobe);
}

bool IsKioskLoginScreenEnabled() {
  return base::FeatureList::IsEnabled(kEnableKioskLoginScreen);
}

bool IsOobeJellyEnabled() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kOobeJelly);
}

bool IsOobeJellyModalEnabled() {
  return IsOobeJellyEnabled() && base::FeatureList::IsEnabled(kOobeJellyModal);
}

bool IsOobeSimonEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementOobeSimon) &&
         base::FeatureList::IsEnabled(kOobeSimon);
}

bool IsOobeSkipAssistantEnabled() {
  return base::FeatureList::IsEnabled(kOobeSkipAssistant);
}

bool IsOobeChoobeEnabled() {
  return base::FeatureList::IsEnabled(kOobeChoobe);
}

bool IsOobeSoftwareUpdateEnabled() {
  return base::FeatureList::IsEnabled(kOobeSoftwareUpdate);
}

bool IsOobeLazyLoadingEnabled() {
  return base::FeatureList::IsEnabled(kOobeLazyLoading);
}

bool IsOobeQuickStartEnabled() {
  return base::FeatureList::IsEnabled(kOobeQuickStart);
}

bool IsOobeTouchpadScrollEnabled() {
  return IsOobeChoobeEnabled() &&
         base::FeatureList::IsEnabled(kOobeTouchpadScroll);
}

bool IsOobeDisplaySizeEnabled() {
  return IsOobeChoobeEnabled() &&
         base::FeatureList::IsEnabled(kOobeDisplaySize);
}

bool IsOsFeedbackDialogEnabled() {
  return base::FeatureList::IsEnabled(kOsFeedbackDialog);
}

bool IsOsSettingsAppBadgingToggleEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppBadgingToggle);
}

bool IsOsSettingsDeprecateSyncMetricsToggleEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsDeprecateSyncMetricsToggle);
}

bool IsOsSettingsRevampWayfindingEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsRevampWayfinding);
}

bool IsOsSettingsTestChromeRefresh() {
  return base::FeatureList::IsEnabled(kOsSettingsTestChromeRefresh);
}

bool IsOsSyncConsentRevampEnabled() {
  return base::FeatureList::IsEnabled(kOsSyncConsentRevamp);
}

bool IsOverviewDeskNavigationEnabled() {
  return base::FeatureList::IsEnabled(kOverviewDeskNavigation);
}

bool IsPasspointARCSupportEnabled() {
  return base::FeatureList::IsEnabled(kPasspointARCSupport);
}

bool IsPasspointSettingsEnabled() {
  return base::FeatureList::IsEnabled(kPasspointSettings) &&
         base::FeatureList::IsEnabled(kPasspointARCSupport);
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

bool IsPersonalizationJellyEnabled() {
  return chromeos::features::IsJellyEnabled();
}

bool IsPhoneHubCameraRollEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCameraRoll);
}

bool IsPhoneHubMonochromeNotificationIconsEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubMonochromeNotificationIcons);
}

bool IsPhoneHubOnboardingNotifierRevampEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubOnboardingNotifierRevamp) &&
         base::FeatureList::IsEnabled(kSystemNudgeV2);
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

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsPinAutosubmitFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmit);
}

bool IsPipDoubleTapToResizeEnabled() {
  return base::FeatureList::IsEnabled(kPipDoubleTapToResize);
}

bool IsPipPinchToResizeEnabled() {
  return base::FeatureList::IsEnabled(kPipPinchToResize);
}

bool IsPipTiltEnabled() {
  return base::FeatureList::IsEnabled(kPipTilt);
}

bool IsLocalPrinterObservingEnabled() {
  return base::FeatureList::IsEnabled(kLocalPrinterObserving);
}

bool IsPrinterSettingsPrinterStatusEnabled() {
  return base::FeatureList::IsEnabled(kPrinterSettingsPrinterStatus);
}

bool IsPrinterSettingsRevampEnabled() {
  return base::FeatureList::IsEnabled(kPrinterSettingsRevamp);
}

bool IsPrintPreviewDiscoveredPrintersEnabled() {
  return base::FeatureList::IsEnabled(kPrintPreviewDiscoveredPrinters);
}

bool IsPrivacyIndicatorsEnabled() {
  // Privacy indicators should not be enabled when video conference is enabled.
  return base::FeatureList::IsEnabled(kPrivacyIndicators) &&
         !IsVideoConferenceEnabled();
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

bool IsQsRevampEnabled() {
  return base::FeatureList::IsEnabled(kQsRevamp);
}

bool IsProjectorAccountSwitchNotificationEnabled() {
  return base::FeatureList::IsEnabled(kProjectorAccountSwitchNotification);
}

bool IsProjectorServerSideRecognitionFallbackImplEnabled() {
  return base::FeatureList::IsEnabled(
      kProjectorServerSideRecognitionFallbackImpl);
}

bool IsProjectorMutingEnabled() {
  return base::FeatureList::IsEnabled(kProjectorMuting);
}

bool IsProjectorTranscriptChapterTitleEnabled() {
  return base::FeatureList::IsEnabled(kProjectorTranscriptChapterTitle);
}

bool IsQuickDimEnabled() {
  return base::FeatureList::IsEnabled(kQuickDim) && switches::HasHps();
}

bool IsPerDeskZOrderEnabled() {
  return base::FeatureList::IsEnabled(kEnablePerDeskZOrder);
}

bool IsRenderArcNotificationsByChromeEnabled() {
  return base::FeatureList::IsEnabled(kRenderArcNotificationsByChrome);
}

bool IsRemoveStalePolicyPinnedAppsFromShelfEnabled() {
  return base::FeatureList::IsEnabled(kRemoveStalePolicyPinnedAppsFromShelf);
}

bool IsSameAppWindowCycleEnabled() {
  return base::FeatureList::IsEnabled(kSameAppWindowCycle);
}

bool IsSamlNotificationOnPasswordChangeSuccessEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableSamlNotificationOnPasswordChangeSuccess);
}

bool IsScalableIphEnabled() {
  return base::FeatureList::IsEnabled(kScalableIph);
}

bool IsScalableIphDebugEnabled() {
  return base::FeatureList::IsEnabled(kScalableIphDebug);
}

bool IsSeaPenEnabled() {
  return base::FeatureList::IsEnabled(kSeaPen);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
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

bool IsShelfPalmRejectionSwipeOffsetEnabled() {
  return base::FeatureList::IsEnabled(kShelfPalmRejectionSwipeOffset);
}

bool IsShelfStackedHotseatEnabled() {
  return base::FeatureList::IsEnabled(kShelfStackedHotseat);
}

bool IsShimlessRMAOsUpdateEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAOsUpdate);
}

bool IsShimlessRMAComplianceCheckEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAComplianceCheck);
}

bool IsShimlessRMA3pDiagnosticsEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMA3pDiagnostics);
}

bool IsShimlessRMA3pDiagnosticsDevModeEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMA3pDiagnosticsDevMode);
}

bool IsSmdsSupportEnabled() {
  return base::FeatureList::IsEnabled(kSmdsDbusMigration) &&
         base::FeatureList::IsEnabled(kSmdsSupport) &&
         base::FeatureList::IsEnabled(kSmdsSupportEuiccUpload);
}

bool IsSmdsSupportEuiccUploadEnabled() {
  return base::FeatureList::IsEnabled(kSmdsSupportEuiccUpload);
}

bool IsSmdsDbusMigrationEnabled() {
  return base::FeatureList::IsEnabled(kSmdsDbusMigration);
}

bool IsSmartReaderEnabled() {
  return base::FeatureList::IsEnabled(kSmartReader);
}

bool IsSnapGroupEnabled() {
  return base::FeatureList::IsEnabled(kSnapGroup);
}

bool IsSpeakOnMuteOptInNudgePrefsResetEnabled() {
  return base::FeatureList::IsEnabled(kSpeakOnMuteOptInNudgePrefsReset);
}

bool IsSystemLiveCaptionEnabled() {
  // TODO(b/295244553): Once `kSystemLiveCaption` is enabled by default, switch
  // to `&&`.
  return base::FeatureList::IsEnabled(kSystemLiveCaption) ||
         base::FeatureList::IsEnabled(kFeatureManagementSystemLiveCaption);
}

bool IsSystemTrayShadowEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayShadow);
}

bool IsStylusBatteryStatusEnabled() {
  return base::FeatureList::IsEnabled(kStylusBatteryStatus);
}

bool IsTimeOfDayScreenSaverEnabled() {
  return base::FeatureList::IsEnabled(kTimeOfDayScreenSaver) &&
         base::FeatureList::IsEnabled(kFeatureManagementTimeOfDayScreenSaver) &&
         IsTimeOfDayWallpaperEnabled();
}

bool IsTimeOfDayWallpaperEnabled() {
  return base::FeatureList::IsEnabled(kTimeOfDayWallpaper) &&
         base::FeatureList::IsEnabled(kFeatureManagementTimeOfDayWallpaper);
}

bool IsTimeOfDayDlcEnabled() {
  return IsTimeOfDayScreenSaverEnabled() &&
         base::FeatureList::IsEnabled(kTimeOfDayDlc);
}

bool IsTabClusterUIEnabled() {
  return base::FeatureList::IsEnabled(kTabClusterUI);
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

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsUnmanagedDeviceDeviceTrustConnectorFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      kUnmanagedDeviceDeviceTrustConnectorEnabled);
}

bool IsUseLoginShelfWidgetEnabled() {
  return base::FeatureList::IsEnabled(kUseLoginShelfWidget);
}

bool ShouldUseAndroidStagingSmds() {
  return base::FeatureList::IsEnabled(kUseAndroidStagingSmds);
}

bool ShouldUseStorkSmds() {
  return base::FeatureList::IsEnabled(kUseStorkSmdsServerAddress);
}

bool ShouldUseAuthSessionStorage() {
  return base::FeatureList::IsEnabled(kUseAuthSessionStorage);
}

bool IsUserEducationEnabled() {
  return IsCaptureModeTourEnabled() || IsHoldingSpaceTourEnabled() ||
         IsWelcomeTourEnabled();
}

bool IsVideoConferenceEnabled() {
  return (base::FeatureList::IsEnabled(kVideoConference) ||
          base::FeatureList::IsEnabled(kFeatureManagementVideoConference)) &&
         base::FeatureList::IsEnabled(kCameraEffectsSupportedByHardware);
}

bool IsStopAllScreenShareEnabled() {
  return base::FeatureList::IsEnabled(kVcStopAllScreenShare) &&
         IsVideoConferenceEnabled();
}

bool IsVcBackgroundReplaceEnabled() {
  return base::FeatureList::IsEnabled(kVcBackgroundReplace) &&
         IsVideoConferenceEnabled();
}

bool IsVcPortraitRelightEnabled() {
  return base::FeatureList::IsEnabled(kVcPortraitRelight) &&
         IsVideoConferenceEnabled();
}

bool IsVcControlsUiFakeEffectsEnabled() {
  return base::FeatureList::IsEnabled(kVcControlsUiFakeEffects);
}

bool IsVcWebApiEnabled() {
  return base::FeatureList::IsEnabled(kVcWebApi) && IsVideoConferenceEnabled();
}

bool IsWallpaperFastRefreshEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperFastRefresh);
}

bool IsWallpaperGooglePhotosSharedAlbumsEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperGooglePhotosSharedAlbums);
}

bool IsWallpaperPerDeskEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperPerDesk);
}

bool IsWallpaperRefreshRevampEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperRefreshRevamp);
}

bool IsWelcomeTourEnabled() {
  return base::FeatureList::IsEnabled(kWelcomeTour);
}

bool IsWelcomeTourEnabledCounterfactually() {
  return IsWelcomeTourEnabled() && kWelcomeTourEnabledCounterfactually.Get();
}

bool IsWelcomeTourForceUserEligibilityEnabled() {
  return IsWelcomeTourEnabled() &&
         base::FeatureList::IsEnabled(kWelcomeTourForceUserEligibility);
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

bool ShouldArcFileTasksUseAppService() {
  return base::FeatureList::IsEnabled(kArcFileTasksUseAppService);
}

bool ShouldOnlyShowNewShortcutApp() {
  return base::FeatureList::IsEnabled(kOnlyShowNewShortcutsApp);
}

bool isSearchCustomizableShortcutsInLauncherEnabled() {
  return base::FeatureList::IsEnabled(kSearchCustomizableShortcutsInLauncher);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool IsFeatureAwareDeviceDemoModeEnabled() {
  return base::FeatureList::IsEnabled(
      kFeatureManagementFeatureAwareDeviceDemoMode);
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(kCryptAuthV2DeviceSync);
}

}  // namespace ash::features
