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
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAllowAmbientEQ;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAllowRepeatedUpdates;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAllowScrollSettings;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool>
    kAmbientModeCulturalInstitutePhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeRssPhotosEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModeDevUseProdFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModePhotoPreviewFeature;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAppListBubble;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcManagedAdbSideloadingSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kArcResizeLock;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAssistAutoCorrect;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAssistEmojiEnhanced;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAssistMultiWord;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistMultiWordExpanded;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAssistPersonalInfo;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoAddress;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoEmail;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoName;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistPersonalInfoPhoneNumber;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAudioUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutoNightLight;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAvatarToolbarButton;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBentoBar;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothAdvertisementMonitoring;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothFixA2dpPacketSize;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBluetoothRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBluetoothWbsDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisDiskManagement;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kButtonARCNetworkDiagnostics;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCalendarView;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCameraPrivacySwitchNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularAllowPerNetworkRoaming;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularForbidAttachApn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularUseAttachApn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularUseExternalEuicc;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kClipboardHistory;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryContextMenuNudge;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryNudgeSessionReset;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryScreenshotNudge;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCompositingBasedThrottling;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kContextualNudges;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCroshSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniUseDlc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniDiskResizing;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniResetLxdDb;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDarkLightMode;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDemoModeSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDiagnosticsAppNavigation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableCryptAuthV1DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableIdleSocketsCloseOnMemoryPressure;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableOfficeEditingComponentApp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDisplayAlignAssist;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisplayIdentification;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDockedMagnifier;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDragUnpinnedAppToPin;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDriveFsBidirectionalNativeMessaging;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWAResizing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEmojiSuggestAddition;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableBackgroundBlur;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableDnsProxy;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableFilesAppCopyImage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableHostnameSetting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableInputInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableInputNoiseCancellationUi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableLocalSearchService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableNetworkingInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableOAuthIpp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableOobeChromeVoxHint;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnablePciguardUi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableSamlNotificationOnPasswordChangeSuccess;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableSamlReauthenticationOnLockscreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kESimPolicy;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kExoLockNotification;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kExoOrdinalMotion;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kExoPointerLock;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFamilyLinkOnSchoolDevice;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFastPair;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesArchivemount;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesBannerFramework;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesSWA;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesSinglePartitionFormat;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesTrash;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipMount;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipPack;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesZipUnpack;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFiltersInRecents;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFullscreenAlertBubble;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFuseBox;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGaiaCloseViewMessage;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGaiaReauthEndpoint;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGamepadVibration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingGestureEditing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingLegacyRecognition;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppBackgroundPage;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kHelpAppDiscoverTab;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppDiscoverTabNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppLauncherSearch;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppSearchServiceIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHiddenNetworkWarning;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHideArcMediaNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHideShelfControlsInTabletMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpaceArcIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpaceInProgressDownloadsIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpaceIncognitoProfileIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kImeMojoDecoder;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kImeMozcProto;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeOptionsInSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPicker;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPickerClipboard;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeStylusHandwriting;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kKeyboardBasedDisplayArrangementInSettings;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosPrimary;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLanguagePacksHandwriting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLanguageSettingsUpdate2;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherAppSort;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherRemoveEmptySpace;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLicensePackagedOobeFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLockScreenHideSensitiveNotificationsSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLockScreenInlineReply;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLockScreenNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLockScreenMediaControls;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kManagedDeviceUIRedesign;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMediaAppHandlesPdf;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMediaAppMultiWindow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kManagedTermsOfService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMicMuteNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMinimumChromeVersion;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMultilingualTyping;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kNearbyKeepAliveFix;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNewLockScreenReauthLayout;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kNightLight;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMinorModeRestriction;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNoteTakingForEnabledWebApps;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNotificationExpansionAnimation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNotificationExperimentalShortTimeouts;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNotificationScrollBar;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNotificationsInContextMenu;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNotificationsRefresh;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOnDeviceGrammarCheck;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOnDeviceSpeechRecognition;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kOsFeedback;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOsSettingsAppNotificationsPage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOsSettingsDeepLinking;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kOverviewButton;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPcieBillboardNotification;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPerDeskShelf;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPerformantSplitViewResizing;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPhoneHub;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPhoneHubCameraRoll;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPhoneHubRecentApps;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPinSetupForManagedUsers;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPipRoundedCorners;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPluginVmFullscreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPreferConstantFrameRate;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjector;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjectorFeaturePod;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kQuickAnswers;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kQuickAnswersDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersOnEditableText;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTextAnnotator;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTranslation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersTranslationCloudAPI;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kQuickAnswersV2;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableQuickAnswersV2Translation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersV2SettingsSubToggle;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersOnEditableText;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickAnswersStandaloneSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockFingerprint;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmit;
