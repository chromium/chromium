// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_FEATURES_H_
#define ASH_CONSTANTS_ASH_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAccountManagementFlowsV2;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAllowRepeatedUpdates;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAllowScrollSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool>
    kAmbientModeCulturalInstitutePhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeRssPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModePhotoPreviewFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModeDevUseProdFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAppListBubble;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcManagedAdbSideloadingSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcPreImeKeyEventSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistAutoCorrect;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistMultiWord;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfo;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoAddress;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoEmail;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoName;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoPhoneNumber;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAvatarToolbarButton;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothWbsDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothFixA2dpPacketSize;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPreferConstantFrameRate;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCdmFactoryDaemon;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularForbidAttachApn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularUseAttachApn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularUseExternalEuicc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCroshSWA;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniDiskResizing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniUseBusterImage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniUseDlc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableCryptAuthV1DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableIdleSocketsCloseOnMemoryPressure;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDemoModeSWA;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableOfficeEditingComponentApp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDriveFsBidirectionalNativeMessaging;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWAResizing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEmojiSuggestAddition;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableDnsProxy;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableHostnameSetting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableInputInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableLocalSearchService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableNetworkingInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableInputNoiseCancellationUi;
extern const base::Feature kEnableOobeChromeVoxHint;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnablePciguardUi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableSamlReauthenticationOnLockscreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kExoOrdinalMotion;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kExoPointerLock;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kExoLockNotification;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFamilyLinkOnSchoolDevice;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVideoPlayerAppHidden;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesSinglePartitionFormat;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesTrash;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipMount;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipPack;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipUnpack;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistory;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryNudgeSessionReset;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryContextMenuNudge;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryScreenshotNudge;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableFilesAppCopyImage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingGestureEditing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGaiaCloseViewMessage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGaiaReauthEndpoint;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGamepadVibration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppBackgroundPage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppDiscoverTab;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppDiscoverTabNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppLauncherSearch;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppSearchServiceIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeMojoDecoder;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeOptionsInSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPicker;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingDefault;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kKerberosSettingsSection;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosPrimary;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLanguageSettingsUpdate2;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppAnnotation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppDisplayExif;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppHandlesPdf;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppVideoControls;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMicMuteNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMinimumChromeVersion;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMultilingualTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNoteTakingForEnabledWebApps;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOnDeviceGrammarCheck;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOnDeviceSpeechRecognition;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNewOobeLayout;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOsFeedback;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOsSettingsDeepLinking;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPhoneHub;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPinSetupForFamilyLink;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPinSetupForManagedUsers;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPluginVmFullscreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPluginVmShowCameraPermissions;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPluginVmShowMicrophonePermissions;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPrintJobManagementApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPrintServerScaling;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPrinterStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPrinterStatusDialog;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjector;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjectorFeaturePod;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kQuickAnswers;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersRichUi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTextAnnotator;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTranslation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTranslationCloudAPI;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersOnEditableText;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersStandaloneSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmit;
// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmitBackfill;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesNotification;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesSuggestionChip;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFiltersInRecents;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppMediaLink;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppSearchablePdf;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppStickySettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSessionManagerLongKillTimeout;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShimlessRMAFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartDimExperimentalComponent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSplitSettingsSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemLatinPhysicalTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemProxyForSystemServices;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTelemetryExtension;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUpdatedCellularActivationUi;
// Visible for testing. Call UseBrowserSyncConsent() to check the flag.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseBrowserSyncConsent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseWallpaperStagingUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardMultipaste;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVmCameraMicIndicatorsAndNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVmStatusPage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWakeOnWifiAllowed;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWallpaperWebUI;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWebApkGenerator;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncAllowDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncApplyDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncAndroid;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeMozcProto;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniResetLxdDb;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCameraPrivacySwitchNotifications;

// Keep alphabetized.

COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAccountManagementFlowsV2Enabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeDevUseProdEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModePhotoPreviewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAppListBubbleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAssistantEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCellularActivationUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryContextMenuNudgeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsClipboardHistoryEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryScreenshotNudgeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryNudgeSessionResetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDemoModeSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDeepLinkingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAResizingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFamilyLinkOnSchoolDeviceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaCloseViewMessageEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaReauthEndpointEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHostnameSettingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsImeSandboxEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsKerberosSettingsSectionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMicMuteNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMinimumChromeVersionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNetworkingInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNewOobeLayoutEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputNoiseCancellationUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeChromeVoxHintEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeScreensPriorityEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPciguardUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubUseBleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitBackfillFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinSetupForFamilyLinkEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinSetupForManagedUsersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorFeaturePodEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersDogfood();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersOnEditableTextEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersRichUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersSettingToggleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersStandaloneSettingsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersTranslationCloudAPIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersTranslationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSamlReauthenticationOnLockscreenEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMAFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSplitSettingsSyncEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSystemLatinPhysicalTypingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWallpaperWebUIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWifiSyncAndroidEnabled();
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseBrowserSyncConsent();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseQuickAnswersTextAnnotator();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseV1DeviceSync();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseV2DeviceSync();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseAttachApn();

// Keep alphabetized.

}  // namespace features
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace features {
using namespace ::ash::features;
}
}  // namespace chromeos

#endif  // ASH_CONSTANTS_ASH_FEATURES_H_
