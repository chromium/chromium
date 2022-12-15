// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "ash/constants/ash_switches.h"
#include "ash_features.h"
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

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
BASE_FEATURE(kAllowAmbientEQ,
             "AllowAmbientEQ",
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

// Controls whether to enable Ambient mode feature.
BASE_FEATURE(kAmbientModeFeature,
             "ChromeOSAmbientMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled{
    &kAmbientModeFeature, "CapturedOnPixelAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled{
    &kAmbientModeFeature, "CapturedOnPixelPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCulturalInstitutePhotosEnabled{
    &kAmbientModeFeature, "CulturalInstitutePhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled{
    &kAmbientModeFeature, "DefaultFeedEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled{
    &kAmbientModeFeature, "EarthAndSpaceAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled{
    &kAmbientModeFeature, "FeaturedPhotoAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled{
    &kAmbientModeFeature, "FeaturedPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled{
    &kAmbientModeFeature, "FineArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled{
    &kAmbientModeFeature, "GeoPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled{
    &kAmbientModeFeature, "PersonalPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeRssPhotosEnabled{
    &kAmbientModeFeature, "RssPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled{
    &kAmbientModeFeature, "StreetArtAlbumEnabled", false};

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

BASE_FEATURE(kAmbientSubpageUIChange,
             "AmbientSubpageUIChange",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApnRevamp, "ApnRevamp", base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Controls whether to enable assistive personal information.
BASE_FEATURE(kAssistPersonalInfo,
             "AssistPersonalInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to suggest addresses in assistive personal information. This
// is only effective when AssistPersonalInfo flag is enabled.
BASE_FEATURE(kAssistPersonalInfoAddress,
             "AssistPersonalInfoAddress",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to suggest emails in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
BASE_FEATURE(kAssistPersonalInfoEmail,
             "AssistPersonalInfoEmail",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to suggest names in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
BASE_FEATURE(kAssistPersonalInfoName,
             "AssistPersonalInfoName",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to suggest phone numbers in assistive personal information.
// This is only effective when AssistPersonalInfo flag is enabled.
BASE_FEATURE(kAssistPersonalInfoPhoneNumber,
             "AssistPersonalInfoPhoneNumber",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the persistent desks bar at the top of the screen in clamshell mode
// when there are more than one desk.
BASE_FEATURE(kBentoBar, "BentoBar", base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Big GL when using Borealis.
BASE_FEATURE(kBorealisBigGl, "BorealisBigGl", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enable TermsOfServiceURL policy for managed users.
// https://crbug.com/1221342
BASE_FEATURE(kManagedTermsOfService,
             "ManagedTermsOfService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable calendar view from the system tray. Also enables the system
// tray to show date in the shelf when the screen is sufficiently large.
BASE_FEATURE(kCalendarView, "CalendarView", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable debug mode for CalendarModel.
BASE_FEATURE(kCalendarModelDebugMode,
             "CalendarModelDebugMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable calendar jelly.
BASE_FEATURE(kCalendarJelly,
             "CalendarJelly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to allow low storage warning feature in the camera app.
BASE_FEATURE(kCameraAppLowStorageWarning,
             "CameraAppLowStorageWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to show multi-page UI when for document scanning feature in the
// camera app.
BASE_FEATURE(kCameraAppMultiPageDocScan,
             "CameraAppMultiPageDocScan",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
BASE_FEATURE(kCameraPrivacySwitchNotifications,
             "CameraPrivacySwitchNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the capture mode demo tools feature is enabled for Capture
// Mode.
BASE_FEATURE(kCaptureModeDemoTools,
             "CaptureModeDemoTools",
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

// If enabled, the clipboard nudge shown prefs will be reset at the start of
// each new user session.
BASE_FEATURE(kClipboardHistoryNudgeSessionReset,
             "ClipboardHistoryNudgeSessionReset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables updated UI for the clipboard history menu and new system behavior
// related to clipboard history.
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

// If enabled, replaces the `DeskMiniView` legacy desk close button and behavior
// with a button to close desk and windows and a button to combine desks (the
// legacy behavior).
BASE_FEATURE(kDesksCloseAll, "DesksCloseAll", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the Captive Portal UI 2022 changes, which includes updates to
// notifications, network details page, quick settings, and portal signin UI.
BASE_FEATURE(kCaptivePortalUI2022,
             "CaptivePortalUI2022",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Captive Portal Error Page changes, which shows a suggestion in
// the Chrome error page on ChromeOS when behind a captive portal.
BASE_FEATURE(kCaptivePortalErrorPage,
             "CaptivePortalErrorPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Active Directory management on ChromeOS (Chromad) is
// supported or not. When this feature is enabled, Chromad continues working
// normally. Disabling this feature will block enrollment in AD mode, and will
// disable devices that are already in AD mode - displaying an error message to
// the user.
BASE_FEATURE(kChromadAvailable,
             "ChromadAvailable",
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

// Enables the Cryptohome recovery feature, which allows users to recover access
// to their profile and Cryptohome after performing an online authentication.
BASE_FEATURE(kCryptohomeRecoveryFlow,
             "CryptohomeRecoveryFlow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI for the cryptohome recovery feature:
// - New UI for Gaia password changed screen.
// - Adds a "forgot password" button to the error bubble that opens when the
//   user fails to enter their correct password.
BASE_FEATURE(kCryptohomeRecoveryFlowUI,
             "CryptohomeRecoveryFlowUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI to enable or disable cryptohome recovery in the settings
// page. Also guards the wiring of cryptohome recovery settings to the
// cryptohome backend.
BASE_FEATURE(kCryptohomeRecoverySetup,
             "CryptohomeRecoverySetup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDarkLightModeKMeansColor,
             "DarkLightModeKMeansColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Assistant stylus features, including the
// Assistant option in the stylus palette tool and the Assistant screen
// selection flow triggered by the stylus long press action.
BASE_FEATURE(kDeprecateAssistantStylusFeatures,
             "DeprecateAssistantStylusFeatures",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables indicators to hint where displays are connected.
BASE_FEATURE(kDisplayAlignAssist,
             "DisplayAlignAssist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable DNS over HTTPS (DoH) with identifiers. Only available on ChromeOS.
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

// Enables dragging an unpinned open app to pinned app side to pin.
BASE_FEATURE(kDragUnpinnedAppToPin,
             "DragUnpinnedAppToPin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables dragging and dropping an existing window to new desk in overview.
BASE_FEATURE(kDragWindowToNewDesk,
             "DragWindowToNewDesk",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, DriveFS will be used for Drive sync.
BASE_FEATURE(kDriveFs, "DriveFS", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables duplex native messaging between DriveFS and extensions.
BASE_FEATURE(kDriveFsBidirectionalNativeMessaging,
             "DriveFsBidirectionalNativeMessaging",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the DNS proxy service providing support split and secure DNS
// for ChromeOS.
BASE_FEATURE(kEnableDnsProxy,
             "EnableDnsProxy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables external keyboard testers in the diagnostics app.
BASE_FEATURE(kEnableExternalKeyboardsInDiagnostics,
             "EnableExternalKeyboardsInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the device hostname.
BASE_FEATURE(kEnableHostnameSetting,
             "EnableHostnameSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the input device cards will be shown in the diagnostics app.
BASE_FEATURE(kEnableInputInDiagnosticsApp,
             "EnableInputInDiagnosticsApp",
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

// Enables using DiagnosticsLogController to manage lifetime of logs for the
// diagnostics app routines, network events, and system snapshot.
// TODO(ashleydp): Remove this after the feature is launched.
BASE_FEATURE(kEnableLogControllerForDiagnosticsApp,
             "EnableLogControllerForDiagnosticsApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the networking cards will be shown in the diagnostics app.
BASE_FEATURE(kEnableNetworkingInDiagnosticsApp,
             "EnableNetworkingInDiagnosticsApp",
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

// Enables skipping of network screen.
BASE_FEATURE(kEnableOobeThemeSelection,
             "EnableOobeThemeSelection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing notification after the password change for SAML users.
BASE_FEATURE(kEnableSamlNotificationOnPasswordChangeSuccess,
             "EnableSamlNotificationOnPasswordChangeSuccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSavedDesks,
             "EnableSavedDesks",
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

// Enables Device End Of Lifetime warning notifications.
BASE_FEATURE(kEolWarningNotifications,
             "EolWarningNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the "Subsequent Pairing" Fast Pair scenario in Bluetooth Settings
// and Quick Settings.
BASE_FEATURE(kFastPairSubsequentPairingUX,
             "FastPairSubsequentPairingUX",
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

// Enables experimental UI features in Files app.
BASE_FEATURE(kFilesAppExperimental,
             "FilesAppExperimental",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable inline sync status in Files app.
BASE_FEATURE(kFilesInlineSyncStatus,
             "FilesInlineSyncStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables filters in Files app Recents view V2.
BASE_FEATURE(kFiltersInRecentsV2,
             "FiltersInRecentsV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the firmware updater app.
BASE_FEATURE(kFirmwareUpdaterApp,
             "FirmwareUpdaterApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables first party Vietnamese input method.
BASE_FEATURE(kFirstPartyVietnameseInput,
             "FirstPartyVietnameseInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Floating Workspace feature on ChromeOS
BASE_FEATURE(kFloatingWorkspace,
             "FloatingWorkspace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Floating Workspace V2 feature on ChromeOS
BASE_FEATURE(kFloatingWorkspaceV2,
             "FloatingWorkspaceV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, makes the Projector app use server side speech
// recognition instead of on-device speech recognition.
BASE_FEATURE(kForceEnableServerSideSpeechRecognitionForDev,
             "ForceEnableServerSideSpeechRecognitionForDev",
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

// Enables the Gaia reauth endpoint.
BASE_FEATURE(kGaiaReauthEndpoint,
             "GaiaReauthEndpoint",
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

// Enables editing with handwriting gestures within the virtual keyboard.
BASE_FEATURE(kHandwritingGestureEditing,
             "HandwritingGestureEditing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new on-device recognition for legacy handwriting input.
BASE_FEATURE(kHandwritingLegacyRecognition,
             "HandwritingLegacyRecognition",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new on-device recognition for legacy handwriting input in all
// supported languages.
BASE_FEATURE(kHandwritingLegacyRecognitionAllLang,
             "HandwritingLegacyRecognitionAllLang",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables downloading the handwriting libraries via DLC.
BASE_FEATURE(kHandwritingLibraryDlc,
             "HandwritingLibraryDlc",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new histogram logic for ChromeOS HaTS surveys.
BASE_FEATURE(kHatsUseNewHistograms,
             "HatsUseNewHistograms",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Background Page in the help app.
BASE_FEATURE(kHelpAppBackgroundPage,
             "HelpAppBackgroundPage",
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

// Enables or disables the flag to synchronize launcher item colors. It is
// in effect only when kLauncherAppSort is enabled.
BASE_FEATURE(kLauncherItemColorSync,
             "LauncherItemColorSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a privacy improvement that removes wrongly configured hidden
// networks and mitigates the creation of these networks. crbug/1327803.
BASE_FEATURE(kHiddenNetworkMigration,
             "HiddenNetworkMigration",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable a new header bar for the ChromeOS virtual keyboard.
BASE_FEATURE(kVirtualKeyboardNewHeader,
             "VirtualKeyboardNewHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, used to configure the heuristic rules for some advanced IME
// features (e.g. auto-correct).
BASE_FEATURE(kImeRuleConfig, "ImeRuleConfig", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables Jelly features.
BASE_FEATURE(kJelly, "Jelly", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Jellyroll features.
BASE_FEATURE(kJellyroll, "Jellyroll", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Disable this to turn off profile migration for non-googlers.
BASE_FEATURE(kLacrosProfileMigrationForAnyUser,
             "LacrosProfileMigrationForAnyUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables sorting app icons shown on the launcher.
BASE_FEATURE(kLauncherAppSort,
             "LauncherAppSort",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, app list folders will be moved so app list remains sorted when
// they get renamed, or created.
BASE_FEATURE(kLauncherFolderRenameKeepsSortOrder,
             "LauncherFolderRenameKeepsSortOrder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the app list sort nudge and toast will have additional
// buttons for dismissal.
BASE_FEATURE(kLauncherDismissButtonsOnSortNudgeAndToast,
             "LauncherDismissButtonsOnSortNudgeAndToast",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the custom color picker and recent colors UI in the media app.
BASE_FEATURE(kMediaAppCustomColors,
             "MediaAppCustomColors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Within the ChromeOS media app, reveals the button to edit the current image
// in Photos.
BASE_FEATURE(kMediaAppPhotosIntegrationImage,
             "MediaAppPhotosIntegrationImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Within the ChromeOS media app, reveals the button to edit the current video
// in Photos.
BASE_FEATURE(kMediaAppPhotosIntegrationVideo,
             "MediaAppPhotosIntegrationVideo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables notification of when a microphone-using app is launched while the
// microphone is muted.
BASE_FEATURE(kMicMuteNotifications,
             "MicMuteNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Migrates rule-based input methods from Chromium into an internal codebase.
BASE_FEATURE(kMigrateRuleBasedInputMethods,
             "MigrateRuleBasedInputMethods",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kEcheLauncher, "EcheLauncher", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables notification scroll bar in UnifiedSystemTray.
BASE_FEATURE(kNotificationScrollBar,
             "NotificationScrollBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notifications to be shown within context menus.
BASE_FEATURE(kNotificationsInContextMenu,
             "NotificationsInContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables new notifications UI and grouped notifications.
BASE_FEATURE(kNotificationsRefresh,
             "NotificationsRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, EULA and ARC Terms of Service screens are skipped and merged
// into Consolidated Consent Screen.
BASE_FEATURE(kOobeConsolidatedConsent,
             "OobeConsolidatedConsent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the ChromeOS OOBE HID Detection Revamp, which updates
// the OOBE HID detection screen UI and related infrastructure. See
// https://crbug.com/1299099.
BASE_FEATURE(kOobeHidDetectionRevamp,
             "OobeHidDetectionRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Oobe quick start flow.
BASE_FEATURE(kOobeQuickStart,
             "OobeQuickStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OOBE Material Next features.
BASE_FEATURE(kOobeMaterialNext,
             "OobeMaterialNext",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes "Shut down" button from OOBE, except first login screen and
// successful enrollment step.
BASE_FEATURE(kOobeRemoveShutdownButton,
             "OobeRemoveShutdownButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the feedback tool new UX on ChromeOS.
// This tool under development will be rolled out via Finch.
// Enabling this flag will use the new feedback tool instead of the current
// tool on CrOS.
BASE_FEATURE(kOsFeedback, "OsFeedback", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a new App Notifications subpage will appear in CrOS Apps section.
BASE_FEATURE(kOsSettingsAppNotificationsPage,
             "OsSettingsAppNotificationsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables app badging toggle to be displayed in app notification page in
// ChromeOS Settings.
BASE_FEATURE(kOsSettingsAppBadgingToggle,
             "OsSettingsAppBadgingToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables search result feedback in ChromeOS Settings when no search results
// are returned.
BASE_FEATURE(kOsSettingsSearchFeedback,
             "OsSettingsSearchFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOverviewButton,
             "OverviewButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables staying in overview when navigating between desks using a swipe
// gesture or keyboard shortcut.
BASE_FEATURE(kOverviewDeskNavigation,
             "OverviewDeskNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a notification warning users that their Thunderbolt device is not
// supported on their CrOS device.
BASE_FEATURE(kPcieBillboardNotification,
             "PcieBillboardNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Limits the items on the shelf to the ones associated with windows the
// currently active desk.
BASE_FEATURE(kPerDeskShelf, "PerDeskShelf", base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the incoming/ongoing call notification feature in Phone Hub.
BASE_FEATURE(kPhoneHubCallNotification,
             "PhoneHubCallNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubMonochromeNotificationIcons,
             "PhoneHubMonochromeNotificationIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubPingOnBubbleOpen,
             "PhoneHubPingOnBubbleOpen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
BASE_FEATURE(kPreferConstantFrameRate,
             "PreferConstantFrameRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set the channel from which the PPD files are loaded.
BASE_FEATURE(kPrintingPpdChannel,
             "PrintingPpdChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<PrintingPpdChannel>::Option
    printing_ppd_channel_options[] = {
        {PrintingPpdChannel::kProduction, "production"},
        {PrintingPpdChannel::kStaging, "staging"},
        {PrintingPpdChannel::kDev, "dev"}};
const base::FeatureParam<PrintingPpdChannel> kPrintingPpdChannelParam{
    &kPrintingPpdChannel, "channel", PrintingPpdChannel::kProduction,
    &printing_ppd_channel_options};

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

// Controls whether to enable Projector annotator tools.
// The annotator tools are based on the ink library.
BASE_FEATURE(kProjectorAnnotator,
             "ProjectorAnnotator",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Projector app launches in debug mode, with more detailed
// error messages.
BASE_FEATURE(kProjectorAppDebug,
             "ProjectorAppDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the Projector exclude transcript feature is enabled.
BASE_FEATURE(kProjectorExcludeTranscript,
             "ProjectorExcludeTranscript",
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

// Enable or disable quick settings revamped view. This flag only works when the
// `QsRevampWip` flag is enabled.
BASE_FEATURE(kQsRevamp, "QsRevamp", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable quick settings revamped wip view.
// TODO(b/257541368): remove this flag once the wip view is finished.
BASE_FEATURE(kQsRevampWip, "QsRevampWip", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Controls whether the vc background blur is enabled.
BASE_FEATURE(kVCBackgroundBlur,
             "VCBackgroundBlur",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the vc background replace is enabled.
BASE_FEATURE(kVCBackgroundReplace,
             "VCBackgroundReplace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the vc portrait relighting is enabled.
BASE_FEATURE(kVCPortraitRelighting,
             "VCPortraitRelighting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Quick Settings Network revamp, which updates Network
// Quick Settings UI and related infrastructure. See https://crbug.com/1169479.
BASE_FEATURE(kQuickSettingsNetworkRevamp,
             "QuickSettingsNetworkRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables display of the release track in the system tray and quick
// settings, for devices running on channels other than "stable."
BASE_FEATURE(kReleaseTrackUi,
             "ReleaseTrackUi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the overview and desk reverse scrolling behaviors are changed
// and if the user performs the old gestures, a notification or toast will show
// up.
// TODO(https://crbug.com/1107183): Remove this after the feature is launched.
BASE_FEATURE(kReverseScrollGestures,
             "EnableReverseScrollGestures",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRgbKeyboard, "RgbKeyboard", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the "Preview" button for screensaver.
BASE_FEATURE(kScreenSaverPreview,
             "ScreenSaverPreview",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables shelf gestures (swipe to show hotseat, swipe to go home or overview)
// in tablet mode when virtual keyboard is shown.
BASE_FEATURE(kShelfGesturesWithVirtualKeyboard,
             "ShelfGesturesWithVirtualKeyboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables launcher nudge that animates the home button to guide users to open
// the launcher.
BASE_FEATURE(kShelfLauncherNudge,
             "ShelfLauncherNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the shelf party.
BASE_FEATURE(kShelfParty, "ShelfParty", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Shelf Palm Rejection in tablet mode by defining a pixel offset for
// the swipe gesture to show the extended hotseat. Limited to certain apps.
BASE_FEATURE(kShelfPalmRejectionSwipeOffset,
             "ShelfPalmRejectionSwipeOffset",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the new shimless rma flow.
BASE_FEATURE(kShimlessRMAFlow,
             "ShimlessRMAFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables launching Shimless RMA as a standalone app.
BASE_FEATURE(kShimlessRMAEnableStandalone,
             "ShimlessRMAEnableStandalone",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the OS update page in the shimless RMA flow.
BASE_FEATURE(kShimlessRMAOsUpdate,
             "ShimlessRMAOsUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the dark mode in the shimless RMA flow.
BASE_FEATURE(kShimlessRMADisableDarkMode,
             "ShimlessRMADisableDarkMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables enterprise policy control for SIM PIN Lock.
BASE_FEATURE(kSimLockPolicy, "SimLockPolicy", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Controls whether the snap group feature is enabled or not.
BASE_FEATURE(kSnapGroup, "SnapGroup", base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);
//
// Enables Terminal System App to load from Downloads for developer testing.
// Only works in dev and canary channels.
BASE_FEATURE(kTerminalDev, "TerminalDev", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables multi-profile theme support for Terminal..
BASE_FEATURE(kTerminalMultiProfile,
             "TerminalMultiProfile",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables SFTP / mount for Terminal..
BASE_FEATURE(kTerminalSftp, "TerminalSftp", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables tmux integration in the Terminal System App.
BASE_FEATURE(kTerminalTmuxIntegration,
             "TerminalTmuxIntegration",
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

// Enables the Office files upload workflow to improve Office files support.
BASE_FEATURE(kUploadOfficeToCloud,
             "UploadOfficeToCloud",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses new AuthFactor-based API when communicating with cryptohome.
// This feature flag also affects usage of AuthSessions in QuickUnlock, but
// only in case when cryptohome is used as backend.
// This feature flag also affects usage of AuthSession on lock screen.
BASE_FEATURE(kUseAuthFactors,
             "UseAuthFactors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, WebAuthN uses auth session based authentication
// instead of legacy CheckKey.
BASE_FEATURE(kUseAuthsessionForWebAuthN,
             "UseAuthsessionForWebAuthN",
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

// Enable or disable the ChromeOS video conferencing controls UI.
BASE_FEATURE(kVcControlsUi, "VcControlsUi", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable full screen wallpaper preview in new wallpaper experience.
BASE_FEATURE(kWallpaperFullScreenPreview,
             "WallpaperFullScreenPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables or disables PSM CheckIn for the first active device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientFirstActiveCheckIn,
             "DeviceActiveClientFirstActiveCheckIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for all time device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientFirstActiveCheckMembership,
             "DeviceActiveClientFirstActiveCheckMembership",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckIn for the monthly device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientMonthlyCheckIn,
             "DeviceActiveClientMonthlyCheckIn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for monthly device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClientMonthlyCheckMembership,
             "DeviceActiveClientMonthlyCheckMembership",
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

// Allows Files App to find and execute tasks using App Service for Arc and
// Guest OS apps.
BASE_FEATURE(kArcAndGuestOsFileTasksUseAppService,
             "ArcAndGuestOsFileTasksUseAppService",
             base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////

bool AreCaptureModeDemoToolsEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeDemoTools);
}

bool AreContextualNudgesEnabled() {
  if (!IsHideShelfControlsInTabletModeEnabled())
    return false;
  return base::FeatureList::IsEnabled(kContextualNudges);
}

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool ArePromiseIconsEnabled() {
  return base::FeatureList::IsEnabled(kPromiseIcons);
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

bool IsAvatarsCloudMigrationEnabled() {
  return base::FeatureList::IsEnabled(kAvatarsCloudMigration);
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

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
}

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsAmbientModePhotoPreviewEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModePhotoPreviewFeature);
}

bool IsAmbientModeThrottleAnimationEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeThrottleAnimation);
}

bool IsAmbientSubpageUIChangeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientSubpageUIChange);
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
  return base::FeatureList::IsEnabled(kArcInputOverlayBeta);
}

bool IsArcInputOverlayAlphaV2Enabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlayAlphaV2);
}

bool IsArcNetworkDiagnosticsButtonEnabled() {
  return IsNetworkingInDiagnosticsAppEnabled();
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

bool IsBentoBarEnabled() {
  return base::FeatureList::IsEnabled(kBentoBar);
}

bool IsBluetoothQualityReportEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothQualityReport);
}

bool IsCalendarViewEnabled() {
  return base::FeatureList::IsEnabled(kCalendarView);
}

bool IsCalendarModelDebugModeEnabled() {
  return base::FeatureList::IsEnabled(kCalendarModelDebugMode);
}

bool IsCalendarJellyEnabled() {
  return base::FeatureList::IsEnabled(kCalendarJelly);
}

bool IsCaptivePortalUI2022Enabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalUI2022);
}

bool IsCaptivePortalErrorPageEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalErrorPage);
}

bool IsCheckPasswordsAgainstCryptohomeHelperEnabled() {
  return base::FeatureList::IsEnabled(kCheckPasswordsAgainstCryptohomeHelper);
}

bool IsChromadAvailableEnabled() {
  return base::FeatureList::IsEnabled(kChromadAvailable);
}

bool IsClipboardHistoryNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryNudgeSessionReset);
}

bool IsClipboardHistoryRefreshEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryRefresh);
}

bool IsClipboardHistoryReorderEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryReorder);
}

bool IsDesksCloseAllEnabled() {
  return base::FeatureList::IsEnabled(kDesksCloseAll);
}

bool IsDnsOverHttpsWithIdentifiersReuseOldPolicyEnabled() {
  return base::FeatureList::IsEnabled(
      kDnsOverHttpsWithIdentifiersReuseOldPolicy);
}

bool IsDnsOverHttpsWithIdentifiersEnabled() {
  return base::FeatureList::IsEnabled(kDnsOverHttpsWithIdentifiers);
}

bool IsLauncherItemColorSyncEnabled() {
  return IsLauncherAppSortEnabled() &&
         base::FeatureList::IsEnabled(kLauncherItemColorSync);
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

bool IsCryptohomeRecoveryFlowEnabled() {
  return base::FeatureList::IsEnabled(kCryptohomeRecoveryFlow);
}

bool IsCryptohomeRecoveryFlowUIEnabled() {
  return base::FeatureList::IsEnabled(kCryptohomeRecoveryFlowUI);
}

bool IsCryptohomeRecoverySetupEnabled() {
  return base::FeatureList::IsEnabled(kCryptohomeRecoverySetup);
}

bool IsDarkLightModeEnabled() {
  return chromeos::features::IsDarkLightModeEnabled();
}

bool IsDarkLightModeKMeansColorEnabled() {
  return IsDarkLightModeEnabled() &&
         base::FeatureList::IsEnabled(kDarkLightModeKMeansColor);
}

bool IsDeprecateAssistantStylusFeaturesEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAssistantStylusFeatures);
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

bool IsDragUnpinnedAppToPinEnabled() {
  return base::FeatureList::IsEnabled(kDragUnpinnedAppToPin);
}

bool IsDragWindowToNewDeskEnabled() {
  return base::FeatureList::IsEnabled(kDragWindowToNewDesk);
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

bool IsFastPairSubsequentPairingUXEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSubsequentPairingUX);
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

bool IsFileManagerSearchV2Enabled() {
  return base::FeatureList::IsEnabled(kFilesSearchV2);
}

bool IsFirmwareUpdaterAppEnabled() {
  return base::FeatureList::IsEnabled(kFirmwareUpdaterApp);
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

bool AreGlanceablesEnabled() {
  return base::FeatureList::IsEnabled(kGlanceables);
}

bool IsHatsUseNewHistogramsEnabled() {
  return base::FeatureList::IsEnabled(kHatsUseNewHistograms);
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

bool IsHomeButtonWithTextEnabled() {
  return base::FeatureList::IsEnabled(kHomeButtonWithText);
}

bool IsHostnameSettingEnabled() {
  return base::FeatureList::IsEnabled(kEnableHostnameSetting);
}

bool IsHotspotEnabled() {
  return base::FeatureList::IsEnabled(kHotspot);
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
  return ShouldForceEnableServerSideSpeechRecognitionForDev() ||
         base::FeatureList::IsEnabled(kInternalServerSideSpeechRecognition);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsJellyEnabled() {
  return base::FeatureList::IsEnabled(kJelly);
}

bool IsJellyrollEnabled() {
  return base::FeatureList::IsEnabled(kJellyroll);
}

bool IsKeyboardBacklightToggleEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardBacklightToggle);
}

bool IsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(kHandwritingLegacyRecognition) ||
         base::FeatureList::IsEnabled(kHandwritingLegacyRecognitionAllLang);
}

bool IsLauncherAppSortEnabled() {
  return base::FeatureList::IsEnabled(kLauncherAppSort);
}

bool IsLauncherFolderRenameKeepsSortOrderEnabled() {
  return IsLauncherAppSortEnabled() &&
         base::FeatureList::IsEnabled(kLauncherFolderRenameKeepsSortOrder);
}

bool IsLauncherDismissButtonsOnSortNudgeAndToastEnabled() {
  return IsLauncherAppSortEnabled() &&
         base::FeatureList::IsEnabled(
             kLauncherDismissButtonsOnSortNudgeAndToast);
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

bool IsLogControllerForDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableLogControllerForDiagnosticsApp);
}

bool IsEducationEnrollmentOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kEducationEnrollmentOobeFlow);
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

bool IsManagedTermsOfServiceEnabled() {
  return base::FeatureList::IsEnabled(kManagedTermsOfService);
}

bool IsMicMuteNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kMicMuteNotifications);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsEcheLauncherEnabled() {
  return base::FeatureList::IsEnabled(kEcheLauncher) &&
         base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsNearbyKeepAliveFixEnabled() {
  return base::FeatureList::IsEnabled(kNearbyKeepAliveFix);
}

bool IsNetworkingInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableNetworkingInDiagnosticsApp);
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

bool IsNotificationScrollBarEnabled() {
  return base::FeatureList::IsEnabled(kNotificationScrollBar);
}

bool IsNotificationsInContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsInContextMenu);
}

bool IsNotificationsRefreshEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsRefresh);
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

bool IsOobeMaterialNextEnabled() {
  return IsJellyEnabled() && base::FeatureList::IsEnabled(kOobeMaterialNext);
}

bool IsOobeNetworkScreenSkipEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeNetworkScreenSkip);
}

bool IsOobeChoobeEnabled() {
  return base::FeatureList::IsEnabled(kOobeChoobe);
}

bool IsOobeConsolidatedConsentEnabled() {
  return base::FeatureList::IsEnabled(kOobeConsolidatedConsent);
}

bool IsOobeQuickStartEnabled() {
  return base::FeatureList::IsEnabled(kOobeQuickStart);
}

bool IsOobeRemoveShutdownButtonEnabled() {
  return base::FeatureList::IsEnabled(kOobeRemoveShutdownButton);
}

bool IsOobeThemeSelectionEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeThemeSelection);
}

bool IsOsSettingsAppBadgingToggleEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppBadgingToggle);
}

bool IsOsSettingsSearchFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsSearchFeedback);
}

bool IsOverviewDeskNavigationEnabled() {
  return base::FeatureList::IsEnabled(kOverviewDeskNavigation);
}

bool IsPcieBillboardNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPcieBillboardNotification);
}

bool IsPerDeskShelfEnabled() {
  return base::FeatureList::IsEnabled(kPerDeskShelf);
}

bool IsPhoneHubCameraRollEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCameraRoll);
}

bool IsPhoneHubMonochromeNotificationIconsEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubMonochromeNotificationIcons);
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

bool IsPrivacyIndicatorsEnabled() {
  return base::FeatureList::IsEnabled(kPrivacyIndicators);
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

bool IsProjectorAnnotatorEnabled() {
  return IsProjectorEnabled() &&
         base::FeatureList::IsEnabled(kProjectorAnnotator);
}

bool IsProjectorAppDebugMode() {
  return base::FeatureList::IsEnabled(kProjectorAppDebug);
}

bool IsProjectorExcludeTranscriptEnabled() {
  return base::FeatureList::IsEnabled(kProjectorExcludeTranscript);
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
  return base::FeatureList::IsEnabled(kQsRevamp) &&
         base::FeatureList::IsEnabled(kQsRevampWip);
}

bool IsProjectorViewerUseSecondaryAccountEnabled() {
  return base::FeatureList::IsEnabled(kProjectorViewerUseSecondaryAccount);
}

bool IsProjectorAccountSwitchNotificationEnabled() {
  return base::FeatureList::IsEnabled(kProjectorAccountSwitchNotification);
}

bool IsQuickDimEnabled() {
  return base::FeatureList::IsEnabled(kQuickDim) && switches::HasHps();
}

bool IsQuickSettingsNetworkRevampEnabled() {
  return base::FeatureList::IsEnabled(kQuickSettingsNetworkRevamp);
}

bool IsPerDeskZOrderEnabled() {
  return base::FeatureList::IsEnabled(kEnablePerDeskZOrder);
}

bool IsReleaseTrackUiEnabled() {
  return base::FeatureList::IsEnabled(kReleaseTrackUi);
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

bool IsSavedDesksEnabled() {
  return base::FeatureList::IsEnabled(kEnableSavedDesks);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
}

bool IsSettingsAppNotificationSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSettingsAppNotificationSettings);
}

bool IsSettingsAppThemeChangeAnimationEnabled() {
  return IsDarkLightModeEnabled() &&
         base::FeatureList::IsEnabled(kSettingsAppThemeChangeAnimation);
}

bool IsShelfLauncherNudgeEnabled() {
  return base::FeatureList::IsEnabled(kShelfLauncherNudge);
}

bool IsShelfPalmRejectionSwipeOffsetEnabled() {
  return base::FeatureList::IsEnabled(kShelfPalmRejectionSwipeOffset);
}

bool IsShimlessRMAFlowEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAFlow);
}

bool IsShimlessRMAStandaloneAppEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAEnableStandalone) &&
         IsShimlessRMAFlowEnabled();
}

bool IsShimlessRMAOsUpdateEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAOsUpdate);
}

bool IsShimlessRMADarkModeDisabled() {
  return base::FeatureList::IsEnabled(kShimlessRMADisableDarkMode);
}

bool IsSimLockPolicyEnabled() {
  return base::FeatureList::IsEnabled(kSimLockPolicy);
}

bool IsSnapGroupEnabled() {
  return base::FeatureList::IsEnabled(kSnapGroup);
}

bool IsSystemTrayShadowEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayShadow);
}

bool IsStylusBatteryStatusEnabled() {
  return base::FeatureList::IsEnabled(kStylusBatteryStatus);
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

bool IsUploadOfficeToCloudEnabled() {
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud);
}

bool IsUseAuthFactorsEnabled() {
  return base::FeatureList::IsEnabled(kUseAuthFactors);
}

bool IsUseAuthsessionForWebAuthNEnabled() {
  return base::FeatureList::IsEnabled(kUseAuthsessionForWebAuthN);
}

bool IsUseLoginShelfWidgetEnabled() {
  return base::FeatureList::IsEnabled(kUseLoginShelfWidget);
}

bool IsUseStorkSmdsServerAddressEnabled() {
  return base::FeatureList::IsEnabled(kUseStorkSmdsServerAddress);
}

bool IsVCBackgroundBlurEnabled() {
  return base::FeatureList::IsEnabled(kVCBackgroundBlur);
}

bool IsVCBackgroundReplaceEnabled() {
  return base::FeatureList::IsEnabled(kVCBackgroundReplace);
}

bool IsVCPortraitRelightingEnabled() {
  return base::FeatureList::IsEnabled(kVCPortraitRelighting);
}

bool IsVcControlsUiEnabled() {
  return base::FeatureList::IsEnabled(kVcControlsUi);
}

bool IsViewPpdEnabled() {
  return base::FeatureList::IsEnabled(kEnableViewPpd);
}

bool IsWallpaperFastRefreshEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperFastRefresh);
}

bool IsWallpaperFullScreenPreviewEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperFullScreenPreview);
}

bool IsWallpaperPerDeskEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperPerDesk);
}

bool IsWebUITabStripTabDragIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kWebUITabStripTabDragIntegration);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
}

bool IsWmModeEnabled() {
  return base::FeatureList::IsEnabled(kWmMode);
}

bool ShouldArcAndGuestOsFileTasksUseAppService() {
  return base::FeatureList::IsEnabled(kArcAndGuestOsFileTasksUseAppService);
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

namespace {

// The boolean flag indicating if "WebUITabStrip" feature is enabled in Chrome.
bool g_webui_tab_strip_enabled = false;

}  // namespace

void SetWebUITabStripEnabled(bool enabled) {
  g_webui_tab_strip_enabled = enabled;
}

bool IsWebUITabStripEnabled() {
  return g_webui_tab_strip_enabled;
}

}  // namespace features
}  // namespace ash