// TODO(crbug.com/1104164) - Remove this once most users have their preferences
// backfilled.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmitBackfill;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReduceDisplayNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesSuggestionChip;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReverseScrollGestures;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kScalableStatusArea;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kScanAppMediaLink;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppMultiPageScan;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppSearchablePdf;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kScanAppStickySettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSemanticColorsDebugOverride;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSeparateNetworkIcons;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSessionManagerLongKillTimeout;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSessionManagerLivenessCheck;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSettingsAppNotificationSettings;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShimlessRMAFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShowDateInTrayButton;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartDimExperimentalComponent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartLockUIRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kSplitSettingsSync;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kStylusBatteryStatus;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kSyncConsentOptional;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSyncSettingsCategorization;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemLatinPhysicalTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemProxyForSystemServices;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTabClusterUI;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTelemetryExtension;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTrilinearFiltering;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseBluetoothSystemInAsh;
// Visible for testing. Call UseBrowserSyncConsent() to check the flag.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseBrowserSyncConsent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseStorkSmdsServerAddress;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseWallpaperStagingUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kVerticalSplitScreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardApi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardMultipaste;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardMultipasteSuggestion;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kVmStatusPage;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWakeOnWifiAllowed;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWallpaperWebUI;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWebApkGenerator;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWebUITabStripTabDragIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncAllowDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWifiSyncAndroid;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncApplyDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWindowsFollowCursor;

// Keep alphabetized.

COMPONENT_EXPORT(ASH_CONSTANTS) bool AreContextualNudgesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool DoWindowsFollowCursor();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAccountManagementFlowsV2Enabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAllowAmbientEQEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeDevUseProdEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModePhotoPreviewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAppListBubbleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAppNotificationsPageEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcResizeLockEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAssistiveMultiWordEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAutoNightLightEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBackgroundBlurEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBentoBarEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsBluetoothAdvertisementMonitoringEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBluetoothRevampEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCalendarViewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryContextMenuNudgeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsClipboardHistoryEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryNudgeSessionResetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsClipboardHistoryScreenshotNudgeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCompositingBasedThrottlingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDarkLightModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDeepLinkingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDemoModeSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDisplayAlignmentAssistanceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDisplayIdentificationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDragUnpinnedAppToPinEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAResizingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFamilyLinkOnSchoolDeviceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFileManagerSwaEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFullscreenAlertBubbleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaCloseViewMessageEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaReauthEndpointEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHideArcMediaNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHideShelfControlsInTabletModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHoldingSpaceArcIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsHoldingSpaceInProgressDownloadsIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsHoldingSpaceIncognitoProfileIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHostnameSettingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputNoiseCancellationUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsKeyboardBasedDisplayArrangementInSettingsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherAppSortEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherRemoveEmptySpaceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLicensePackagedOobeFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsLockScreenHideSensitiveNotificationsSupported();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLockScreenInlineReplyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLockScreenNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsManagedDeviceUIRedesignEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsManagedTermsOfServiceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMicMuteNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMinimumChromeVersionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMinorModeRestrictionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNearbyKeepAliveFixEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNetworkingInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNewLockScreenReauthLayoutEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationExpansionAnimationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsNotificationExperimentalShortTimeoutsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationScrollBarEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationsInContextMenuEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationsRefreshEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOAuthIppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeChromeVoxHintEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPcieBillboardNotificationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPciguardUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPerDeskShelfEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubCameraRollEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPerformantSplitViewResizingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubRecentAppsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitBackfillFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinSetupForManagedUsersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPipRoundedCornersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorFeaturePodEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersDogfood();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersOnEditableTextEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersTranslationCloudAPIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersTranslationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersV2Enabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersV2TranslationDisabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickAnswersV2SettingsSubToggleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsReduceDisplayNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsReverseScrollGesturesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSamlNotificationOnPasswordChangeSuccessEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSamlReauthenticationOnLockscreenEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsScalableStatusAreaEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSeparateNetworkIconsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSettingsAppNotificationSettingsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMAFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShowDateInTrayButtonEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSplitSettingsSyncEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSyncSettingsCategorizationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSyncConsentOptionalEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsStylusBatteryStatusEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSystemLatinPhysicalTypingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTabClusterUIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTrilinearFilteringEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUseStorkSmdsServerAddressEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsVerticalSplitScreenEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWallpaperWebUIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWebUITabStripTabDragIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWifiSyncAndroidEnabled();
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseAttachApn();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseBrowserSyncConsent();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseQuickAnswersTextAnnotator();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseV1DeviceSync();
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldUseV2DeviceSync();

// Keep alphabetized.

// These two functions are supposed to be temporary functions to set or get
// whether "WebUITabStrip" feature is enabled from Chrome.
COMPONENT_EXPORT(ASH_CONSTANTS) void SetWebUITabStripEnabled(bool enabled);
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWebUITabStripEnabled();

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
