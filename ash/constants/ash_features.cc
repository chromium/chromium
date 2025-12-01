// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "ash/constants/ash_switches.h"
#include "ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chromeos/components/libsegmentation/buildflags.h"
#include "chromeos/constants/chromeos_features.h"

#if defined(ARCH_CPU_ARM_FAMILY)
#include "base/command_line.h"
#endif  // defined(ARCH_CPU_ARM_FAMILY)

namespace ash::features {
// Enables the UI for additional on-device parental controls that can be used to
// enable or block ARC++ apps.
BASE_FEATURE(kOnDeviceAppControls, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
BASE_FEATURE(kAllowAmbientEQ, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Cross-Device features, e.g. Nearby Share, Smart Lock, Fast Pair,
// Quick Start, etc. This flag is used to disable Cross-Device on platforms
// where we cannot yet guarantee a good experience with the stock Bluetooth
// hardware (e.g. Reven / ChromeOS Flex). Access through
// IsCrossDeviceFeatureSuiteAllowed().
BASE_FEATURE(kAllowCrossDeviceFeatureSuite, base::FEATURE_ENABLED_BY_DEFAULT);

// Always reinstall system web apps, instead of only doing so after version
// upgrade or locale changes.
BASE_FEATURE(kAlwaysReinstallSystemWebApps,
             "ReinstallSystemWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAltClickAndSixPackCustomization,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature changes the default setting of Ambient EQ to off. This feature
// has no effect if `kAllowAmbientEQ` is not also enabled.
BASE_FEATURE(kAmbientEQDefaultOff, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to allow Dev channel to use Prod server feature.
BASE_FEATURE(kAmbientModeDevUseProdFeature,
             "ChromeOSAmbientModeDevChannelUseProdServer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds support for allowing or disabling APN modification by policy.
BASE_FEATURE(kAllowApnModificationPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the annotator feature is enabled in ChromeOS.
BASE_FEATURE(kAnnotatorMode, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApnRevamp, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to defer loading of active tabs of background (occluded)
// browser windows during session restore.
BASE_FEATURE(kAshSessionRestoreDeferOccludedActiveTabLoad,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable assistive multi word suggestions.
BASE_FEATURE(kAssistMultiWord, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables audio I/O selection improvement algorithm. http://launch/4301655.
BASE_FEATURE(kAudioSelectionImprovement, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Auto Night Light feature which sets the default schedule type to
// sunset-to-sunrise until the user changes it to something else. This feature
// is not exposed to the end user, and is enabled only via cros_config for
// certain devices.
BASE_FEATURE(kAutoNightLight, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
BASE_FEATURE(kAutoScreenBrightness, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a setting to automatically sign out a user when their account signs
// in on a new device.
BASE_FEATURE(kAutoSignOut, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables params tuning experiment for autocorrect on ChromeOS.
BASE_FEATURE(kAutocorrectParamsTuning, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a toggle for enabling autocorrect on ChromeOS.
BASE_FEATURE(kAutocorrectByDefault, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the autozoom nudge shown prefs will be reset at the start of
// each new user session.
BASE_FEATURE(kAutozoomNudgeSessionReset, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a settings option to set an explicit charge limit for Chromebooks.
BASE_FEATURE(kBatteryChargeLimit,
             "CrosBatteryChargeLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Make Battery Saver available.
BASE_FEATURE(kBatterySaver,
             "CrosBatterySaver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determines if BabelOrca captions are available.
BASE_FEATURE(kBabelOrca, base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables or disables the Bluetooth Quality Report feature.
BASE_FEATURE(kBluetoothQualityReport, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables entire Boca feature on ChromeOS. Use as kill switch.
BASE_FEATURE(kBocaUber, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Boca feature on ChromeOS
BASE_FEATURE(kBoca, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Boca consumer user experience on ChromeOS.
BASE_FEATURE(kBocaConsumer, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Boca custom polling interval on ChromeOS.
BASE_FEATURE(kBocaCustomPolling, base::FEATURE_ENABLED_BY_DEFAULT);

// Time interval to do indefinite session polling.
constexpr base::FeatureParam<base::TimeDelta>
    kBocaIndefinitePeriodicJobIntervalInSeconds{
        &kBocaCustomPolling, "IndefinitePollingIntervalInSeconds",
        base::Seconds(0)};

// Time interval to do session polling within session
constexpr base::FeatureParam<base::TimeDelta>
    kBocaInSessionPeriodicJobIntervalInSeconds{
        &kBocaCustomPolling, "InSessionPollingIntervalInSeconds",
        base::Seconds(60)};

// Enables or disables OnTask status check.
BASE_FEATURE(kOnTaskStatusCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Custom boca receiver polling time interval.
const base::FeatureParam<base::TimeDelta> kOnTaskStatusCheckInterval{
    &kOnTaskStatusCheck, "OnTaskStatusCheckInterval", base::Seconds(60)};

// Enables or disables locked quiz migration to leverage the OnTask SWA.
BASE_FEATURE(kBocaOnTaskLockedQuizMigration, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Boca OnTask pod on ChromeOS.
BASE_FEATURE(kBocaOnTaskPod, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables unmute browser tabs when unlock Boca.
BASE_FEATURE(kBocaOnTaskUnmuteBrowserTabsOnUnlock,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Boca OnTask enter locked mode countdown duration on
// ChromeOS.
BASE_FEATURE(kBocaLockedModeCustomCountdownDuration,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Time duration for entering locked mode countdown.
constexpr base::FeatureParam<base::TimeDelta>
    kBocaLockedModeCountdownDurationInSeconds{
        &kBocaLockedModeCustomCountdownDuration,
        "BocaLockedModeCountdownDurationInSeconds", base::Seconds(5)};

// Enables or disables Boca sending student heartbeat requests on ChromeOS.
BASE_FEATURE(kBocaStudentHeartbeat, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Boca student heartbeat custom interval on ChromeOS.
BASE_FEATURE(kBocaStudentHeartbeatCustomInterval,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Time interval to do student heartbeat
constexpr base::FeatureParam<base::TimeDelta>
    kBocaStudentHeartbeatPeriodicJobIntervalInSeconds{
        &kBocaStudentHeartbeatCustomInterval,
        "StudentHeartbeatPeriodicJobIntervalInSeconds", base::Seconds(30)};

// Enables or disables Spotlight for Boca on ChromeOS.
BASE_FEATURE(kBocaSpotlight, base::FEATURE_ENABLED_BY_DEFAULT);

// The url to use when connecting to spotlight
constexpr base::FeatureParam<std::string> kBocaSpotlightUrlTemplate{
    &kBocaSpotlight, "spotlight-url-template",
    "https://remotedesktop.google.com/support/session/{sessionCode}"};

// Enables or disables Boca network restriction for Boca on ChromeOS.
BASE_FEATURE(kBocaNetworkRestriction, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables using a specific app name for speech recognition for Boca
// on ChromeOS.
BASE_FEATURE(kBocaClientTypeForSpeechRecognition,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables using a specific app name for speech recognition for Boca
// on ChromeOS.
BASE_FEATURE(kBocaAdjustCaptionBubbleOnExpand,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables keeping the Boca SWA open when the session is ended.
BASE_FEATURE(kBocaKeepSWAOpenOnSessionEnded, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables enforcing sequential execution for Boca Session load.
BASE_FEATURE(kBocaSequentialSessionLoad, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the updated lock / pause ui for boca.
BASE_FEATURE(kBocaLockPauseUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the updated nav settings ui for boca.
BASE_FEATURE(kBocaNavSettingsDialog, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new caption toggle button for boca.
BASE_FEATURE(kBocaCaptionToggle, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables using the native ChromeOS implementation of the CRD
// client for Spotlight within the Boca SWA.
BASE_FEATURE(kBocaSpotlightRobotRequester, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables enforcing sequential execution for Boca insert activity.
BASE_FEATURE(kBocaSequentialInsertActivity, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables translation toggle for caption bubble in the context of
// boca.
BASE_FEATURE(kBocaTranslateToggle, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables migration to `BabelOrcaSpeechRecognizerClient` for
// speech recognition.
BASE_FEATURE(kBocaMigrateSpeechRecongnizerClient,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Boca receiver app.
BASE_FEATURE(kBocaReceiverApp, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables using a configured number of students.
BASE_FEATURE(kBocaConfigureMaxStudents, base::FEATURE_DISABLED_BY_DEFAULT);

// The maximum number of students allowed in a class.
constexpr base::FeatureParam<int> kBocaMaxNumStudentsAllowed{
    &kBocaConfigureMaxStudents, "BocaMaxNumStudentsAllowed", 100};

// Enables or disables use of the courseWorkMaterials API in the Boca app.
BASE_FEATURE(kBocaCourseWorkMaterialApi, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables sharing teacher's screen in the Boca app.
BASE_FEATURE(kBocaScreenSharingTeacher, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables sharing student's screen in the Boca app.
BASE_FEATURE(kBocaScreenSharingStudent, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables sharing host audio in the Boca app.
BASE_FEATURE(kBocaHostAudio, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables using audio for the Kiosk client in the Boca app.
BASE_FEATURE(kBocaAudioForKiosk, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables setting audio params when sharing from a student device
// to a remote kiosk receiver.
BASE_FEATURE(kBocaRedirectStudentAudioToKiosk,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Boca receiver custom polling.
BASE_FEATURE(kBocaReceiverCustomPolling, base::FEATURE_DISABLED_BY_DEFAULT);

// Custom boca receiver polling time interval.
const base::FeatureParam<base::TimeDelta> kBocaReceiverCustomPollingInterval{
    &kBocaReceiverCustomPolling, "BocaReceiverCustomPollingInterval",
    base::Seconds(10)};

// Max number of consecutive polling failures to end receiver session.
const base::FeatureParam<int> kBocaReceiverCustomPollingMaxFailuresCount{
    &kBocaReceiverCustomPolling, "BocaReceiverCustomPollingMaxFailuresCount",
    3};

BASE_FEATURE(kCrosSwitcher, base::FEATURE_ENABLED_BY_DEFAULT);

// Indicates whether the camera super resolution is supported. Note that this
// feature is overridden by login_manager based on whether a per-board build
// sets the USE camera_feature_super_res flag. Refer to:
// chromiumos/src/platform2/login_manager/chrome_setup.cc
BASE_FEATURE(kCameraSuperResSupported, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Big GL when using Borealis.
BASE_FEATURE(kBorealisBigGl, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable dGPU when using Borealis.
BASE_FEATURE(kBorealisDGPU, base::FEATURE_ENABLED_BY_DEFAULT);

// Bypass some hardware checks when deciding whether to block/allow borealis.
BASE_FEATURE(kBorealisEnableUnsupportedHardware,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force the steam client to be on its beta version. If not set, the client will
// be on its stable version.
BASE_FEATURE(kBorealisForceBetaClient, base::FEATURE_DISABLED_BY_DEFAULT);

// Force the steam client to render in 2x size (using GDK_SCALE as discussed in
// b/171935238#comment4).
BASE_FEATURE(kBorealisForceDoubleScale, base::FEATURE_DISABLED_BY_DEFAULT);

// Prevent the steam client from exercising ChromeOS integrations, in this mode
// it functions more like the linux client.
BASE_FEATURE(kBorealisLinuxMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable borealis on this device. This won't necessarily allow it, since you
// might fail subsequent checks.
BASE_FEATURE(kBorealisPermitted, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the "provision" flag when mounting Borealis' stateful disk.
// TODO(b/288361720): This is temporary while we test the 'provision'
// mount option. Once we're satisfied things are stable, we'll make this
// the default and remove this feature/flag.
BASE_FEATURE(kBorealisProvision, base::FEATURE_DISABLED_BY_DEFAULT);

// Disable use of calculated scale for -forcedesktopscaling on Steam client.
// Scale will default to a value of 1.
BASE_FEATURE(kBorealisScaleClientByDPI, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBorealisZinkGlDriver, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows UserDataAuth client to use fingerprint auth factor.
BASE_FEATURE(kFingerprintAuthFactor, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<BorealisZinkGlDriverParam>::Option
    borealis_zink_gl_driver_options[] = {
        {BorealisZinkGlDriverParam::kZinkEnableRecommended,
         "ZinkEnableRecommended"},
        {BorealisZinkGlDriverParam::kZinkEnableAll, "ZinkEnableAll"}};
const base::FeatureParam<BorealisZinkGlDriverParam> kBorealisZinkGlDriverParam{
    &kBorealisZinkGlDriver, "BorealisZinkGlDriverParam",
    BorealisZinkGlDriverParam::kZinkEnableRecommended,
    &borealis_zink_gl_driver_options};

// Enables the feature to parameterize glyph for "Campbell" feature.
BASE_FEATURE(kCampbellGlyph, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the nudges/tutorials that inform users of the screen capture keyboard
// shortcut and feature tile.
BASE_FEATURE(kCaptureModeEducation, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(hewer): Remove the unused paths after at least one milestone after
// Capture Mode Education has been enabled by default.
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables on-device OCR functionality in capture mode, used as part of the
// Scanner and Sunfish features.
BASE_FEATURE(kCaptureModeOnDeviceOcr, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, allow eSIM installation bypass the non-cellular internet
// connectivity check.
BASE_FEATURE(kCellularBypassESimInstallationConnectivityCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use second the Euicc that is exposed by Hermes in Cellular Setup
// and Settings.
BASE_FEATURE(kCellularUseSecondEuicc, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Multiple scraped passwords should be checked against password in
// cryptohome.
BASE_FEATURE(kCheckPasswordsAgainstCryptohomeHelper,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled alongside the keyboard auto-repeat setting, holding down Ctrl+V
// will cause the clipboard history menu to show. From there, the user can
// select a clipboard history item to replace the initially pasted content.

// Controls enabling/disabling conch.
BASE_FEATURE(kConch, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, conch will provide transcription language options for users to
// choose.
BASE_FEATURE(kConchExpandTranscriptionLanguage,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, conch will provide available GenAI features.
BASE_FEATURE(kConchGenAi, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, conch will request DLC to download large models. Otherwise,
// request DLC to download small models. Note that if requested models are not
// available on the device, GenAI features will be unavailable.
BASE_FEATURE(kConchLargeModel, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, conch will use microphone to capture system audio.
BASE_FEATURE(kConchSystemAudioFromMic, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a smooth overview mode transition based on the gesture position.
BASE_FEATURE(kContinuousOverviewScrollAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling/disabling the coral feature.
BASE_FEATURE(kCoralFeature, base::FEATURE_DISABLED_BY_DEFAULT);

// Since kCoralFeature is also controlled by login_manager, finch kill switch
// could not effectively control it. The kCoralFeatureAllowed is designed to be
// always enabled by default, but can be disabled by the finch kill switch. When
// disabled, this overrides kCoralFeature's status and force disables the
// feature.
BASE_FEATURE(kCoralFeatureAllowed, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the coral feature supports multi-language.
BASE_FEATURE(kCoralFeatureMultiLanguage, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables execution of routine for copying client keys and certs from NSS DB to
// software backed Chaps slot. It's only respected if the
// EnableNssDbClientCertsRollback feature flag is disabled.
BASE_FEATURE(kCopyClientKeysCertsToChaps, base::FEATURE_ENABLED_BY_DEFAULT);

// Adds location access control to Privacy Hub.
BASE_FEATURE(kCrosPrivacyHub, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, ChromeOS system services and Chrome-on-ChromeOS will use separate
// API keys for Geolocation resolution.
BASE_FEATURE(kCrosSeparateGeoApiKey, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables cros safety service for trust and safety filtering for the text/image
// output of on-device gen ai models.
BASE_FEATURE(kCrosSafetyService, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables syncing attestation certificates to cryptauth for use by Cross Device
// features, including Eche and Phone Hub.
BASE_FEATURE(kCryptauthAttestationSyncing, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental containerless Crostini VMs.
BASE_FEATURE(kCrostiniContainerless, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini GPU support.
// Note that this feature can be overridden by login_manager based on
// whether a per-board build sets the USE virtio_gpu flag.
// Refer to: chromiumos/src/platform2/login_manager/chrome_setup.cc
BASE_FEATURE(kCrostiniGpuSupport, base::FEATURE_DISABLED_BY_DEFAULT);

// Force enable recreating the LXD DB at LXD launch.
BASE_FEATURE(kCrostiniResetLxdDb, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental UI creating and managing multiple Crostini containers.
BASE_FEATURE(kCrostiniMultiContainer, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Qt application IME support.
BASE_FEATURE(kCrostiniQtImeSupport, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Crostini Virtual Keyboard support.
BASE_FEATURE(kCrostiniVirtualKeyboardSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
BASE_FEATURE(kCryptAuthV2DeviceActivityStatus,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables use of the connectivity status from Cryptauth's
// GetDevicesActivityStatus API to sort devices.
BASE_FEATURE(kCryptAuthV2DeviceActivityStatusUseConnectivity,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disable a Files banner about Google One offer. This flag is used by G1+
// nudge to conditionally disable the G1 file banner via finch.
BASE_FEATURE(kDisableGoogleOneOfferFilesBanner,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the default value for the option to set up
// cryptohome recovery presented for consumer users.
// - if enabled, recovery would set up by default (opt-out mode)
// - if disabled, user have to explicitly opt-in to use recovery
BASE_FEATURE(kCryptohomeRecoveryByDefaultForConsumers,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the behavior during onboarding when the RecoveryFactorBehavior
// policy is unset.
// - if enabled, treat as "recommended enable recovery" policy value.
// - if disabled, treat as "recommended disable recovery" policy value.
BASE_FEATURE(kCryptohomeRecoveryByDefaultForEnterprise,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether should set demo app window's parent to
// `kShellWindowId_AlwaysOnTopWallpaperContainer` when attract loop is playing.
BASE_FEATURE(kDemoModeAppResetWindowContainer,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether use a demo account (consumer account) to login Demo mode
// session.
BASE_FEATURE(kDemoModeSignIn, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether demo mode applies CBX wallpaper logic.
BASE_FEATURE(kDemoModeWallpaperUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether clean up local files between shopper session when demo mode
// sign in is enable. No-op if demo mode sign in is disabled.
BASE_FEATURE(kDemoModeSignInFileCleanup, base::FEATURE_ENABLED_BY_DEFAULT);

// Toggle different display features based on user setting and power state
BASE_FEATURE(kDisplayPerformanceMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the do not disturb shortcut.
BASE_FEATURE(kDoNotDisturbShortcut, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Sync for desk templates on ChromeOS.
BASE_FEATURE(kDeskTemplateSync, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDesksTemplates, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables diacritics on longpress on the physical keyboard by default.
BASE_FEATURE(kDiacriticsOnPhysicalKeyboardLongpressDefaultOn,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables hardware requirement checks for Bruschetta installer, allowing for
// more easy development against changes of said requirements.
BASE_FEATURE(kDisableBruschettaInstallChecks,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables the DNS proxy service for ChromeOS.
BASE_FEATURE(kDisableDnsProxy, base::FEATURE_DISABLED_BY_DEFAULT);

// Disconnect WiFi when the device get connected to Ethernet.
BASE_FEATURE(kDisconnectWiFiOnEthernetConnected,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables indicators to hint where displays are connected.
BASE_FEATURE(kDisplayAlignAssist, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, DriveFS will be used for Drive sync.
BASE_FEATURE(kDriveFs, "DriveFS", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables DriveFS' experimental local files mirroring functionality.
BASE_FEATURE(kDriveFsMirroring, base::FEATURE_DISABLED_BY_DEFAULT);

// Carries DriveFS' bulk-pinning experimental parameters.
BASE_FEATURE(kDriveFsBulkPinningExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables DriveFS' bulk pinning functionality. This flag is to be enabled by
// the feature management module.
BASE_FEATURE(kFeatureManagementDriveFsBulkPinning,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables authenticating to Wi-Fi networks using EAP-GTC.
BASE_FEATURE(kEapGtcWifiAuthentication, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the System Web App (SWA) version of Eche.
BASE_FEATURE(kEcheSWA, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Debug Mode of Eche.
BASE_FEATURE(kEcheSWADebugMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the E2E latecny measurement of Eche.
BASE_FEATURE(kEcheSWAMeasureLatency, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending start signaling to establish Eche's WebRTC connection.
BASE_FEATURE(kEcheSWASendStartSignaling, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows disabling the stun servers when establishing a WebRTC connection to
// Eche.
BASE_FEATURE(kEcheSWADisableStunServer, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows CrOS to analyze Android
// network information to provide more context on connection errors.
BASE_FEATURE(kEcheSWACheckAndroidNetworkInfo, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables settings to control internal display brightness and auto-brightness.
BASE_FEATURE(kEnableBrightnessControlInSettings,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables external keyboard testers in the diagnostics app.
BASE_FEATURE(kEnableExternalKeyboardsInDiagnostics,
             "EnableExternalKeyboardsInDiagnosticsApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the device hostname.
BASE_FEATURE(kEnableHostnameSetting, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OAuth support when printing via the IPP protocol.
BASE_FEATURE(kEnableOAuthIpp, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables RFC8925 (prefer IPv6-only on an IPv6-only-capable network).
BASE_FEATURE(kEnableRFC8925, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the DNS proxy service running in root network namespace for ChromeOS.
BASE_FEATURE(kEnableRootNsDnsProxy, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the shortcut to toggle whether the camera is enabled/disabled in
// Settings > Privacy controls.
BASE_FEATURE(kEnableToggleCameraShortcut, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, touchscreen mapping experience is visible in settings.
BASE_FEATURE(kEnableTouchscreenMappingExperience,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, touchpad cards will be shown in the diagnostics app's input
// section.
BASE_FEATURE(kEnableTouchpadsInDiagnosticsApp,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, touchscreen cards will be shown in the diagnostics app's input
// section.
BASE_FEATURE(kEnableTouchscreensInDiagnosticsApp,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, touchscreen calibration will be shown in settings.
BASE_FEATURE(kEnableTouchscreenCalibration, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables rollback routine which will delete client keys and certificates
// from the software backed Chaps storage. Copies of keys and certificates will
// will continue to exist in NSS DB.
BASE_FEATURE(kEnableNssDbClientCertsRollback,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables WiFi QoS to detect and prioritize selected egress network traffic
// using WiFi QoS/WMM in congested WiFi environments.
BASE_FEATURE(kEnableWifiQos, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables WiFi QoS to detect and prioritize selected egress network traffic
// using WiFi QoS/WMM in congested WiFi environments. For an Enterprise enrolled
// device:
// - If this flag is enabled, the feature will be controlled by EnableWifiQos;
// - If this flag is disabled, the feature will be disabled.
BASE_FEATURE(kEnableWifiQosEnterprise, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables entering overview mode by clicking the wallpaper with the mouse.
BASE_FEATURE(kEnterOverviewFromWallpaper, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to the chrome://enterprise-reporting WebUI.
BASE_FEATURE(kEnterpriseReportingUI, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ephemeral network configuration policies are respected.
BASE_FEATURE(kEphemeralNetworkPolicies,
             "kEphemeralNetworkPolicies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control whether the eSIM activation dialog supports submitting an empty code.
BASE_FEATURE(kESimEmptyActivationCodeSupported,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
BASE_FEATURE(kExoOrdinalMotion, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows RGB Keyboard to test new animations/patterns.
BASE_FEATURE(kExperimentalRgbKeyboardPatterns,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables extended updates opt-in functionality.
BASE_FEATURE(kExtendedUpdatesOptInFeature, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Fast Pair feature.
BASE_FEATURE(kFastPair, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the usage of the 2025 format for Fast Pair advertisements.
BASE_FEATURE(kFastPairAdvertisingFormat2025, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables logic for handling BLE address rotations during retroactive pair
// scenarios.
BASE_FEATURE(kFastPairBleRotation, base::FEATURE_ENABLED_BY_DEFAULT);

// Sets mode to DEBUG when fetching metadata from the Nearby server, allowing
// debug devices to trigger Fast Pair notifications.
BASE_FEATURE(kFastPairDebugMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using longterm Handshake retry logic for Fast Pair.
BASE_FEATURE(kFastPairHandshakeLongTermRefactor,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables prototype support for Fast Pair for keyboards.
BASE_FEATURE(kFastPairKeyboards, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Saved Devices nicknames logic for Fast Pair.
BASE_FEATURE(kFastPairSavedDevicesNicknames, base::FEATURE_ENABLED_BY_DEFAULT);

// The amount of minutes we should wait before allowing notifications for a
// recently lost device.
const base::FeatureParam<double> kFastPairDeviceLostNotificationTimeoutMinutes{
    &kFastPair, "fast-pair-device-lost-notification-timeout-minutes", 5};

// Enables link to Progressive Web Application companion app to configure
// Pixel Buds after Fast Pair.
BASE_FEATURE(kFastPairPwaCompanion, base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kFastPairSavedDevices, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the "Saved Devices" Fast Pair strict interpretation of opt-in status,
// meaning that a user's preferences determine if retroactive pairing and
// subsequent pairing scenarios are enabled.
BASE_FEATURE(kFastPairSavedDevicesStrictOptIn,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows the creation of up to 16 desks (default is 8). This flag
// is intended to be controlled by the feature management module.
BASE_FEATURE(kFeatureManagement16Desks, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows borealis on certain boards whose features are determined by
// FeatureManagement. This feature does not apply to all boards, and does not
// guarantee borealis will be available (due to additional hardware checks).
BASE_FEATURE(kFeatureManagementBorealis, base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts GenAi features in Conch to the intended target population, while
// the `kConchGenAi` flag controls the feature's rollout within said target
// population. This flag is only intended to be modified by the
// feature_management module.
BASE_FEATURE(kFeatureManagementConchGenAi, base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts some content in the Help app to the intended target population.
// This flag is only intended to be modified by the feature management module.
BASE_FEATURE(kFeatureManagementShowoff, base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts the time-of-day wallpaper/screensaver features to the intended
// target population, whereas the `kTimeOfDayScreenSaver|Wallpaper` flags
// control the feature's rollout within said target population. These flags are
// only intended to be modified by the feature_management module.
BASE_FEATURE(kFeatureManagementTimeOfDayScreenSaver,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureManagementTimeOfDayWallpaper,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated service. If enabled, launches federated service when
// user first login.
BASE_FEATURE(kFederatedService, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the federated strings service.
BASE_FEATURE(kFederatedStringsService, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the federated strings service to schedule tasks.
BASE_FEATURE(kFederatedStringsServiceScheduleTasks,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables scheduling of launcher query federated analytics version 2 tasks.
BASE_FEATURE(kFederatedLauncherQueryAnalyticsVersion2Task,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the files transfer conflict dialog in Files app.
BASE_FEATURE(kFilesConflictDialog, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables local image search by query in the Files app.
BASE_FEATURE(kFilesLocalImageSearch, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables materialized views in Files App.
BASE_FEATURE(kFilesMaterializedViews, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partitioning of removable disks in file manager.
BASE_FEATURE(kFilesSinglePartitionFormat, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable background cleanup for old files in Trash.
BASE_FEATURE(kFilesTrashAutoCleanup, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable files app trash for Drive.
BASE_FEATURE(kFilesTrashDrive, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the v2 version of the Firmware Updates app.
BASE_FEATURE(kFirmwareUpdateUIV2, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if the Fjord variant of OOBE is shown.
BASE_FEATURE(kFjordOobe, base::FEATURE_DISABLED_BY_DEFAULT);

// Force flag for the Fjord variant of OOBE. This is to make testing easier
// because the Fjord OOBE variant is buildflag dependent.
BASE_FEATURE(kFjordOobeForceEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Flex Auto-Enrollment feature on ChromeOS
BASE_FEATURE(kFlexAutoEnrollment, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables firmware updates from LVFS for ChromeOS Flex.
BASE_FEATURE(kFlexFirmwareUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls Floating SSO feature which can move cookies between ChromeOS
// enterprise devices. The feature is also guarded by an enterprise policy. This
// flag controls if we are allowed to launch the service observing the policy
// and if we show the user selectable UI when the policy is enabled.
BASE_FEATURE(kFloatingSso, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, makes the Projector app use server side speech
// recognition instead of on-device speech recognition.
BASE_FEATURE(kForceEnableServerSideSpeechRecognition,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force enables on-device apps controls regardless of the device region.
// Used for development and testing only. Should remain disabled by default.
// See `kOnDeviceAppControls` description for the feature details.
BASE_FEATURE(kForceOnDeviceAppControlsForAllRegions,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling/disabling the forest feature.
// For more info, see go/crosforest.
BASE_FEATURE(kForestFeature, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, there will be an alert bubble showing up when the device
// returns from low brightness (e.g., sleep, closed cover) without a lock screen
// and the active window is in fullscreen.
// TODO(crbug.com/40140761): Remove this after the feature is launched.
BASE_FEATURE(kFullscreenAlertBubble,
             "EnableFullscreenBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Debugging UI for ChromeOS FuseBox service.
BASE_FEATURE(kFuseBoxDebug, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the fwupd dbus client should be active. This is used only
// for testing to prevent the fwupd service from spooling and re-activating
// powerd service.
BASE_FEATURE(kBlockFwupdClient, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Classroom Student Glanceable on time management surface.
BASE_FEATURE(kGlanceablesTimeManagementClassroomStudentView,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Tasks Glanceable on time management surface.
BASE_FEATURE(kGlanceablesTimeManagementTasksView,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fetching assigned (shared) tasks for Google Tasks integration.
BASE_FEATURE(kGlanceablesTimeManagementTasksViewAssignedTasks,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables logging new Gaia account creation event.
BASE_FEATURE(kGaiaRecordAccountCreation, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Gamepad Support.
BASE_FEATURE(kGameDashboardGamepadSupport, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Game Dashboard Main Menu utility views.
BASE_FEATURE(kGameDashboardUtilities, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the App launch keyboard shortcut.
BASE_FEATURE(kAppLaunchShortcut, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Game Dashboard's Record Game feature. This flag is to be enabled
// by the feature management module.
BASE_FEATURE(kFeatureManagementGameDashboardRecordGame,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls gamepad vibration in Exo.
BASE_FEATURE(kGamepadVibration,
             "ExoGamepadVibration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable a D-Bus service for accessing gesture properties.
BASE_FEATURE(kGesturePropertiesDBusService, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Graduation app for EDU users if the Graduation policy allows it.
BASE_FEATURE(kGraduation, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the Graduation app will use a webview-specific endpoint to
// load the Takeout Transfer tool.
BASE_FEATURE(kGraduationUseEmbeddedTransferEndpoint,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a Files banner about Google One offer. This flag is used by Gamgee
// nudge to conditionally disable the G1 file banner for CBX boards via finch.
BASE_FEATURE(kGoogleOneOfferFilesBanner, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables targeting for feature-aware devices, as controlled by the feature
// management module.
BASE_FEATURE(kFeatureManagementGrowthFramework,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables growth framework.
BASE_FEATURE(kGrowthFramework, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable set app window as nudge parent.
BASE_FEATURE(kGrowthCampaignsNudgeParentToAppWindow,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables CrOS events recording with growth campaigns.
BASE_FEATURE(kGrowthCampaignsCrOSEvents, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables demo mode sign in with growth campaigns.
BASE_FEATURE(kGrowthCampaignsDemoModeSignIn, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns experiment tag targeting is enabled. The
// flag also used by finch to tag the session with finch params.
BASE_FEATURE(kGrowthCampaignsExperimentTagTargeting,
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of predefined Growth Framework experiment flag that will be associated
// with a finch study to deliver finch param for each experiment group to
// create randomization group that match the experiment tag targeting in
// Growth campaigns.
// The group will be selected by `predefinedFeatureIndex` config in experimental
// campaigns.
BASE_FEATURE(kGrowthCampaignsExperiment1, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment3, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment4, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment5, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment6, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment7, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment8, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment9, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment10, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment11, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment12, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment13, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment14, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment15, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment16, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment17, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment18, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment19, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperiment20, base::FEATURE_ENABLED_BY_DEFAULT);

// List of one-off Growth Framework experiment flag that will be associated
// with a finch study to deliver finch param for each experiment group to
// create randomization group that match the experiment tag targeting in
// Growth campaigns.
// The group will be selected by `oneOffExpFeatureIndex` config in experimental
// campaigns.
// Different from the predefined feature flag section above. These flags are
// used by study/groups that refer to multiple feature flags.
BASE_FEATURE(kGrowthCampaignsExperimentFileAppGamgee,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGrowthCampaignsExperimentG1Nudge,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables consumer session customizations with growth campaigns.
BASE_FEATURE(kGrowthCampaignsInConsumerSession,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Demo Mode customizations with growth campaigns.
BASE_FEATURE(kGrowthCampaignsInDemoMode, base::FEATURE_ENABLED_BY_DEFAULT);

// Show the nudge widget inside the window bounds and parent to the window.
BASE_FEATURE(kGrowthCampaignsShowNudgeInsideWindowBounds,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering when loading campaigns complete.
BASE_FEATURE(kGrowthCampaignsTriggerAtLoadComplete,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by app open event is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByAppOpen,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by url navigation is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByBrowser,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by any event is enabled.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByEvent, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether growth campaigns triggering by recording an event.
// This flag is used as a kill switch to disable the feature in the case that
// the feature introduces any unexpected behaviours.
BASE_FEATURE(kGrowthCampaignsTriggerByRecordEvent,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the growth nudge's triggering and the nudge widget
// invisibility and inactivation event should be observed to conditionally
// cancel the nudge.
BASE_FEATURE(kGrowthCampaignsObserveTriggeringWidgetChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables internals page of ChromeOS growth framework.
BASE_FEATURE(kGrowthInternals, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing the menu tabs in chrome://healthd-internals for displaying
// information from `cros_healthd`.
BASE_FEATURE(kHealthdInternalsTabs, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Kiosk session for the Helium android app.
BASE_FEATURE(kHeliumArcvmKiosk, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables additional features (e.g. manual launch) for ARCVM Kiosk debugging.
// Should stay disabled by default.
BASE_FEATURE(kHeliumArcvmKioskDevMode, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the Help app will render the App Detail Page and entry point.
BASE_FEATURE(kHelpAppAppDetailPage, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the Help app will render the Apps List page and entry point.
BASE_FEATURE(kHelpAppAppsList, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the logic that auto triggers the install dialog during the web app
// install flow initiated from the Help App.
BASE_FEATURE(kHelpAppAutoTriggerInstallDialog,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the home page of the Help App will show a section containing
// articles about apps.
BASE_FEATURE(kHelpAppHomePageAppArticles, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable showing search results from the help app in the launcher.
BASE_FEATURE(kHelpAppLauncherSearch, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new onboarding experience in the Help App.
BASE_FEATURE(kHelpAppOnboardingRevamp, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables opening the Help App's What's New page immediately instead of showing
// a notification to open the help app.
BASE_FEATURE(kHelpAppOpensInsteadOfReleaseNotesNotification,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
BASE_FEATURE(kHiddenNetworkWarning, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy). This feature also enables "contextual nudges" for
// gesture education.
BASE_FEATURE(kHideShelfControlsInTabletMode, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, add Hindi Inscript keyboard layout.
BASE_FEATURE(kHindiInscriptLayout, base::FEATURE_DISABLED_BY_DEFAULT);

// Helpful notifications for devices with Hybrid Chargers.
BASE_FEATURE(kHybridChargerNotifications, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, allows the user to cycle between windows of an app using Alt + `.
BASE_FEATURE(kSameAppWindowCycle, base::FEATURE_DISABLED_BY_DEFAULT);

// Make Sanitize available. This feature provides a "soft reset" option in CrOS
// settings. This soft reset will disable extensions and reset some of the
// settings to default.
BASE_FEATURE(kSanitize, "CrosSanitize", base::FEATURE_ENABLED_BY_DEFAULT);

// Make Sanitize V1 available. This feature provides a "soft reset" option in
// CrOS settings. In addition to the existing Sanitize features, this will
// provide a functional reset to user's proxy settings, input methods for
// keyboard and choice of languages in the spellchecker.
BASE_FEATURE(kSanitizeV1, "CrosSanitizeV1", base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, `SmbService` is created on user session startup task completed.
BASE_FEATURE(kSmbServiceIsCreatedOnUserSessionStartUpTaskCompleted,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, smbprovider is started on-demand.
BASE_FEATURE(kSmbproviderdOnDemand, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the snooping protection prototype is enabled.
BASE_FEATURE(kSnoopingProtection, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the split keyboard refactor cleanup.
BASE_FEATURE(kSplitKeyboardRefactor, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, used to configure the heuristic rules for some advanced IME
// features (e.g. auto-correct).
BASE_FEATURE(kImeRuleConfig, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables IME downloader experiment logic.
BASE_FEATURE(kImeDownloaderExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled use the updated US English IME language models.
BASE_FEATURE(kImeUsEnglishModelUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable or disable proto-based communication for IME Service.
BASE_FEATURE(kImeServiceProto, base::FEATURE_DISABLED_BY_DEFAULT);

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
// Enables a change in the IME switching logic such that the mojo connection
// status is tracked via a global boolean instead of checking if the runner is
// idle.
BASE_FEATURE(kImeSwitchCheckConnectionStatus, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show new management disclosure UI page instead of the
// management warning bubble.
BASE_FEATURE(kImprovedManagementDisclosure, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Instant Hotspot on Nearby. b/303121363.
BASE_FEATURE(kInstantHotspotOnNearby, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Instant Hotspot rebrand/feature improvements. b/290075504.
BASE_FEATURE(kInstantHotspotRebrand, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Instant Tethering on ChromeOS.
BASE_FEATURE(kInstantTethering, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// Controls the launched locales.
BASE_FEATURE(kInternalServerSideSpeechRecognition,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// The supported locales for this feature are specified using the locales
// filter in finch config.
BASE_FEATURE(kInternalServerSideSpeechRecognitionByFinch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the internal server side speech recognition on ChromeOS.
// The supported locales for this feature are specified using the locales
// filter in finch config. The languages controlled by this feature use the
// S3 USM_RNNT model.
BASE_FEATURE(kInternalServerSideSpeechRecognitionUSMModelFinch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables new experimental IPP-first setup path for USB printers on ChromeOS.
// Used in finch experiment.
BASE_FEATURE(kIppFirstSetupForUsbPrinters, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Romaji/Kana mode switch for Japanese VK.
BASE_FEATURE(kJapaneseInputModeSwitchInVK, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kJupiterScreensaver, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables automatic downloading and installing fonts via language packs, based
// on the user's preferences.
BASE_FEATURE(kLanguagePacksFonts, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables loading in fonts via language packs on login, even after a download.
const base::FeatureParam<bool> kLanguagePacksFontsLoadAfterDownloadDuringLogin =
    {&kLanguagePacksFonts, "load_after_download_during_login", true};

// Enables the UI and relative logic to manage Language Packs in Settings.
// This feature allows users to install/remove languages and input methods
// via the corresponding Settings page.
BASE_FEATURE(kLanguagePacksInSettings, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, launcher continue section will suggest drive files based on
// recency, instead of fetching them using drive's ItemSuggest API.
BASE_FEATURE(kLauncherContinueSectionWithRecents,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Same as `kLauncherContinueSectionWithRecents`, but used to enable the feature
// via finch, while ensuring minimum Chrome version - i.e. to avoid finch config
// from enabling the feature on versions where
// LauncherContinueSectionWithRecents was first added.
BASE_FEATURE(kLauncherContinueSectionWithRecentsRollout,
             "LauncherContinueSectionWithRecentsRollout125",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses short intervals for launcher nudge for testing if enabled.
BASE_FEATURE(kLauncherNudgeShortInterval, base::FEATURE_DISABLED_BY_DEFAULT);

// Segmentation flag for local image search.
BASE_FEATURE(kFeatureManagementLocalImageSearch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lobster feature.
BASE_FEATURE(kLobster, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabling this testing flag will force the Lobster disclaimer screen to be
// shown every time Lobster is triggered, even if users have previously approved
// the Lobster consent. If users have declined the Lobster consent, the feature
// This flag should solely be enabled for convenient testing. Do not turn it on
// unless the feature is under testing.
BASE_FEATURE(kLobsterAlwaysShowDisclaimerForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lobster dogfood.
BASE_FEATURE(kLobsterDogfood, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables lobster dragging support.
BASE_FEATURE(kLobsterDraggingSupport, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster feedback form.
BASE_FEATURE(kLobsterFeedback, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster feedback form.
BASE_FEATURE(kLobsterFileNamingImprovement, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster restriction based on the current active IME.
BASE_FEATURE(kLobsterDisabledByInvalidIME, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls lobster availability on managed accounts.
BASE_FEATURE(kLobsterForManagedUsers, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster i18n response.
BASE_FEATURE(kLobsterI18n, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster entry point in quick insert zero state.
BASE_FEATURE(kLobsterQuickInsertZeroState, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lobster right click menu entry point.
BASE_FEATURE(kLobsterRightClickMenu, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabling this flag allows Lobster to receive and use the rewritten queries
// returned from the server.
BASE_FEATURE(kLobsterUseRewrittenQuery, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables / Disables the lobster feature from the feature management module.
BASE_FEATURE(kFeatureManagementLobster, base::FEATURE_DISABLED_BY_DEFAULT);

// Enabling this flag allows password complexity checks when setting a local pin
// or password.
BASE_FEATURE(kLocalFactorsPasswordComplexity,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables local authentication controller with PIN support.
BASE_FEATURE(kLocalAuthenticationWithPin, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables cross device supported reports within the feedback tool.
// (This feature is only available for dogfooders)
BASE_FEATURE(kLinkCrossDeviceDogfoodFeedback,
             "LinkCrossDeviceDogFoodFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables nearby-internals logs to be automatically saved to disk and attached
// to feedback reports.
BASE_FEATURE(kLinkCrossDeviceInternals, base::FEATURE_DISABLED_BY_DEFAULT);

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
BASE_FEATURE(kLockScreenHideSensitiveNotificationsSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notifications on the lock screen.
BASE_FEATURE(kLockScreenNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to allow MAC address randomization to be enabled for WiFi networks.
BASE_FEATURE(kMacAddressRandomization, base::FEATURE_DISABLED_BY_DEFAULT);

// Enabling this flag allows the managed local pin and password related changes
// to be applied.
BASE_FEATURE(kManagedLocalPinAndPassword, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables policy management for USB printers.
BASE_FEATURE(kManagedUsbPrinters, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Mahi on PDF contents in the Media App.
BASE_FEATURE(kMediaAppPdfMahi, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Mantis on image contents in the Media App
BASE_FEATURE(kMediaAppImageMantis, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Erase feature of Mantis
BASE_FEATURE(kMediaAppImageMantisErase, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Expand Background feature of Mantis
BASE_FEATURE(kMediaAppImageMantisExpandBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Make A Sticker feature of Mantis
BASE_FEATURE(kMediaAppImageMantisMakeASticker,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the specified model will be used with the Mantis feature
BASE_FEATURE(kMediaAppImageMantisModel, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Reimagine feature of Mantis
BASE_FEATURE(kMediaAppImageMantisReimagine, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Remove Background feature of Mantis
BASE_FEATURE(kMediaAppImageMantisRemoveBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<MantisModel>::Option mantis_model_options[] = {
    {MantisModel::V1, "v1"},
    {MantisModel::V2, "v2"}};

const base::FeatureParam<MantisModel> kMediaAppImageMantisModelParams{
    &kMediaAppImageMantisModel, "mantis_model", MantisModel::V2,
    &mantis_model_options};

// Enables to split left and right modifiers in settings.
BASE_FEATURE(kModifierSplit, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables to split left and right modifiers in settings.
BASE_FEATURE(kMouseImposterCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Phone Hub recent apps loading and error views based on the
// connection status with the phone.
BASE_FEATURE(kEcheNetworkConnectionState, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kEcheShorterScanningDutyCycle, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kEcheScanningCycleOnTime{
    &kEcheShorterScanningDutyCycle, "EcheScanningCycleOnTime",
    base::Seconds(30)};

const base::FeatureParam<base::TimeDelta> kEcheScanningCycleOffTime{
    &kEcheShorterScanningDutyCycle, "EcheScanningCycleOffTime",
    base::Seconds(30)};

// Enables events from multiple calendars to be displayed in the Quick
// Settings Calendar.
BASE_FEATURE(kMultiCalendarSupport, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Nearby Presence for scanning and discovery of nearby devices.
BASE_FEATURE(kNearbyPresence, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a limit on the number of notifications that can show.
BASE_FEATURE(kNotificationLimit, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a bugfix for devices with a null custom top row property.
BASE_FEATURE(kNullTopRowFix, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature Management flag for the Sys UI holdback experiment, used to avoid
// certain devices.
BASE_FEATURE(kFeatureManagementShouldExcludeFromSysUiHoldback,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Drive integration.
BASE_FEATURE(kSysUiShouldHoldbackDriveIntegration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a holdback experiment for Task Management
// Glanceables.
BASE_FEATURE(kSysUiShouldHoldbackTaskManagement,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Night Light feature.
BASE_FEATURE(kNightLight, base::FEATURE_ENABLED_BY_DEFAULT);

// Extracts controller logic from child views of `NotificationCenterView` to
// place it in a new `NotificationCenterController` class.
BASE_FEATURE(kNotificationCenterController, base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled notification expansion animation.
BASE_FEATURE(kNotificationExpansionAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification scroll bar in UnifiedSystemTray.
BASE_FEATURE(kNotificationScrollBar, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notifications to be shown within context menus.
BASE_FEATURE(kNotificationsInContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable on-device grammar check service.
BASE_FEATURE(kOnDeviceGrammarCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the device supports on-device speech recognition.
// Forwarded to LaCrOS as BrowserInitParams::is_ondevice_speech_supported.
BASE_FEATURE(kOnDeviceSpeechRecognition, base::FEATURE_DISABLED_BY_DEFAULT);

// Gates syncing of the first batch of visual accessibility settings so the
// rollout can be staged and rolled back independently if issues surface.
BASE_FEATURE(kOsSyncAccessibilitySettingsBatch1,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gates syncing of the second batch of accessibility settings (reduced
// animations and caption styling) so the rollout can proceed in small,
// reversible stages.
BASE_FEATURE(kOsSyncAccessibilitySettingsBatch2,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gates syncing of the third batch of accessibility settings (screen + docked
// magnifiers and select-to-speak toggles) so rollout can proceed incrementally.
BASE_FEATURE(kOsSyncAccessibilitySettingsBatch3,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the OneDrive upload flow should immediately prompt the user to
// re-authenticate without first showing a notification.
BASE_FEATURE(kOneDriveUploadImmediateReauth, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the new UI for pinned notifications will be enabled.
// go/ongoing-ui
BASE_FEATURE(kOngoingProcesses, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, enrollment screen will allow for automatically adding the
// authenticated user to the device.
BASE_FEATURE(kOobeAddUserDuringEnrollment, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, CHOOBE Screen will be shown during the new user onboarding flow.
BASE_FEATURE(kOobeChoobe, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, CrOS events for OOBE and onboarding flow will be recorded.
BASE_FEATURE(kOobeCrosEvents, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled , Personalized Onboarding + App Recommendations
// will be shown if eligible during user onboarding flow.
BASE_FEATURE(kOobePersonalizedOnboarding, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Pre-consent metrics functionality is enabled during OOBE.
BASE_FEATURE(kOobePreConsentMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Consumer Software Screen will be shown during OOBE.
BASE_FEATURE(kOobeSoftwareUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, TouchPadScreen will be shown in CHOOBE.
// enabling this without enabling OobeChoobe flag will have no effect
BASE_FEATURE(kOobeTouchpadScroll,
             "OobeTouchpadScrollDirection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOobeDisplaySize, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, InputMethodsScreen will be shown in CHOOBE.
BASE_FEATURE(kOobeInputMethods, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SplitModifierKeyboardInfoScreen will be shown in OOBE.
BASE_FEATURE(kOobeSplitModifierKeyboardInfo, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE Jelly features.
BASE_FEATURE(kOobeJelly, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE Jelly modal features.
BASE_FEATURE(kOobeJellyModal, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE perks discovery feature.
BASE_FEATURE(kOobePerksDiscovery, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables OOBE ai intro feature.
BASE_FEATURE(kFeatureManagementOobeAiIntro, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables OOBE gemini intro feature.
BASE_FEATURE(kFeatureManagementOobeGeminiIntro,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables boot animation feature.
BASE_FEATURE(kFeatureManagementOobeSimon, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the OOBE QuickStart flow on the login screen.
BASE_FEATURE(kOobeQuickStartOnLoginScreen, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the enforcement of AutoEnrollment check in OOBE.
BASE_FEATURE(kOobeAutoEnrollmentCheckForced, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca for ARC apps.
BASE_FEATURE(kOrcaArc, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables elaborate for Orca.
BASE_FEATURE(kOrcaElaborate, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables emojify for Orca.
BASE_FEATURE(kOrcaEmojify, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca for managed users.
BASE_FEATURE(kOrcaForManagedUsers,
             "kOrcaForManagedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables formalize for Orca.
BASE_FEATURE(kOrcaFormalize, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables proofread for Orca.
BASE_FEATURE(kOrcaProofread, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables rephrase for Orca.
BASE_FEATURE(kOrcaRephrase, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables shorten for Orca.
BASE_FEATURE(kOrcaShorten, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables elaborate for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeElaborate, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables emojify for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeEmojify, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables formalize for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeFormalize, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables proofread for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeProofread, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables rephrase for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeRephrase, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables shorten for internationalized Orca.
BASE_FEATURE(kOrcaInternationalizeShorten, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Africaans support for Orca.
BASE_FEATURE(kOrcaAfrikaans, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Danish support for Orca.
BASE_FEATURE(kOrcaDanish, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Dutch support for Orca.
BASE_FEATURE(kOrcaDutch, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Finnish support for Orca.
BASE_FEATURE(kOrcaFinnish, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables French support for Orca.
BASE_FEATURE(kOrcaFrench, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables German support for Orca.
BASE_FEATURE(kOrcaGerman, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Italian support for Orca.
BASE_FEATURE(kOrcaItalian, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Japanese support for Orca.
BASE_FEATURE(kOrcaJapanese, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Norwegian support for Orca.
BASE_FEATURE(kOrcaNorwegian, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Polish support for Orca.
BASE_FEATURE(kOrcaPolish, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Portugese support for Orca.
BASE_FEATURE(kOrcaPortugese, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Spanish support for Orca.
BASE_FEATURE(kOrcaSpanish, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Swedish support for Orca.
BASE_FEATURE(kOrcaSwedish, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca dragging support.
BASE_FEATURE(kOrcaDraggingSupport, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca capability check.
BASE_FEATURE(kOrcaUseAccountCapabilities, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Orca on Workspace.
BASE_FEATURE(kOrcaForceFetchContextOnGetEditorPanelContext,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we force fetching input context
BASE_FEATURE(kOrcaOnWorkspace, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables new Orca service connection logic.
BASE_FEATURE(kOrcaServiceConnection, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables proto-based Orca service communication logic.
BASE_FEATURE(kOrcaServiceProto, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Orca will only be available in English locales.
BASE_FEATURE(kOrcaOnlyInEnglishLocales, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Orca resizing support.
BASE_FEATURE(kOrcaResizingSupport, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Orca on Demo mode.
BASE_FEATURE(kOrcaSupportDemoMode, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, OsSyncConsent Revamp will be shown.
// enabling this without enabling Lacros flag will have no effect
BASE_FEATURE(kOsSyncConsentRevamp, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Jelly colors and components to appear in the Parent Access Widget
// if jelly-colors is also enabled.
BASE_FEATURE(kParentAccessJelly, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a notification warning users that their Thunderbolt device is not
// supported on their CrOS device.
// TODO(crbug.com/40199811): Revisit this flag when there is a way to query
// billboard devices correctly.
BASE_FEATURE(kPcieBillboardNotification, base::FEATURE_DISABLED_BY_DEFAULT);

// Limits the items on the shelf to the ones associated with windows the
// currently active desk.
BASE_FEATURE(kPerDeskShelf, base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within ChromeOS.
BASE_FEATURE(kPhoneHub, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Camera Roll feature in Phone Hub, which allows users to access
// recent photos and videos taken on a connected Android device
BASE_FEATURE(kPhoneHubCameraRoll, base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of seconds to wait before users can download the same photo
// from Camera Roll again.
const base::FeatureParam<base::TimeDelta> kPhoneHubCameraRollThrottleInterval{
    &kPhoneHubCameraRoll, "PhoneHubCameraRollThrottleInterval",
    base::Seconds(2)};

// Enables the incoming/ongoing call notification feature in Phone Hub.
BASE_FEATURE(kPhoneHubCallNotification, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubMonochromeNotificationIcons,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubPingOnBubbleOpen, base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of seconds to wait for ping response before disconnecting
const base::FeatureParam<base::TimeDelta> kPhoneHubPingTimeout{
    &kPhoneHubPingOnBubbleOpen, "PhoneHubPingTimeout", base::Seconds(5)};

BASE_FEATURE(kPhoneHubShortQuickActionPodsTitles,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables GIF search in Picker.
BASE_FEATURE(kPickerGifs, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
BASE_FEATURE(kPreferConstantFrameRate, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, ChromeOS print preview app is available. Enabling does not
// replace the existing Chrome print preview UI, and will require an additional
// flag and pref configured to facilitate. See b/323421684 for more information.
BASE_FEATURE(kPrintPreviewCrosApp, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Projector for managed users.
BASE_FEATURE(kProjectorManagedUser, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Projector app launches in debug mode, with more detailed
// error messages.
BASE_FEATURE(kProjectorAppDebug, base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to show pseduo transcript that is shorter than the
// threshold.
BASE_FEATURE(kProjectorShowShortPseudoTranscript,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to update the indexable text when metadata file gets
// uploaded.
BASE_FEATURE(kProjectorUpdateIndexableText, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable features that are not ready to enable by
// default but ready for internal testing.
BASE_FEATURE(kProjectorBleedingEdgeExperience,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the transcript muting feature is enabled.
BASE_FEATURE(kProjectorMuting, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether higher version transcripts should be redirected to PWA.
BASE_FEATURE(kProjectorRedirectToPwa, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether projector V2 is enabled.
BASE_FEATURE(kProjectorV2, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use USM for serverside speech recognition for projector.
BASE_FEATURE(kProjectorUseUSMForS3, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the projector app uses the latest endpoint for retrieving
// playback urls.
BASE_FEATURE(kProjectorUseDVSPlaybackEndpoint,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show promise icons during web app installations.
BASE_FEATURE(kPromiseIconsForWebApps, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the quick dim prototype is enabled.
BASE_FEATURE(kQuickDim, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuickAppAccessTestUI, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables fingerprint quick unlock.
// Note, that this feature is set from session manager via
// command-line flag.
BASE_FEATURE(kQuickUnlockFingerprint, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
// Controls whether the PIN auto submit backfill operation should be performed.
BASE_FEATURE(kQuickUnlockPinAutosubmitBackfill,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables reordering of screens in the recovery flow.
BASE_FEATURE(kRecoveryFlowReorder, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Release Notes notifications on non-stable ChromeOS
// channels. Used for testing.
BASE_FEATURE(kReleaseNotesNotificationAllChannels,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes the user always eligible to see the release notes notification.
// Normally there are conditions that prevent the notification from appearing.
// For example: channel, profile type, and whether or not the notification had
// already been shown this milestone.
BASE_FEATURE(kReleaseNotesNotificationAlwaysEligible,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables rendering ARC notifications using ChromeOS notification framework
BASE_FEATURE(kRenderArcNotificationsByChrome,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the OS to unpin apps that were pinned by PinnedLauncherApps policy
// but are no longer a part of it from shelf under specific conditions.
BASE_FEATURE(kRemoveStalePolicyPinnedAppsFromShelf,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reset audio I/O selection improvement pref, used for testing purpose.
BASE_FEATURE(kResetAudioSelectionImprovementPref,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, will reset all shortcut customizations on startup.
BASE_FEATURE(kResetShortcutCustomizations, base::FEATURE_DISABLED_BY_DEFAULT);

// Set all ScalableIph client side config to tracking only config.
BASE_FEATURE(kScalableIphTrackingOnly, base::FEATURE_DISABLED_BY_DEFAULT);

// Use client side config.
BASE_FEATURE(kScalableIphClientConfig, base::FEATURE_DISABLED_BY_DEFAULT);

// Adds a shelf pod button that appears whenever the shelf has limited space and
// acts as an entrypoint to other shelf pod buttons to prevent overflow.
BASE_FEATURE(kScalableShelfPods, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the scanner dogfood update.
BASE_FEATURE(kScannerDogfood, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the toast which allows users to provide feedback after a Scanner
// action is completed.
BASE_FEATURE(kScannerFeedbackToast, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the scanner update.
BASE_FEATURE(kScannerUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables / Disables the scanner feature from the feature management module.
BASE_FEATURE(kFeatureManagementScanner, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable support for multiple scheduler configurations.
BASE_FEATURE(kSchedulerConfiguration, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen feature in the personalization app.
BASE_FEATURE(kFeatureManagementSeaPen, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sea pen text input feature in the personalization app.
BASE_FEATURE(kSeaPenTextInput, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sea pen text input translation feature.
BASE_FEATURE(kSeaPenTextInputTranslation, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sea pen feature for ChromeOS demo mode.
BASE_FEATURE(kSeaPenDemoMode, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sea pen prompt rewrite feature.
BASE_FEATURE(kSeaPenQueryRewrite, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sea pen feature with next templates.
BASE_FEATURE(kSeaPenUseExptTemplate, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automated control of the refresh rate for the internal display.
BASE_FEATURE(kSeamlessRefreshRateSwitching, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
BASE_FEATURE(kSeparateNetworkIcons, base::FEATURE_DISABLED_BY_DEFAULT);

// With this feature enabled, the shortcut app badge is painted in the UI
// instead of being part of the shortcut app icon.
BASE_FEATURE(kSeparateWebAppShortcutBadgeIcon,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
BASE_FEATURE(kSessionManagerLongKillTimeout, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the session manager daemon will abort the browser if its
// liveness checker detects a hang, i.e. the browser fails to acknowledge and
// respond sufficiently to periodic pings.  IMPORTANT NOTE: the feature name
// here must match exactly the name of the feature in the open-source ChromeOS
// file session_manager_service.cc.
BASE_FEATURE(kSessionManagerLivenessCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Removes notifier settings from quick settings view.
BASE_FEATURE(kSettingsAppNotificationSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether theme changes should be animated for the Settings app.
BASE_FEATURE(kSettingsAppThemeChangeAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should track auto-hide preferences separately between clamshell
// and tablet.
BASE_FEATURE(kShelfAutoHideSeparation, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables launcher nudge that animates the home button to guide users to open
// the launcher.
BASE_FEATURE(kShelfLauncherNudge, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the OS update page in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMAOsUpdate, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables 3p diagnostics in the Shimless RMA flow.
BASE_FEATURE(kShimlessRMA3pDiagnostics, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables 3p diagnostics dev mode in the Shimless RMA flow. This will skip some
// checks to allow developers to use dev-signed extensions for development
// purpose.
BASE_FEATURE(kShimlessRMA3pDiagnosticsDevMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Shimless diagnostics IWAs can access user permission through
// requesting permission at install time.
BASE_FEATURE(kShimlessRMA3pDiagnosticsAllowPermissionPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the skip option of hardware validation on Shimless RMA
// landing page.
BASE_FEATURE(kShimlessRMAHardwareValidationSkip,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the option of grey out specific input fields on Shimless
// RMA device information page.
BASE_FEATURE(kShimlessRMADynamicDeviceInfoInputs,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, system shortcuts will utilize state machiens instead of
// keeping track of entire history of keys pressed.
BASE_FEATURE(kShortcutStateMachines, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables a toggle to enable Bluetooth debug logs.
BASE_FEATURE(kShowBluetoothDebugLogToggle, base::FEATURE_ENABLED_BY_DEFAULT);

// Shows live caption in the video conference tray.
BASE_FEATURE(kShowLiveCaptionInVideoConferenceTray,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether sharing user name should be shown in the continue section for drive
// files shown because they have been recently shared with the user.
BASE_FEATURE(kShowSharingUserInLauncherContinueSection,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows the spatial audio toggle in audio settings page.
BASE_FEATURE(kShowSpatialAudioToggle, base::FEATURE_ENABLED_BY_DEFAULT);

// Only collect metrics for the server certificate verification failure in
// EAP networks.
BASE_FEATURE(kSingleCaCertVerificationPhase0,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Try to use only a single CA cert for the EAP network if CA cert was selected,
// fallback to the previous config.
BASE_FEATURE(kSingleCaCertVerificationPhase1, base::FEATURE_ENABLED_BY_DEFAULT);

// Use a single CA cert for the EAP network if CA cert was selected, no
// fallback.
BASE_FEATURE(kSingleCaCertVerificationPhase2,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling/disabling the Sunfish feature.
BASE_FEATURE(kSunfishFeature, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables custom Demo Mode behavior on feature-aware devices, as controlled by
// the feature management module.
BASE_FEATURE(kFeatureManagementFeatureAwareDeviceDemoMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the demo mode app orientation locked in landscape.
BASE_FEATURE(kDemoModeAppLandscapeLocked, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the ToS Notification in demo mode signed-in sessions.
BASE_FEATURE(kDemoSessionToSNotification, base::FEATURE_ENABLED_BY_DEFAULT);

// The pref kSecondaryGoogleAccountSigninAllowed is set to false in Demo Mode.
BASE_FEATURE(kDemoModeSecondaryGoogleAccountSigninAllowedFalse,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to include the device info in the demo account setup request
// to the demo server in signed-in experience.
BASE_FEATURE(kSendDeviceInfoToDemoServer, base::FEATURE_ENABLED_BY_DEFAULT);

// Moves toasts to the bottom-side corner where the status area is instead of
// the center when enabled.
BASE_FEATURE(kSideAlignedToasts, base::FEATURE_DISABLED_BY_DEFAULT);

// Uses experimental component version for smart dim.
BASE_FEATURE(kSmartDimExperimentalComponent, base::FEATURE_DISABLED_BY_DEFAULT);

// Deprecates Sign in with Smart Lock feature. Hides Smart Lock at the sign in
// screen, removes the Smart Lock subpage in settings, and shows a one-time
// notification for users who previously had this feature enabled.
BASE_FEATURE(kSmartLockSignInRemoved, base::FEATURE_ENABLED_BY_DEFAULT);

// Replaces uses of `SystemNudge` with the new `AnchoredNudge` component.
BASE_FEATURE(kSystemNudgeMigration, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Device End Of Lifetime incentive notifications.
BASE_FEATURE(kSystemShortcutBehavior, base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kSystemTrayShadow, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ChromeOS system-proxy daemon, only for system services. This
// means that system services like tlsdate, update engine etc. can opt to be
// authenticated to a remote HTTP web proxy via system-proxy.
BASE_FEATURE(kSystemProxyForSystemServices, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI to allow Chromebook hotspot functionality for experimental
// carriers, modem and modem FW.
BASE_FEATURE(kTetheringExperimentalFunctionality,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Telemetry Extension.
BASE_FEATURE(kTelemetryExtension, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Terminal System App to load from Downloads for developer testing.
// Only works in dev and canary channels.
BASE_FEATURE(kTerminalDev, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental feature for resizing tiling windows.
BASE_FEATURE(kTilingWindowResize, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the TrafficCountersHandler class to auto-reset traffic counters
// and shows Data Usage in the Celluar Settings UI.
BASE_FEATURE(kTrafficCountersEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables traffic counters for WiFi networks.
BASE_FEATURE(kTrafficCountersForWiFiTesting, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables trilinear filtering.
BASE_FEATURE(kTrilinearFiltering, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Device Trust connector client code on unmanaged devices
BASE_FEATURE(kUnmanagedDeviceDeviceTrustConnectorEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Android staging SM-DS server when fetching pending eSIM profiles.
BASE_FEATURE(kUseAndroidStagingSmds, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature toggles which dhcpcd version is used for IPv4 provisioning.
// If it is enabled, dhcpcd10 will be used, otherwise the legacy dhcpcd7 will be
// used. Note that IPv6 (DHCPv6-PD) always uses dhcpcd10.
BASE_FEATURE(kUseDHCPCD10, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the new `TokenHandleStoreImpl` will be used instead of
// `TokenHandleUtil`.
BASE_FEATURE(kUseTokenHandleStore, base::FEATURE_DISABLED_BY_DEFAULT);

// Use the AnnotatedAccountId for mapping between User and BrowserContext
// (a.k.a. browser's Profile).
BASE_FEATURE(kUseAnnotatedAccountId, base::FEATURE_DISABLED_BY_DEFAULT);

// This features toggles which implementation is used for authentication UIs on
// ChromeOS settings or PasswordManager. When the feature is enabled,
// `AuthPanel` is used as an authentication UI.
BASE_FEATURE(kUseAuthPanelInSession, base::FEATURE_ENABLED_BY_DEFAULT);

// This features toggles `AuthHub` is used as authentication backend by
// `AuthPanel` on ChromeOS.
BASE_FEATURE(kAuthPanelUsingAuthHub, base::FEATURE_DISABLED_BY_DEFAULT);

// This features controls whether or not we'll show the legacy WebAuthNDialog,
// that lives in ash/in_session_auth/auth_dialog_contents_view or
// the new dialog that's also shared with Settings and Password Manager,
// that lives in ash/auth/view/active_session_auth_view
BASE_FEATURE(kWebAuthNAuthDialogMerge, base::FEATURE_ENABLED_BY_DEFAULT);

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
BASE_FEATURE(kUseMessagesStagingUrl, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLiveCaptionUserMicrophone, base::FEATURE_DISABLED_BY_DEFAULT);

// Remap search+click to right click instead of the legacy alt+click on
// ChromeOS.
BASE_FEATURE(kUseSearchClickForRightClick, base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Stork production SM-DS server when fetching pending eSIM profiles.
BASE_FEATURE(kUseStorkSmdsServerAddress, base::FEATURE_DISABLED_BY_DEFAULT);

// Use the staging server as part of the Wallpaper App to verify
// additions/removals of wallpapers.
BASE_FEATURE(kUseWallpaperStagingUrl, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables user activity prediction for power management on
// ChromeOS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
BASE_FEATURE(kUserActivityPrediction, base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts the video conference feature to the intended
// target population,
BASE_FEATURE(kFeatureManagementVideoConference,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the vc background replace is enabled.
BASE_FEATURE(kVcBackgroundReplace,
             "VCBackgroundReplace",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the birch model provides lost video conference tab
// suggestions.
BASE_FEATURE(kBirchVideoConferenceSuggestions,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to resize thumbnail in VcBackgroundApp.
BASE_FEATURE(kVcResizeThumbnail, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the DLC downloading UI for video conferencing tiles is
// enabled.
BASE_FEATURE(kVcDlcUi, base::FEATURE_ENABLED_BY_DEFAULT);

// This is only used as a way to disable portrait relighting.
BASE_FEATURE(kVcPortraitRelight, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables alternative inference backends for running ChromeOS video
// conferencing portrait relighing models.
BASE_FEATURE(kVcRelightingInferenceBackend, base::FEATURE_DISABLED_BY_DEFAULT);

// This is only used as a way to disable stopAllScreenShare.
BASE_FEATURE(kVcStopAllScreenShare, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable or disable the fake effects for ChromeOS video conferencing controls
// UI. Only meaningful in the emulator.
BASE_FEATURE(kVcControlsUiFakeEffects, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative inference backends for running ChromeOS video
// conferencing segmentation models.
BASE_FEATURE(kVcSegmentationInferenceBackend,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative segmentation models for ChromeOS video
// conferencing blur or relighting.
BASE_FEATURE(kVcSegmentationModel,
             "VCSegmentationModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables alternative inference backends for running ChromeOS video
// conferencing face retouch models.
BASE_FEATURE(kVcRetouchInferenceBackend, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Studio Look and VC settings for ChromeOS video
// conferencing.
BASE_FEATURE(kVcStudioLook, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables mic indicator inside VC tray title header
BASE_FEATURE(kVcTrayMicIndicator,
             "VCTrayMicIndicator",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables sidetone toggle inside VC tray title header
BASE_FEATURE(kVcTrayTitleHeader,
             "VCTrayTitleHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables alternative light intensity for ChromeOS video
// conferencing relighting.
BASE_FEATURE(kVcLightIntensity,
             "VCLightIntensity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables web API support for ChromeOS video conferencing.
BASE_FEATURE(kVcWebApi, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to allow enabling wake on WiFi features in shill.
BASE_FEATURE(kWakeOnWifiAllowed, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable "daily" refresh wallpaper to refresh every ten seconds for testing.
BASE_FEATURE(kWallpaperFastRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable using google photos shared albums for wallpaper.
BASE_FEATURE(kWallpaperGooglePhotosSharedAlbums,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new Welcome Experience for first-time peripheral connections.
BASE_FEATURE(kWelcomeExperience, base::FEATURE_ENABLED_BY_DEFAULT);

// kWelcomeExperienceTestUnsupportedDevices enables the new device Welcome
// Experience to be tested on external devices that are not officially
// supported. When enabled, users will be able to initiate and complete
// the enhanced Welcome Experience flow using these unsupported external
// devices. This flag is intended for testing purposes and should be disabled
// disabled in production environments.
BASE_FEATURE(kWelcomeExperienceTestUnsupportedDevices,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Welcome Tour that walks new users through ChromeOS System UI.
BASE_FEATURE(kWelcomeTour, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether ChromeVox is supported in the Welcome Tour that walks new users
// through ChromeOS System UI.
BASE_FEATURE(kWelcomeTourChromeVoxSupported, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the Welcome Tour is enabled counterfactually as part of an experiment
// arm. When this is enabled, the Welcome Tour V1 will be shown.
BASE_FEATURE(kWelcomeTourCounterfactualArm, base::FEATURE_DISABLED_BY_DEFAULT);

// Forces user eligibility for the Welcome Tour that walks new users through
// ChromeOS System UI. Enabling this flag has no effect unless `kWelcomeTour` is
// also enabled.
BASE_FEATURE(kWelcomeTourForceUserEligibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Welcome Tour holdback is enabled as part of an experiment arm.
// When this is enabled, neither version of Welcome Tour version will be shown.
BASE_FEATURE(kWelcomeTourHoldbackArm, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Welcome Tour V3 that has different strings and steps than V1.
// Enabling this flag has no effect unless `kWelcomeTour` is also enabled.
BASE_FEATURE(kWelcomeTourV3, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable MAC Address Randomization on WiFi connection.
BASE_FEATURE(kWifiConnectMacAddressRandomization,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control whether the Wi-Fi concurrency Shill API is used when enable station
// Wi-Fi or tethering in Chrome Ash.
BASE_FEATURE(kWifiConcurrency, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable syncing of Wi-Fi configurations between
// ChromeOS and a connected Android phone.
BASE_FEATURE(kWifiSyncAndroid, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable syncing of proxy configurations on
// Wi-Fi networks that are uploaded to Chrome Sync.
BASE_FEATURE(kWifiSyncUploadProxyConfigs, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable syncing of proxy configurations on
// Wi-Fi networks that are received from Chrome Sync.
BASE_FEATURE(kWifiSyncApplyProxyConfigs, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to apply incoming Wi-Fi configuration delete events from
// the Chrome Sync server.
BASE_FEATURE(kWifiSyncApplyDeletes, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that splits windows by dragging one window
// over another window.
BASE_FEATURE(kWindowSplitting, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that lets users easily layout, resize and
// position their windows using only mouse and touch gestures.
BASE_FEATURE(kWmMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental feature that overrides the specific holdback
// experiments on the M-129.
BASE_FEATURE(kIgnoreM129Holdback, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for 28 day device active pings
// on ChromeOS.
BASE_FEATURE(kDeviceActiveClient28DayActiveCheckMembership,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn cohort device active
// pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnCohortCheckMembership,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables PSM CheckMembership for the churn observation
// device active pings on ChromeOS.
BASE_FEATURE(kDeviceActiveClientChurnObservationCheckMembership,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables attaching first active week and last powerwash week to
// the churn observation check in ping.
BASE_FEATURE(kDeviceActiveClientChurnObservationNewDeviceMetadata,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables peripheral customization to be split per device.
BASE_FEATURE(kPeripheralCustomization, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables peripherals logging.
BASE_FEATURE(kEnablePeripheralsLogging,
             "PeripheralsLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable peripheral notification to notify users when a input device is
// connected to the user's chromebook for the first time.
BASE_FEATURE(kPeripheralNotification, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable fast ink for software cursor. Fast ink provides a low-latency
// cursor with possible tearing artifacts.
BASE_FEATURE(kEnableFastInkForSoftwareCursor, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDozeModePowerScheduler, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fwupd developer mode, disabling all firmware authentication checks.
BASE_FEATURE(kFwupdDeveloperMode, base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool ArePromiseIconsForWebAppsEnabled() {
  return base::FeatureList::IsEnabled(kPromiseIconsForWebApps);
}

bool ForceOnDeviceAppControlsForAllRegions() {
  return base::FeatureList::IsEnabled(kForceOnDeviceAppControlsForAllRegions);
}

bool IsAudioSelectionImprovementEnabled() {
  return base::FeatureList::IsEnabled(kAudioSelectionImprovement);
}

bool Is16DesksEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagement16Desks);
}

bool IsOnDeviceAppControlsEnabled() {
  return base::FeatureList::IsEnabled(kOnDeviceAppControls);
}

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAltClickAndSixPackCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kAltClickAndSixPackCustomization);
}

bool IsAmbientEQDefaultOff() {
  return base::FeatureList::IsEnabled(kAmbientEQDefaultOff);
}

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
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

bool IsAutoNightLightEnabled() {
  return base::FeatureList::IsEnabled(kAutoNightLight);
}

bool IsAutoSignOutEnabled() {
  return base::FeatureList::IsEnabled(kAutoSignOut);
}

bool IsBabelOrcaAvailable() {
  return base::FeatureList::IsEnabled(kBabelOrca);
}

bool IsBatteryChargeLimitAvailable() {
  return base::FeatureList::IsEnabled(kBatteryChargeLimit);
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

bool IsBocaUberEnabled() {
  return base::FeatureList::IsEnabled(kBocaUber);
}

bool IsBocaEnabled() {
  return base::FeatureList::IsEnabled(kBoca);
}

bool IsBocaConsumerEnabled() {
  return base::FeatureList::IsEnabled(kBocaConsumer);
}

bool IsBocaCustomPollingEnabled() {
  return base::FeatureList::IsEnabled(kBocaCustomPolling);
}

bool IsBocaOnTaskLockedQuizMigrationEnabled() {
  return base::FeatureList::IsEnabled(kBocaOnTaskLockedQuizMigration);
}

bool IsBocaOnTaskPodEnabled() {
  return base::FeatureList::IsEnabled(kBocaOnTaskPod);
}

bool IsBocaOnTaskUnmuteBrowserTabsOnUnlockEnabled() {
  return base::FeatureList::IsEnabled(kBocaOnTaskUnmuteBrowserTabsOnUnlock);
}

bool IsBocaLockedModeCustomCountdownDurationEnabled() {
  return base::FeatureList::IsEnabled(kBocaLockedModeCustomCountdownDuration);
}

bool IsBocaStudentHeartbeatEnabled() {
  return base::FeatureList::IsEnabled(kBocaStudentHeartbeat);
}

bool IsBocaStudentHeartbeatCustomIntervalEnabled() {
  return base::FeatureList::IsEnabled(kBocaStudentHeartbeatCustomInterval);
}

bool IsBocaSpotlightEnabled() {
  return base::FeatureList::IsEnabled(kBocaSpotlight);
}

bool IsBocaNetworkRestrictionEnabled() {
  return base::FeatureList::IsEnabled(kBocaNetworkRestriction);
}

bool IsBocaClientTypeForSpeechRecognitionEnabled() {
  return base::FeatureList::IsEnabled(kBocaClientTypeForSpeechRecognition);
}

bool IsBocaAdjustCaptionBubbleOnExpandEnabled() {
  return base::FeatureList::IsEnabled(kBocaAdjustCaptionBubbleOnExpand);
}

bool IsBocaKeepSWAOpenOnSessionEndedEnabled() {
  return base::FeatureList::IsEnabled(kBocaKeepSWAOpenOnSessionEnded);
}

bool IsBocaSequentialSessionLoadEnabled() {
  return base::FeatureList::IsEnabled(kBocaSequentialSessionLoad);
}

bool IsBocaLockPauseUpdateEnabled() {
  return base::FeatureList::IsEnabled(kBocaLockPauseUpdate);
}

bool IsBocaNavSettingsDialogEnabled() {
  return base::FeatureList::IsEnabled(kBocaNavSettingsDialog);
}

bool IsBocaCaptionToggleEnabled() {
  return base::FeatureList::IsEnabled(kBocaCaptionToggle);
}

bool IsBocaSpotlightRobotRequesterEnabled() {
  return base::FeatureList::IsEnabled(kBocaSpotlightRobotRequester);
}

bool IsBocaSequentialInsertActivityEnabled() {
  return base::FeatureList::IsEnabled(kBocaSequentialInsertActivity);
}

bool IsBocaTranslateToggleEnabled() {
  return base::FeatureList::IsEnabled(kBocaTranslateToggle);
}

bool IsBocaMigrateSpeechRecognizerClientEnabled() {
  return base::FeatureList::IsEnabled(kBocaMigrateSpeechRecongnizerClient);
}

bool IsBocaReceiverAppEnabled() {
  return base::FeatureList::IsEnabled(kBocaReceiverApp);
}

bool IsBocaConfigureMaxStudentsEnabled() {
  return base::FeatureList::IsEnabled(kBocaConfigureMaxStudents);
}

bool IsBocaCourseWorkMaterialApiEnabled() {
  return base::FeatureList::IsEnabled(kBocaCourseWorkMaterialApi);
}

bool IsBocaScreenSharingTeacherEnabled() {
  return base::FeatureList::IsEnabled(kBocaScreenSharingTeacher);
}

bool IsBocaScreenSharingStudentEnabled() {
  return base::FeatureList::IsEnabled(kBocaScreenSharingStudent);
}

bool IsBocaHostAudioEnabled() {
  return base::FeatureList::IsEnabled(kBocaHostAudio);
}

bool IsBocaAudioForKioskEnabled() {
  return base::FeatureList::IsEnabled(kBocaAudioForKiosk);
}

bool IsBocaRedirectStudentAudioToKioskEnabled() {
  return base::FeatureList::IsEnabled(kBocaRedirectStudentAudioToKiosk);
}

bool IsBocaReceiverCustomPollingEnabled() {
  return base::FeatureList::IsEnabled(kBocaReceiverCustomPolling);
}

bool IsOnTaskStatusCheckEnabled() {
  return base::FeatureList::IsEnabled(kOnTaskStatusCheck);
}

bool IsBrightnessControlInSettingsEnabled() {
  return base::FeatureList::IsEnabled(kEnableBrightnessControlInSettings);
}

bool IsCaptureModeEducationEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeEducation);
}

bool IsCaptureModeEducationBypassLimitsEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeEducationBypassLimits);
}

bool IsCaptureModeOnDeviceOcrEnabled() {
  return (IsScannerEnabled() || IsSunfishFeatureEnabled()) &&
         base::FeatureList::IsEnabled(kCaptureModeOnDeviceOcr);
}

bool IsCheckPasswordsAgainstCryptohomeHelperEnabled() {
  return base::FeatureList::IsEnabled(kCheckPasswordsAgainstCryptohomeHelper);
}

bool IsContinuousOverviewScrollAnimationEnabled() {
  return base::FeatureList::IsEnabled(kContinuousOverviewScrollAnimation);
}

bool IsCoralFeatureEnabled() {
  return base::FeatureList::IsEnabled(kCoralFeature) &&
         base::FeatureList::IsEnabled(kCoralFeatureAllowed);
}

bool IsCryptauthAttestationSyncingEnabled() {
  return base::FeatureList::IsEnabled(kCryptauthAttestationSyncing);
}

bool IsCopyClientKeysCertsToChapsEnabled() {
  return !IsNssDbClientCertsRollbackEnabled() &&
         base::FeatureList::IsEnabled(kCopyClientKeysCertsToChaps);
}

bool IsCrosPrivacyHubLocationEnabled() {
  return base::FeatureList::IsEnabled(kCrosPrivacyHub);
}

bool IsCrosSeparateGeoApiKeyEnabled() {
  return base::FeatureList::IsEnabled(kCrosSeparateGeoApiKey);
}

bool IsCrosSafetyServiceEnabled() {
  return base::FeatureList::IsEnabled(kCrosSafetyService) ||
         IsCoralFeatureEnabled();
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

bool IsDemoModeAppResetWindowContainerEnable() {
  return base::FeatureList::IsEnabled(kDemoModeAppResetWindowContainer);
}

bool IsDemoModeSignInEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeSignIn);
}

bool IsDemoModeWallpaperUpdateEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeWallpaperUpdate);
}

bool IsDemoModeSignInFileCleanupEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeSignInFileCleanup);
}

bool IsDeskTemplateSyncEnabled() {
  return base::FeatureList::IsEnabled(kDeskTemplateSync);
}

bool IsDozeModePowerSchedulerEnabled() {
  return base::FeatureList::IsEnabled(kEnableDozeModePowerScheduler);
}

bool IsDisplayPerformanceModeEnabled() {
  return base::FeatureList::IsEnabled(kDisplayPerformanceMode);
}

bool IsPeripheralCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kPeripheralCustomization);
}

bool IsPeripheralsLoggingEnabled() {
  return base::FeatureList::IsEnabled(kEnablePeripheralsLogging);
}

bool IsDisplayAlignmentAssistanceEnabled() {
  return base::FeatureList::IsEnabled(kDisplayAlignAssist);
}

bool IsDoNotDisturbShortcutEnabled() {
  return base::FeatureList::IsEnabled(kDoNotDisturbShortcut);
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

bool IsDemoModeAppLandscapeLockedEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeAppLandscapeLocked);
}

bool IsDemoSessionToSNotificationEnabled() {
  return base::FeatureList::IsEnabled(kDemoSessionToSNotification);
}

bool IsDemoModeSecondaryGoogleAccountSigninAllowedFalse() {
  return base::FeatureList::IsEnabled(
      kDemoModeSecondaryGoogleAccountSigninAllowedFalse);
}

bool IsSendDeviceInfoToDemoServerEnabled() {
  return base::FeatureList::IsEnabled(kSendDeviceInfoToDemoServer);
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

bool IsESimEmptyActivationCodeSupportEnabled() {
  return base::FeatureList::IsEnabled(kESimEmptyActivationCodeSupported);
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

bool IsFastInkForSoftwareCursorEnabled() {
  return base::FeatureList::IsEnabled(kEnableFastInkForSoftwareCursor);
}

bool IsFastPairEnabled() {
  return base::FeatureList::IsEnabled(kFastPair);
}

bool IsFastPairAdvertisingFormat2025Enabled() {
  return base::FeatureList::IsEnabled(kFastPairAdvertisingFormat2025);
}

bool IsFastPairBleRotationEnabled() {
  return base::FeatureList::IsEnabled(kFastPairBleRotation);
}

bool IsFastPairDebugMetadataEnabled() {
  return base::FeatureList::IsEnabled(kFastPairDebugMetadata);
}

bool IsFastPairHandshakeLongTermRefactorEnabled() {
  return base::FeatureList::IsEnabled(kFastPairHandshakeLongTermRefactor);
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

bool IsFastPairSavedDevicesEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevices);
}

bool IsFastPairSavedDevicesStrictOptInEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSavedDevicesStrictOptIn);
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

bool IsFjordOobeEnabled() {
  return base::FeatureList::IsEnabled(kFjordOobe);
}

bool IsFjordOobeForceEnabled() {
  return base::FeatureList::IsEnabled(kFjordOobeForceEnabled);
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

bool ShouldForceEnableServerSideSpeechRecognition() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(kForceEnableServerSideSpeechRecognition);
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING);
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

bool IsGraduationEnabled() {
  return base::FeatureList::IsEnabled(kGraduation);
}

bool IsGraduationUseEmbeddedTransferEndpointEnabled() {
  return base::FeatureList::IsEnabled(kGraduationUseEmbeddedTransferEndpoint);
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

bool IsGrowthCampaignsDemoModeSignInEnabled() {
  return base::FeatureList::IsEnabled(kGrowthCampaignsDemoModeSignIn);
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

bool IsHeliumArcvmKioskEnabled() {
  return base::FeatureList::IsEnabled(kHeliumArcvmKiosk);
}

bool IsHeliumArcvmKioskDevModeEnabled() {
  return IsHeliumArcvmKioskEnabled() &&
         base::FeatureList::IsEnabled(kHeliumArcvmKioskDevMode);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode);
}

bool IsHybridChargerNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kHybridChargerNotifications);
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

bool IsInternalServerSideSpeechRecognitionEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(b/245614967): Once ready, enable this feature under
  // kProjectorBleedingEdgeExperience flag as well.
  return (ShouldForceEnableServerSideSpeechRecognition() ||
          base::FeatureList::IsEnabled(kInternalServerSideSpeechRecognition));
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

bool IsJupiterScreensaverEnabled() {
  return base::FeatureList::IsEnabled(kJupiterScreensaver) &&
         IsTimeOfDayScreenSaverEnabled();
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

bool IsLinkCrossDeviceDogfoodFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceDogfoodFeedback);
}

bool IsLinkCrossDeviceInternalsEnabled() {
  return base::FeatureList::IsEnabled(kLinkCrossDeviceInternals);
}

bool IsLobsterEnabled() {
  return base::FeatureList::IsEnabled(kLobsterDogfood) ||
         (base::FeatureList::IsEnabled(kLobster) &&
          base::FeatureList::IsEnabled(kFeatureManagementLobster));
}

bool IsLobsterDisabledByInvalidIME() {
  return base::FeatureList::IsEnabled(kLobsterDisabledByInvalidIME);
}

bool IsLobsterUseRewrittenQuery() {
  return base::FeatureList::IsEnabled(kLobsterUseRewrittenQuery);
}

bool IsLobsterI18nEnabled() {
  return base::FeatureList::IsEnabled(kLobsterI18n);
}

bool IsLobsterAlwaysShowDisclaimerForTesting() {
  return base::FeatureList::IsEnabled(kLobsterAlwaysShowDisclaimerForTesting);
}

bool IsLobsterEnabledForManagedUsers() {
  return base::FeatureList::IsEnabled(kLobsterForManagedUsers);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsGameDashboardGamepadSupportEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardGamepadSupport);
}

bool AreGameDashboardUtilitiesEnabled() {
  return base::FeatureList::IsEnabled(kGameDashboardUtilities);
}

bool IsAppLaunchShortcutEnabled() {
  return base::FeatureList::IsEnabled(kAppLaunchShortcut);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsProductivityLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementLocalImageSearch);
}

bool IsMacAddressRandomizationEnabled() {
  return base::FeatureList::IsEnabled(kMacAddressRandomization);
}

bool IsMultiCalendarSupportEnabled() {
  return base::FeatureList::IsEnabled(kMultiCalendarSupport);
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

bool IsNearbyPresenceEnabled() {
  return base::FeatureList::IsEnabled(kNearbyPresence);
}

bool IsNotificationLimitEnabled() {
  return base::FeatureList::IsEnabled(kNotificationLimit);
}

bool IsOAuthIppEnabled() {
  return base::FeatureList::IsEnabled(kEnableOAuthIpp);
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

bool IsOobeJellyEnabled() {
  return base::FeatureList::IsEnabled(kOobeJelly);
}

bool IsModifierSplitEnabled() {
  return base::FeatureList::IsEnabled(kModifierSplit);
}

bool IsMouseImposterCheckEnabled() {
  return base::FeatureList::IsEnabled(kMouseImposterCheck);
}

bool IsSplitKeyboardRefactorEnabled() {
  return base::FeatureList::IsEnabled(kSplitKeyboardRefactor) &&
         IsModifierSplitEnabled();
}

bool IsOobeAiIntroEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementOobeAiIntro);
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

bool IsOobeChoobeEnabled() {
  return base::FeatureList::IsEnabled(kOobeChoobe);
}

bool IsOobeCrosEventsEnabled() {
  return base::FeatureList::IsEnabled(kOobeCrosEvents);
}

bool IsOobePersonalizedOnboardingEnabled() {
  return base::FeatureList::IsEnabled(kOobePersonalizedOnboarding);
}

bool IsOobePreConsentMetricsEnabled() {
  return base::FeatureList::IsEnabled(kOobePreConsentMetrics);
}

bool IsOobeSoftwareUpdateEnabled() {
  return base::FeatureList::IsEnabled(kOobeSoftwareUpdate);
}

bool IsOobePerksDiscoveryEnabled() {
  return base::FeatureList::IsEnabled(kOobePerksDiscovery);
}

bool IsOobeQuickStartOnLoginScreenEnabled() {
  return IsCrossDeviceFeatureSuiteAllowed() &&
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

bool IsOobeAutoEnrollmentCheckForcedEnabled() {
  return base::FeatureList::IsEnabled(kOobeAutoEnrollmentCheckForced);
}

bool IsOobeSplitModifierKeyboardInfoEnabled() {
  return base::FeatureList::IsEnabled(kOobeSplitModifierKeyboardInfo);
}

bool IsOsSyncConsentRevampEnabled() {
  return base::FeatureList::IsEnabled(kOsSyncConsentRevamp);
}

bool IsParentAccessJellyEnabled() {
  return base::FeatureList::IsEnabled(kParentAccessJelly);
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

bool IsPhoneHubPingOnBubbleOpenEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubPingOnBubbleOpen);
}

bool IsPhoneHubEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHub);
}

bool IsPhoneHubCallNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCallNotification);
}

bool IsPhoneHubShortQuickActionPodsTitlesEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubShortQuickActionPodsTitles);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsPrinterPreviewCrosAppEnabled() {
  return base::FeatureList::IsEnabled(kPrintPreviewCrosApp);
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

bool IsProjectorUseUSMForS3Enabled() {
  return base::FeatureList::IsEnabled(kProjectorUseUSMForS3);
}

bool IsProjectorUseDVSPlaybackEndpointEnabled() {
  return base::FeatureList::IsEnabled(kProjectorUseDVSPlaybackEndpoint);
}

bool IsQuickDimEnabled() {
  return base::FeatureList::IsEnabled(kQuickDim) && switches::HasHps();
}

bool IsRecoveryFlowReorderEnabled() {
  return base::FeatureList::IsEnabled(kRecoveryFlowReorder);
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
  return base::FeatureList::IsEnabled(kScannerUpdate) ||
         base::FeatureList::IsEnabled(kScannerDogfood);
}

bool IsScannerFeedbackToastEnabled() {
  return base::FeatureList::IsEnabled(kScannerFeedbackToast);
}

bool IsSeaPenDemoModeEnabled() {
  return IsSeaPenEnabled() && base::FeatureList::IsEnabled(kSeaPenDemoMode);
}

bool IsSeaPenQueryRewriteEnabled() {
  return IsSeaPenTextInputEnabled() &&
         base::FeatureList::IsEnabled(kSeaPenQueryRewrite);
}

bool IsSeaPenEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementSeaPen);
}

bool IsSeaPenTextInputEnabled() {
  return IsSeaPenEnabled() && base::FeatureList::IsEnabled(kSeaPenTextInput);
}

bool IsSeaPenTextInputTranslationEnabled() {
  return IsSeaPenTextInputEnabled() &&
         base::FeatureList::IsEnabled(kSeaPenTextInputTranslation);
}

bool IsSeaPenUseExptTemplateEnabled() {
  return IsSeaPenEnabled() &&
         base::FeatureList::IsEnabled(kSeaPenUseExptTemplate);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
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

bool IsShimlessRMAHardwareValidationSkipEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAHardwareValidationSkip);
}

bool IsShimlessRMADynamicDeviceInfoInputsEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMADynamicDeviceInfoInputs);
}

bool IsShowSharingUserInLauncherContinueSectionEnabled() {
  return IsLauncherContinueSectionWithRecentsEnabled() &&
         base::FeatureList::IsEnabled(
             kShowSharingUserInLauncherContinueSection);
}

bool IsSunfishFeatureEnabled() {
  return base::FeatureList::IsEnabled(kSunfishFeature);
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

bool IsToggleCameraShortcutEnabled() {
  return base::FeatureList::IsEnabled(kEnableToggleCameraShortcut);
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

bool IsTouchscreenCalibrationEnabled() {
  return base::FeatureList::IsEnabled(kEnableTouchscreenCalibration);
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

bool IsWelcomeTourV3Enabled() {
  return IsWelcomeTourEnabled() && base::FeatureList::IsEnabled(kWelcomeTourV3);
}

bool IsWifiConcurrencyEnabled() {
  return base::FeatureList::IsEnabled(kWifiConcurrency);
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

bool IsFeatureAwareDeviceDemoModeEnabled() {
  return base::FeatureList::IsEnabled(
      kFeatureManagementFeatureAwareDeviceDemoMode);
}

bool IsUseAuthPanelInSessionEnabled() {
  return base::FeatureList::IsEnabled(kUseAuthPanelInSession);
}

bool IsAuthPanelUsingAuthHub() {
  return base::FeatureList::IsEnabled(kAuthPanelUsingAuthHub);
}

bool IsLocalAuthenticationWithPinEnabled() {
  return base::FeatureList::IsEnabled(kLocalAuthenticationWithPin);
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

bool IsUseTokenHandleStoreEnabled() {
  return base::FeatureList::IsEnabled(kUseTokenHandleStore);
}

bool IsFwupdDeveloperModeEnabled() {
  return base::FeatureList::IsEnabled(kFwupdDeveloperMode);
}

bool IsLocalFactorsPasswordComplexityEnabled() {
  return base::FeatureList::IsEnabled(kLocalFactorsPasswordComplexity);
}

bool IsManagedLocalPinAndPasswordEnabled() {
  return base::FeatureList::IsEnabled(kManagedLocalPinAndPassword);
}

}  // namespace ash::features
