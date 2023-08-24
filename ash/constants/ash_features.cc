// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
BASE_FEATURE(kInstantTetheringBackgroundAdvertisementSupport,
             "InstantTetheringBackgroundAdvertisementSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kTestImageRelease[] = "testimage-channel";

}  // namespace

// Enables the UI and logic that minimizes the amount of time the device spends
// at full battery. This preserves battery lifetime.
BASE_FEATURE(kAdaptiveCharging,
             "AdaptiveCharging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether adaptive charging feature is supported by the hardware. This is not
// finch or user configurable but a hardware attribute controlled by ChromeOS
// USE flag.
BASE_FEATURE(kAdaptiveChargingHardwareSupport,
             "AdaptiveChargingHardwareSupport",
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

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
BASE_FEATURE(kAllowAmbientEQ,
             "AllowAmbientEQ",
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
// mouse/touchpad.
BASE_FEATURE(kAllowScrollSettings,
             "AllowScrollSettings",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApnRevamp, "ApnRevamp", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppCollectionFolderRefresh,
             "AppCollectionFolderRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kAssistantNativeIcons,
             "AssistantNativeIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Peripheral volume change by hardware reported steps
BASE_FEATURE(kAudioPeripheralVolumeGranularity,
             "AudioPeripheralVolumeGranularity",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the AudioSourceFetcher resamples the audio for speech
// recongnition.
BASE_FEATURE(kAudioSourceFetcherResampling,
             "AudioSourceFetcherResampling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Audio Settings Page in System Settings, which allows
// audio configuration. crbug.com/1092970.
BASE_FEATURE(kAudioSettingsPage,
             "AudioSettingsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables extended autocomplete results.
BASE_FEATURE(kAutocompleteExtendedSuggestions,
             "AutocompleteExtendedSuggestions",
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

// If enabled, the autozoom nudge shown prefs will be reset at the start of
// each new user session.
BASE_FEATURE(kAutozoomNudgeSessionReset,
             "AutozoomNudgeSessionReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables loading avatar images from the cloud on ChromeOS.
BASE_FEATURE(kAvatarsCloudMigration,
             "AvatarsCloudMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enable storage ballooning for Borealis. This takes precedence over
// kBorealisDiskManagement.
BASE_FEATURE(kBorealisStorageBallooning,
             "BorealisStorageBallooning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable calendar jelly.
BASE_FEATURE(kCalendarJelly,
             "CalendarJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to allow time-lapse video recording in the camera app.
BASE_FEATURE(kCameraAppTimeLapse,
             "CameraAppTimeLapse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
BASE_FEATURE(kCameraPrivacySwitchNotifications,
             "CameraPrivacySwitchNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the capture mode demo tools feature is enabled for Capture
// Mode.
BASE_FEATURE(kCaptureModeDemoTools,
             "CaptureModeDemoTools",
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

// When enabled alongside the keyboard auto-repeat setting, holding down Ctrl+V
// will cause the clipboard history menu to show. From there, the user can
// select a clipboard history item to replace the initially pasted content.
BASE_FEATURE(kClipboardHistoryLongpress,
             "ClipboardHistoryLongpress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables updated UI for the clipboard history menu and new system behavior
// related to clipboard history. Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kClipboardHistoryRefresh,
             "ClipboardHistoryRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, pasting a clipboard history item will cause that item to move to
// the top of the history list.
BASE_FEATURE(kClipboardHistoryReorder,
             "ClipboardHistoryReorder",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled and account falls under the new deal, will be allowed to toggle
// auto updates.
BASE_FEATURE(kConsumerAutoUpdateToggleAllowed,
             "ConsumerAutoUpdateToggleAllowed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Privacy Hub for ChromeOS.
BASE_FEATURE(kCrosPrivacyHub,
             "CrosPrivacyHub",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Privacy Hub features selected for dogfooding.
BASE_FEATURE(kCrosPrivacyHubV0,
             "CrosPrivacyHubV0",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables future features for Privacy Hub for ChromeOS.
BASE_FEATURE(kCrosPrivacyHubV2,
             "CrosPrivacyHubV2",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Virtual Keyboard support.
BASE_FEATURE(kCrostiniVirtualKeyboardSupport,
             "CrostiniVirtualKeyboardSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables support for third party VMs.
BASE_FEATURE(kBruschetta, "Bruschetta", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables migration for third party VMs installed during alpha.
BASE_FEATURE(kBruschettaAlphaMigrate,
             "BruschettaAlphaMigrate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Captive Portal Error Page changes, which shows a suggestion in
// the Chrome error page on ChromeOS when behind a captive portal.
BASE_FEATURE(kCaptivePortalErrorPage,
             "CaptivePortalErrorPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables diacritics on longpress on the physical keyboard by default.
BASE_FEATURE(kDiacriticsOnPhysicalKeyboardLongpressDefaultOn,
             "DiacriticsOnPhysicalKeyboardLongpressDefaultOn",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables DriveFS' bulk pinning functionality.
BASE_FEATURE(kDriveFsBulkPinning,
             "DriveFsBulkPinning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partial support of CSE files on ChromeOS: users will be able to see
// the files and open in web apps, but not to open/read/write CSE files locally.
BASE_FEATURE(kDriveFsShowCSEFiles,
             "DriveFsShowCSEFiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables authenticating to Wi-Fi networks using EAP-GTC.
BASE_FEATURE(kEapGtcWifiAuthentication,
             "EapGtcWifiAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the System Web App (SWA) version of Eche.
BASE_FEATURE(kEcheSWA, "EcheSWA", base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, allows the creation of up to 16 desks (default is 8).
BASE_FEATURE(kEnable16Desks,
             "Enable16Desks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables background blur for the app list, shelf, unified system tray,
// autoclick menu, etc. Also enables the AppsGridView mask layer, slower devices
// may have choppier app list animations while in this mode. crbug.com/765292.
BASE_FEATURE(kEnableBackgroundBlur,
             "EnableBackgroundBlur",
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

// If enabled, the input device cards will be shown in the diagnostics app.
BASE_FEATURE(kEnableInputInDiagnosticsApp,
             "EnableInputInDiagnosticsApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the diagnostics app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kDiagnosticsAppJelly,
             "kDiagnosticsAppJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables skipping of network screen.
BASE_FEATURE(kEnableOobeNetworkScreenSkip,
             "EnableOobeNetworkScreenSkip",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, allows user to request to view PPD for a printer.
BASE_FEATURE(kEnableViewPpd, "EnableViewPpd", base::FEATURE_ENABLED_BY_DEFAULT);

// Enforces Ash extension keep-list. Only the extensions/Chrome apps in the
// keep-list are enabled in Ash.
BASE_FEATURE(kEnforceAshExtensionKeeplist,
             "EnforceAshExtensionKeeplist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enforces enrollment nudging for managed users during OOBE. This is a
// temporary flag for WIP feature to until we implement required DM server
// controls.
BASE_FEATURE(kEnrollmentNudgingForTesting,
             "EnrollmentNudgingForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to the chrome://enterprise-reporting WebUI.
BASE_FEATURE(kEnterpriseReportingUI,
             "EnterpriseReportingUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
BASE_FEATURE(kExoOrdinalMotion,
             "ExoOrdinalMotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to check KeyEvent flag to see if the event is consumed by IME
// or not (=decides using heuristics based on key code etc.).
BASE_FEATURE(kExoConsumedByImeByFlag,
             "ExoConsumedByImeByFlag",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kFastPair, "FastPair", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables using new Handshake retry logic for Fast Pair.
BASE_FEATURE(kFastPairHandshakeRefactor,
             "FastPairHandshakeRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enabled Fast Pair sub feature to prevent notifications for recently lost
// devices for |kFastPairDeviceLostNotificationTimeout|.
BASE_FEATURE(kFastPairPreventNotificationsForRecentlyLostDevice,
             "FastPairPreventNotificationsForRecentlyLostDevice",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the "Saved Devices" Fast Pair strict interpretation of opt-in status,
// meaning that a user's preferences determine if retroactive pairing and
// subsequent pairing scenarios are enabled.
BASE_FEATURE(kFastPairSavedDevicesStrictOptIn,
             "FastPairSavedDevicesStrictOptIn",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable inline sync status in Files app.
BASE_FEATURE(kFilesInlineSyncStatus,
             "FilesInlineSyncStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables V2 of search functionality in files.
BASE_FEATURE(kFilesSearchV2,
             "FilesSearchV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partitioning of removable disks in file manager.
BASE_FEATURE(kFilesSinglePartitionFormat,
             "FilesSinglePartitionFormat",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable files app trash.
BASE_FEATURE(kFilesTrash, "FilesTrash", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable files app trash for Drive.
BASE_FEATURE(kFilesTrashDrive,
             "FilesTrashDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the firmware update app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kFirmwareUpdateJelly,
             "FirmwareUpdateJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables first party Vietnamese input method.
BASE_FEATURE(kFirstPartyVietnameseInput,
             "FirstPartyVietnameseInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
        base::Seconds(15)};

// Time interval to capture current desk as desk template and upload template to
// server.
constexpr base::FeatureParam<base::TimeDelta>
    kFloatingWorkspaceV2PeriodicJobIntervalInSeconds{
        &kFloatingWorkspaceV2, "PeriodicJobIntervalInSeconds",
        base::Seconds(30)};

// If enabled, makes the Projector app use server side speech
// recognition instead of on-device speech recognition.
BASE_FEATURE(kForceEnableServerSideSpeechRecognitionForDev,
             "ForceEnableServerSideSpeechRecognitionForDev",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Drive to forcibly resync office files. Operations such as copy,
// move, ZIP on MS Office files call on the Drive to resync the files.
BASE_FEATURE(kForceReSyncDrive,
             "ForceReSyncDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable a notification to provide an option to open Gallery app for a
// downloaded pdf file.
BASE_FEATURE(kGalleryAppPdfEditNotification,
             "GalleryAppPdfEditNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Button label text used for the above kGalleryAppPdfEditNotification.
const base::FeatureParam<std::string> kGalleryAppPdfEditNotificationText{
    &kGalleryAppPdfEditNotification, "text", ""};

// Enable glanceables on login.
BASE_FEATURE(kGlanceables, "Glanceables", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables glanceables on time management surface.
BASE_FEATURE(kGlanceablesV2,
             "GlanceablesV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Gaia reauth endpoint.
BASE_FEATURE(kGaiaReauthEndpoint,
             "GaiaReauthEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the Game Dashboard.
BASE_FEATURE(kGameDashboard,
             "GameDashboard",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables downloading the handwriting libraries via DLC.
BASE_FEATURE(kHandwritingLibraryDlc,
             "HandwritingLibraryDlc",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ChromeOS Apps Discovery experience in the Help App.
BASE_FEATURE(kHelpAppAppsDiscovery,
             "HelpAppAppsDiscovery",
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

// Enable ChromeOS hibernation features.
BASE_FEATURE(kHibernate, "Hibernate", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables image search for productivity launcher.
BASE_FEATURE(kProductivityLauncherImageSearch,
             "ProductivityLauncherImageSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a privacy improvement that removes wrongly configured hidden
// networks and mitigates the creation of these networks. crbug/1327803.
BASE_FEATURE(kHiddenNetworkMigration,
             "HiddenNetworkMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables in-progress downloads notification suppression with the productivity
// feature that aims to reduce context switching by enabling users to collect
// content and transfer or access it later.
BASE_FEATURE(kHoldingSpaceInProgressDownloadsNotificationSuppression,
             "HoldingSpaceInProgressNotificationSuppression",
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

BASE_FEATURE(kHomeButtonQuickAppAccess,
             "HomeButtonQuickAppAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled use the updated US English IME language models.
BASE_FEATURE(kImeUsEnglishModelUpdate,
             "ImeUsEnglishModelUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Controls whether to show new improved UI for cryptohome errors that happened
// during login. UI contains links to help center and might provide actions
// that can be taken to resolve the problem.
BASE_FEATURE(kImprovedLoginErrorHandling,
             "ImprovedLoginErrorHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Instant Tethering on ChromeOS.
BASE_FEATURE(kInstantTethering,
             "InstantTethering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
BASE_FEATURE(kInternalServerSideSpeechRecognition,
             "InternalServerSideSpeechRecognition",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature overrides the `InternalServerSideSpeechRecognition` flag if disabled.
BASE_FEATURE(kInternalServerSideSpeechRecognitionControl,
             "InternalServerSideSpeechRecognitionControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending `client-info` values to IPP printers on ChromeOS.
BASE_FEATURE(kIppClientInfo, "IppClientInfo", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables correct handling of the function key row in Japanese.
BASE_FEATURE(kJapaneseFunctionRow,
             "JapaneseFunctionRow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables IME button in the floating accessibility menu for the Kiosk session.
BASE_FEATURE(kKioskEnableImeButton,
             "KioskEnableImeButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to use lacros-chrome as the only web browser on ChromeOS.
// This works only when both LacrosSupport and LacrosPrimary below are enabled.
// NOTE: Use crosapi::browser_util::IsAshWebBrowserEnabled() instead of checking
// the feature directly. Similar to LacrosSupport and LacrosPrimary,
// this may not be allowed depending on user types and/or policies.
BASE_FEATURE(kLacrosOnly, "LacrosOnly", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to use lacros-chrome as a primary web browser on ChromeOS.
// This works only when LacrosSupport below is enabled.
// NOTE: Use crosapi::browser_util::IsLacrosPrimary() instead of checking
// the feature directly. Similar to LacrosSupport, this may not be allowed
// depending on user types and/or policies.
BASE_FEATURE(kLacrosPrimary,
             "LacrosPrimary",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables "Linux and ChromeOS" support. Allows a Linux version of Chrome
// ("lacros-chrome") to run as a Wayland client with this instance of Chrome
// ("ash-chrome") acting as the Wayland server and window manager.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking the
// feature directly. Lacros is not allowed for certain user types and can be
// disabled by policy. These restrictions will be lifted when Lacros development
// is complete.
BASE_FEATURE(kLacrosSupport,
             "LacrosSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, wayland logging is enabled for Lacros.
BASE_FEATURE(kLacrosWaylandLogging,
             "LacrosWaylandLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Emergency switch to turn off profile migration.
BASE_FEATURE(kLacrosProfileMigrationForceOff,
             "LacrosProfileMigrationForceOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use `MoveMigrator` instead of `CopyMigrator` to migrate data.
// `MoveMigrator` moves data from ash to lacros instead of copying them.
BASE_FEATURE(kLacrosMoveProfileMigration,
             "LacrosMoveProfileMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, it is allowed to migrate data from lacros back to ash, provided
// that other conditions are also met (e.g. the policy is enabled, or the
// command line flag is passed).
BASE_FEATURE(kLacrosProfileBackwardMigration,
             "LacrosProfileBackwardMigration",
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

// Enables new flow for license packaged devices with enterprise license.
BASE_FEATURE(kLicensePackagedOobeFlow,
             "LicensePackagedOobeFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kMediaAppPdfSignature,
             "MediaAppPdfSignature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification of when a microphone-using app is launched while the
// microphone is muted.
BASE_FEATURE(kMicMuteNotifications,
             "MicMuteNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disables the deprecated Messages cross-device integration, to be used
// along side the flag preinstall-by-default (kMessagesPreinstall).
BASE_FEATURE(kDisableMessagesCrossDeviceIntegration,
             "DisableMessagesCrossDeviceIntegration",
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

// Enables multi-zone rgb keyboard customization.
BASE_FEATURE(kMultiZoneRgbKeyboard,
             "MultiZoneRgbKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for multilingual assistive typing on ChromeOS.
BASE_FEATURE(kMultilingualTyping,
             "MultilingualTyping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Nearby Connections to specificy KeepAlive interval and timeout while
// also making the Nearby Connections WebRTC defaults longer.
BASE_FEATURE(kNearbyKeepAliveFix,
             "NearbyKeepAliveFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether new Lockscreen reauth layout is shown or not.
BASE_FEATURE(kNewLockScreenReauthLayout,
             "NewLockScreenReauthLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kOobeChoobe, "OobeChoobe", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Drive Pinning Screen will be shown during
// the new user onboarding flow.
BASE_FEATURE(kOobeDrivePinning,
             "OobeDrivePinning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, TouchPadScreen will be shown in CHOOBE.
// enabling this without enabling OobeChoobe flag will have no effect
BASE_FEATURE(kOobeTouchpadScroll,
             "OobeTouchpadScrollDirection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOobeDisplaySize,
             "OobeDisplaySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the ChromeOS OOBE HID Detection Revamp, which updates
// the OOBE HID detection screen UI and related infrastructure. See
// https://crbug.com/1299099.
BASE_FEATURE(kOobeHidDetectionRevamp,
             "OobeHidDetectionRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE Jelly features.
BASE_FEATURE(kOobeJelly, "OobeJelly", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OOBE Simon features.
BASE_FEATURE(kOobeSimon, "OobeSimon", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Oobe quick start flow.
BASE_FEATURE(kOobeQuickStart,
             "OobeQuickStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnlyShowNewShortcutsApp,
             "OnlyShowNewShortcutsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchInShortcutsApp,
             "SearchInShortcutsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the feedback tool new UX on ChromeOS.
// This tool under development will be rolled out via Finch.
// Enabling this flag will use the new feedback tool instead of the current
// tool on CrOS.
BASE_FEATURE(kOsFeedback, "OsFeedback", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a new App Notifications subpage will appear in CrOS Apps section.
BASE_FEATURE(kOsSettingsAppNotificationsPage,
             "OsSettingsAppNotificationsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, OsSyncConsent Revamp will be shown.
// enabling this without enabling Lacros flag will have no effect
BASE_FEATURE(kOsSyncConsentRevamp,
             "OsSyncConsentRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the os feedback app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kOsFeedbackJelly,
             "OsFeedbackJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables app badging toggle to be displayed in app notification page in
// ChromeOS Settings.
BASE_FEATURE(kOsSettingsAppBadgingToggle,
             "OsSettingsAppBadgingToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables search result feedback in ChromeOS Settings when no search results
// are returned.
BASE_FEATURE(kOsSettingsSearchFeedback,
             "OsSettingsSearchFeedback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverviewButton,
             "OverviewButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables staying in overview when navigating between desks using a swipe
// gesture or keyboard shortcut.
BASE_FEATURE(kOverviewDeskNavigation,
             "OverviewDeskNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables user to provision PasspointARCSupport credentials.
BASE_FEATURE(kPasspointARCSupport,
             "PasspointARCSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables user to display Passpoint credentials in the UI.
BASE_FEATURE(kPasspointSettings,
             "PasspointSettings",
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

// Enables Jelly features in Personalization App.
BASE_FEATURE(kPersonalizationJelly,
             "PersonalizationJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within ChromeOS.
BASE_FEATURE(kPhoneHub, "PhoneHub", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Camera Roll feature in Phone Hub, which allows users to access
// recent photos and videos taken on a connected Android device
BASE_FEATURE(kPhoneHubCameraRoll,
             "PhoneHubCameraRoll",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Determine whether system nudge will be shown on user eligible for Phone Hub
// instead of multidevice notification.
BASE_FEATURE(kPhoneHubNudge,
             "PhoneHubNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubPingOnBubbleOpen,
             "PhoneHubPingOnBubbleOpen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of seconds to wait for ping response before disconnecting
const base::FeatureParam<base::TimeDelta> kPhoneHubPingTimeout{
    &kPhoneHubPingOnBubbleOpen, "PhoneHubPingTimeout", base::Seconds(60)};

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

// If enabled, the jelly colors will be used in the print management app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kPrintManagementJelly,
             "PrintManagementJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the new OS Printer Settings UI.
BASE_FEATURE(kPrinterSettingsRevamp,
             "PrinterSettingsRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to allocate more video capture buffers.
BASE_FEATURE(kMoreVideoCaptureBuffers,
             "MoreVideoCaptureBuffers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing notification and status area indicators when an app is
// using camera/microphone.
BASE_FEATURE(kPrivacyIndicators,
             "PrivacyIndicators",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a bubble-based launcher in clamshell mode. Changes the suggestions
// that appear in the launcher in both clamshell and tablet modes. Removes pages
// from the apps grid. This feature was previously named "AppListBubble".
// https://crbug.com/1204551
BASE_FEATURE(kProductivityLauncher,
             "ProductivityLauncher",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Projector.
BASE_FEATURE(kProjector, "Projector", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Projector for managed users.
BASE_FEATURE(kProjectorManagedUser,
             "ProjectorManagedUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Projector app launches in debug mode, with more detailed
// error messages.
BASE_FEATURE(kProjectorAppDebug,
             "ProjectorAppDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to fold short gap between transcript into the previous
// transcript.
BASE_FEATURE(kProjectorFoldShortGapIntoPreviousTranscript,
             "ProjectorFoldShortGapIntoPreviousTranscript",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Projector's tutorial videos are displayed.
BASE_FEATURE(kProjectorTutorialVideoView,
             "ProjectorTutorialVideoView",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Projector use custom thumbnail in gallery page.
BASE_FEATURE(kProjectorCustomThumbnail,
             "kProjectorCustomThumbnail",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Controls whether to use OAuth token for getting streaming URL from
// get_video_info endpoint.
BASE_FEATURE(kProjectorUseOAuthForGetVideoInfo,
             "ProjectorUseOAuthForGetVideoInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow viewing screencast with local playback URL when
// screencast is being transcoded.
BASE_FEATURE(kProjectorLocalPlayback,
             "ProjectorLocalPlayback",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable features that are not ready to enable by
// default but ready for internal testing.
BASE_FEATURE(kProjectorBleedingEdgeExperience,
             "ProjectorBleedingEdgeExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable crash report from the Projector web component.
BASE_FEATURE(kProjectorWebReportCrash,
             "ProjectorWebReportCrash",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use API key instead of OAuth token for translation
// requests.
BASE_FEATURE(kProjectorUseApiKeyForTranslation,
             "ProjectorUseApiKeyForTranslation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable quick settings revamped view.
BASE_FEATURE(kQsRevamp, "QsRevamp", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the Projector Viewer supports the user experience for
// secondary account.
BASE_FEATURE(kProjectorViewerUseSecondaryAccount,
             "ProjectorViewerUseSecondaryAccount",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show toast notification when account switches.
BASE_FEATURE(kProjectorAccountSwitchNotification,
             "ProjectorAccountSwitchNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show promise icons during app installations.
BASE_FEATURE(kPromiseIcons, "PromiseIcons", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the quick dim prototype is enabled.
BASE_FEATURE(kQuickDim, "QuickDim", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the smart reader feature is enabled.
BASE_FEATURE(kSmartReader, "SmartReader", base::FEATURE_DISABLED_BY_DEFAULT);

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

// When enabled, the overview and desk reverse scrolling behaviors are changed
// and if the user performs the old gestures, a notification or toast will show
// up.
// TODO(https://crbug.com/1107183): Remove this after the feature is launched.
BASE_FEATURE(kReverseScrollGestures,
             "EnableReverseScrollGestures",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRgbKeyboard, "RgbKeyboard", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the scanning app. Requires
// jelly-colors flag to also be enabled.
BASE_FEATURE(kScanningAppJelly,
             "ScanningAppJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables screensaver customized running time.
BASE_FEATURE(kScreenSaverDuration,
             "ScreenSaverDuration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the "Preview" button for screensaver.
BASE_FEATURE(kScreenSaverPreview,
             "ScreenSaverPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the shelf party.
BASE_FEATURE(kShelfParty, "ShelfParty", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the shelf party.
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

// Enables or disables the dark mode in the shimless RMA flow.
BASE_FEATURE(kShimlessRMADisableDarkMode,
             "ShimlessRMADisableDarkMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the diagnostic page in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMADiagnosticPage,
             "ShimlessRMADiagnosticPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the jelly colors will be used in the shortcut customization app.
// Requires jelly-colors flag to also be enabled.
BASE_FEATURE(kShortcutCustomizationJelly,
             "ShortcutCustomizationJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables a toggle to enable Bluetooth debug logs.
BASE_FEATURE(kShowBluetoothDebugLogToggle,
             "ShowBluetoothDebugLogToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows the Play Store icon in Demo Mode.
BASE_FEATURE(kShowPlayInDemoMode,
             "ShowPlayInDemoMode",
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

BASE_FEATURE(kSmdsSupport, "SmdsSupport", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmdsSupportEuiccUpload,
             "SmdsSupportEuiccUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmdsDbusMigration,
             "SmdsDbusMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the snap group feature is enabled or not.
BASE_FEATURE(kSnapGroup, "SnapGroup", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to create the snap group automatically when two windows are
// snapped if true. Otherwise, the user has to explicitly lock the two windows
// when both are snapped via cliking on the lock button when hovering the mouse
// over the shared edge of the two snapped windows.
constexpr base::FeatureParam<bool> kAutomaticallyLockGroup{
    &kSnapGroup, "AutomaticLockGroup", true};

// Controls whether the speak-on-mute detection feature is enabled or not.
BASE_FEATURE(kSpeakOnMuteEnabled,
             "SpeakOnMuteEnabled",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ability to play sounds for system services.
BASE_FEATURE(kSystemSounds, "SystemSounds", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables multi-profile theme support for Terminal..
BASE_FEATURE(kTerminalMultiProfile,
             "TerminalMultiProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables SFTP / mount for Terminal..
BASE_FEATURE(kTerminalSftp, "TerminalSftp", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables tmux integration in the Terminal System App.
BASE_FEATURE(kTerminalTmuxIntegration,
             "TerminalTmuxIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables time of day screen saver.
BASE_FEATURE(kTimeOfDayScreenSaver,
             "TimeOfDayScreenSaver",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables time of day wallpaper.
BASE_FEATURE(kTimeOfDayWallpaper,
             "TimeOfDayWallpaper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the TrafficCountersHandler class to auto-reset traffic counters
// and shows Data Usage in the Celluar Settings UI.
BASE_FEATURE(kTrafficCountersEnabled,
             "TrafficCountersEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables trilinear filtering.
BASE_FEATURE(kTrilinearFiltering,
             "TrilinearFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses new AuthFactor-based API when communicating with cryptohome.
// This feature flag also affects usage of AuthSessions in QuickUnlock, but
// only in case when cryptohome is used as backend.
// This feature flag also affects usage of AuthSession on lock screen.
BASE_FEATURE(kUseAuthFactors,
             "UseAuthFactors",
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

// Use the Stork Production SM-DS address to fetch pending ESim profiles
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

// Controls whether the vc background replace is enabled.
BASE_FEATURE(kVcBackgroundReplace,
             "VCBackgroundReplace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is only used as a way to disable portrait relighting.
BASE_FEATURE(kVcPortraitRelight,
             "VcPortraitRelight",
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

// Enable or disable multitouch for virtual keyboard on ChromeOS.
BASE_FEATURE(kVirtualKeyboardMultitouch,
             "VirtualKeyboardMultitouch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable round corners for virtual keyboard on ChromeOS.
BASE_FEATURE(kVirtualKeyboardRoundCorners,
             "VirtualKeyboardRoundCorners",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable different wallpapers per desk.
BASE_FEATURE(kWallpaperPerDesk,
             "WallpaperPerDesk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables special handling of Chrome tab drags from a WebUI tab strip.
// These will be treated similarly to a window drag, showing split view
// indicators in tablet mode, etc. The functionality is behind a flag right now
// since it is under development.
BASE_FEATURE(kWebUITabStripTabDragIntegration,
             "WebUITabStripTabDragIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Welcome Tour that walks new users through ChromeOS System UI.
BASE_FEATURE(kWelcomeTour, "WelcomeTour", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Change window creation to be based on cursor position when there are multiple
// displays.
BASE_FEATURE(kWindowsFollowCursor,
             "WindowsFollowCursor",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn cohort device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnCohortCheckMembership,
             "DeviceActiveClientChurnCohortCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckIn for the churn observation device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnObservationCheckIn,
             "DeviceActiveClientChurnObservationCheckIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

bool AreCaptureModeDemoToolsEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeDemoTools);
}

bool AreContextualNudgesEnabled() {
  if (!IsHideShelfControlsInTabletModeEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kContextualNudges);
}

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool ArePolicyProvidedTrustAnchorsAllowedAtLockScreen() {
  return base::FeatureList::IsEnabled(
      kPolicyProvidedTrustAnchorsAllowedAtLockScreen);
}

bool ArePromiseIconsEnabled() {
  return base::FeatureList::IsEnabled(kPromiseIcons);
}

bool AreSideAlignedToastsEnabled() {
  return base::FeatureList::IsEnabled(kSideAlignedToasts);
}

bool AreSystemSoundsEnabled() {
  return base::FeatureList::IsEnabled(kSystemSounds);
}

bool IsAutocompleteExtendedSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kAutocompleteExtendedSuggestions);
}

bool IsAutoEnrollmentKioskInOobeEnabled() {
  return base::FeatureList::IsEnabled(kAutoEnrollmentKioskInOobe);
}

bool DoWindowsFollowCursor() {
  return base::FeatureList::IsEnabled(kWindowsFollowCursor);
}

bool Is16DesksEnabled() {
  return base::FeatureList::IsEnabled(kEnable16Desks);
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

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsEapDefaultCasWithoutSubjectVerificationAllowed() {
  return base::FeatureList::IsEnabled(
      kAllowEapDefaultCasWithoutSubjectVerification);
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

bool IsAppCollectionFolderRefreshEnabled() {
  return base::FeatureList::IsEnabled(kAppCollectionFolderRefresh);
}

bool IsArcFuseBoxFileSharingEnabled() {
  return base::FeatureList::IsEnabled(kArcFuseBoxFileSharing);
}

bool IsArcInputOverlayBetaEnabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlayBeta);
}

bool IsArcInputOverlayAlphaV2Enabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlayAlphaV2);
}

bool IsAssistantNativeIconsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantNativeIcons);
}

bool IsAssistiveMultiWordEnabled() {
  return base::FeatureList::IsEnabled(kAssistMultiWord);
}

bool IsAudioSettingsPageEnabled() {
  return base::FeatureList::IsEnabled(kAudioSettingsPage);
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

bool IsBluetoothQualityReportEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothQualityReport);
}

bool IsCalendarJellyEnabled() {
  return base::FeatureList::IsEnabled(kCalendarJelly);
}

bool IsCaptivePortalErrorPageEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalErrorPage);
}

bool IsCaptureModeTourEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeTour);
}

bool IsCheckPasswordsAgainstCryptohomeHelperEnabled() {
  return base::FeatureList::IsEnabled(kCheckPasswordsAgainstCryptohomeHelper);
}

bool IsClipboardHistoryLongpressEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryLongpress);
}

bool IsClipboardHistoryRefreshEnabled() {
  return chromeos::features::IsJellyEnabled() &&
         base::FeatureList::IsEnabled(kClipboardHistoryRefresh);
}

bool IsClipboardHistoryReorderEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryReorder);
}

bool IsCryptauthAttestationSyncingEnabled() {
  return base::FeatureList::IsEnabled(kCryptauthAttestationSyncing);
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

bool IsCrosPrivacyHubEnabled() {
  return IsCrosPrivacyHubV0Enabled() || IsCrosPrivacyHubV1Enabled() ||
         IsCrosPrivacyHubV2Enabled();
}

bool IsCrosPrivacyHubV0Enabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubV0) ||
         IsCrosPrivacyHubV1Enabled();
}

bool IsCrosPrivacyHubV2Enabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHubV2);
}

bool IsCrosPrivacyHubV1Enabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHub) ||
         IsCrosPrivacyHubV2Enabled();
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

bool IsDisplayAlignmentAssistanceEnabled() {
  return base::FeatureList::IsEnabled(kDisplayAlignAssist);
}

bool IsDriveFsMirroringEnabled() {
  return base::FeatureList::IsEnabled(kDriveFsMirroring);
}

bool IsDriveFsBulkPinningEnabled() {
  return base::FeatureList::IsEnabled(kDriveFsBulkPinning);
}

bool IsInlineSyncStatusEnabled() {
  return base::FeatureList::IsEnabled(kFilesInlineSyncStatus);
}

bool IsEapGtcWifiAuthenticationEnabled() {
  return base::FeatureList::IsEnabled(kEapGtcWifiAuthentication);
}

bool IsAudioPeripheralVolumeGranularityEnabled() {
  return base::FeatureList::IsEnabled(kAudioPeripheralVolumeGranularity);
}

bool IsAudioSourceFetcherResamplingEnabled() {
  // TODO(b/245617354): Once ready, enable this feature under
  // kProjectorBleedingEdgeExperience flag as well.
  return base::FeatureList::IsEnabled(kAudioSourceFetcherResampling);
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

bool IsFastPairHandshakeRefactorEnabled() {
  return base::FeatureList::IsEnabled(kFastPairHandshakeRefactor);
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

bool AreGlanceablesEnabled() {
  return base::FeatureList::IsEnabled(kGlanceables);
}

bool AreGlanceablesV2Enabled() {
  return base::FeatureList::IsEnabled(kGlanceablesV2);
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

bool IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled() {
  return base::FeatureList::IsEnabled(
      kHoldingSpaceInProgressDownloadsNotificationSuppression);
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

bool IsHoldingSpaceTourEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceTour);
}

bool IsHomeButtonQuickAppAccessEnabled() {
  return base::FeatureList::IsEnabled(kHomeButtonQuickAppAccess);
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

bool IsScreenSaverPreviewEnabled() {
  return base::FeatureList::IsEnabled(kScreenSaverPreview);
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

bool IsInputInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableInputInDiagnosticsApp);
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

bool IsKeyboardBacklightToggleEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardBacklightToggle);
}

bool IsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(kHandwritingLegacyRecognition);
}

bool IsLauncherNudgeShortIntervalEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeShortInterval);
}

bool IsLauncherNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kLauncherNudgeSessionReset);
}

bool IsLicensePackagedOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kLicensePackagedOobeFlow);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsEducationEnrollmentOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kEducationEnrollmentOobeFlow);
}

bool IsEnrollmentNudgingForTestingEnabled() {
  return base::FeatureList::IsEnabled(kEnrollmentNudgingForTesting);
}

bool IsGameDashboardEnabled() {
  if (!base::FeatureList::IsEnabled(kGameDashboard)) {
    return false;
  }

  // Only allow the dashboard on test images until further in development.
  std::string track;
  return base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &track) &&
         track.find(kTestImageRelease) != std::string::npos;
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

bool IsNearbyKeepAliveFixEnabled() {
  return base::FeatureList::IsEnabled(kNearbyKeepAliveFix);
}

bool IsOAuthIppEnabled() {
  return base::FeatureList::IsEnabled(kEnableOAuthIpp);
}

bool IsNewLockScreenReauthLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewLockScreenReauthLayout);
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

bool IsOobeSimonEnabled() {
  return base::FeatureList::IsEnabled(kOobeSimon);
}

bool IsOobeNetworkScreenSkipEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeNetworkScreenSkip);
}

bool IsOobeChoobeEnabled() {
  return base::FeatureList::IsEnabled(kOobeChoobe);
}

bool IsOobeDrivePinningEnabled() {
  return base::FeatureList::IsEnabled(kOobeDrivePinning) &&
         IsOobeChoobeEnabled();
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

bool IsOsSettingsAppBadgingToggleEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppBadgingToggle);
}

bool IsOsSettingsSearchFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsSearchFeedback);
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

bool IsPcieBillboardNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPcieBillboardNotification);
}

bool IsPerDeskShelfEnabled() {
  return base::FeatureList::IsEnabled(kPerDeskShelf);
}

bool IsPersonalizationJellyEnabled() {
  return base::FeatureList::IsEnabled(kPersonalizationJelly) &&
         chromeos::features::IsJellyEnabled();
}

bool IsPhoneHubCameraRollEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCameraRoll);
}

bool IsPhoneHubMonochromeNotificationIconsEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubMonochromeNotificationIcons);
}

bool IsPhoneHubNudgeEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubNudge);
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

bool IsPhoneHubCallNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCallNotification);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsPinAutosubmitFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmit);
}

bool IsPrinterSettingsRevampEnabled() {
  return base::FeatureList::IsEnabled(kPrinterSettingsRevamp);
}

bool IsPrivacyIndicatorsEnabled() {
  // Privacy indicators should not be enabled when video conference is enabled.
  return base::FeatureList::IsEnabled(kPrivacyIndicators) &&
         !IsVideoConferenceEnabled();
}

bool IsProductivityLauncherEnabled() {
  return base::FeatureList::IsEnabled(kProductivityLauncher);
}

bool IsProjectorEnabled() {
  return IsProjectorAllUserEnabled() || IsProjectorManagedUserEnabled();
}

bool IsProjectorAllUserEnabled() {
  return base::FeatureList::IsEnabled(kProjector);
}

bool IsProjectorManagedUserEnabled() {
  return base::FeatureList::IsEnabled(kProjectorManagedUser);
}

bool IsProjectorAppDebugMode() {
  return base::FeatureList::IsEnabled(kProjectorAppDebug);
}

bool IsProjectorTutorialVideoViewEnabled() {
  return base::FeatureList::IsEnabled(kProjectorTutorialVideoView);
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

bool IsProjectorUseOAuthForGetVideoInfoEnabled() {
  return base::FeatureList::IsEnabled(kProjectorUseOAuthForGetVideoInfo);
}

bool IsProjectorLocalPlaybackEnabled() {
  return base::FeatureList::IsEnabled(kProjectorLocalPlayback) ||
         base::FeatureList::IsEnabled(kProjectorBleedingEdgeExperience);
}

bool IsProjectorWebReportCrashEnabled() {
  return base::FeatureList::IsEnabled(kProjectorWebReportCrash);
}

bool IsProjectorUseApiKeyForTranslationEnabled() {
  return base::FeatureList::IsEnabled(kProjectorUseApiKeyForTranslation);
}

bool IsQsRevampEnabled() {
  return base::FeatureList::IsEnabled(kQsRevamp);
}

bool IsProjectorViewerUseSecondaryAccountEnabled() {
  return base::FeatureList::IsEnabled(kProjectorViewerUseSecondaryAccount);
}

bool IsProjectorAccountSwitchNotificationEnabled() {
  return base::FeatureList::IsEnabled(kProjectorAccountSwitchNotification);
}

bool IsProjectorFoldShortGapIntoPreviousTranscriptEnabled() {
  return base::FeatureList::IsEnabled(
      kProjectorFoldShortGapIntoPreviousTranscript);
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

bool IsReverseScrollGesturesEnabled() {
  return base::FeatureList::IsEnabled(kReverseScrollGestures);
}

bool IsRgbKeyboardEnabled() {
  return base::FeatureList::IsEnabled(kRgbKeyboard);
}

bool IsSameAppWindowCycleEnabled() {
  return base::FeatureList::IsEnabled(kSameAppWindowCycle);
}

bool IsSamlNotificationOnPasswordChangeSuccessEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableSamlNotificationOnPasswordChangeSuccess);
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

bool IsShimlessRMADarkModeDisabled() {
  return base::FeatureList::IsEnabled(kShimlessRMADisableDarkMode);
}

bool IsShimlessRMADiagnosticPageEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMADiagnosticPage);
}

bool IsSmdsSupportEnabled() {
  return base::FeatureList::IsEnabled(kSmdsDbusMigration) &&
         base::FeatureList::IsEnabled(kSmdsSupport);
}

bool IsSmdsSupportEuiccUploadEnabled() {
  return base::FeatureList::IsEnabled(kSmdsDbusMigration) &&
         base::FeatureList::IsEnabled(kSmdsSupport) &&
         base::FeatureList::IsEnabled(kSmdsSupportEuiccUpload);
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

bool IsSpeakOnMuteEnabled() {
  return base::FeatureList::IsEnabled(kSpeakOnMuteEnabled);
}

bool IsSystemTrayShadowEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayShadow);
}

bool IsStylusBatteryStatusEnabled() {
  return base::FeatureList::IsEnabled(kStylusBatteryStatus);
}

bool IsTimeOfDayScreenSaverEnabled() {
  // TODO(b/270597524): Check `kFeatureManagementTimeOfDayScreenSaver` flag as
  // well when feature management team has completed their design.
  return base::FeatureList::IsEnabled(kTimeOfDayScreenSaver) &&
         IsTimeOfDayWallpaperEnabled();
}

bool IsTimeOfDayWallpaperEnabled() {
  // TODO(b/270597524): Check `kFeatureManagementTimeOfDayWallpaper` flag as
  // well when feature management team has completed their design.
  return base::FeatureList::IsEnabled(kTimeOfDayWallpaper);
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

bool IsUseLoginShelfWidgetEnabled() {
  return base::FeatureList::IsEnabled(kUseLoginShelfWidget);
}

bool IsUseStorkSmdsServerAddressEnabled() {
  return base::FeatureList::IsEnabled(kUseStorkSmdsServerAddress);
}

bool IsUserEducationEnabled() {
  return IsCaptureModeTourEnabled() || IsHoldingSpaceTourEnabled() ||
         IsWelcomeTourEnabled();
}

bool IsVideoConferenceEnabled() {
  return base::FeatureList::IsEnabled(kVideoConference) &&
         switches::IsCameraEffectsSupportedByHardware();
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

bool IsViewPpdEnabled() {
  return base::FeatureList::IsEnabled(kEnableViewPpd);
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

bool IsWebUITabStripTabDragIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kWebUITabStripTabDragIntegration);
}

bool IsWelcomeTourEnabled() {
  return base::FeatureList::IsEnabled(kWelcomeTour);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
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

bool IsSearchInShortcutsAppEnabled() {
  return base::FeatureList::IsEnabled(kSearchInShortcutsApp);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(kCryptAuthV2DeviceSync);
}

}  // namespace features
}  // namespace ash
