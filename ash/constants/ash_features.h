// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_FEATURES_H_
#define ASH_CONSTANTS_ASH_FEATURES_H_

#include "ash/constants/ambient_animation_theme.h"
#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAdaptiveCharging;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAdaptiveChargingForTesting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAdjustSplitViewForVK;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAllowAmbientEQ;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAllowPolyDevicePairing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAllowRepeatedUpdates;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAllowScrollSettings;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAlwaysReinstallSystemWebApps;
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
extern const base::Feature kAmbientModeAnimationFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<AmbientAnimationTheme>
    kAmbientModeAnimationThemeParam;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModeDevUseProdFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAmbientModePhotoPreviewFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcFuseBoxFileSharing;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kArcInputOverlay;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kArcInputOverlayBeta;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcAndGuestOsFileTasksUseAppService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kArcManagedAdbSideloadingSupport;
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
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAssistantNativeIcons;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAudioPeripheralVolumeGranularity;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kAudioUrl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutoNightLight;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutocompleteExtendedSuggestions;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutocorrectParamsTuning;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAutozoomNudgeSessionReset;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAvatarsCloudMigration;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBentoBar;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothFixA2dpPacketSize;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBluetoothQualityReport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBluetoothRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBluetoothWbsDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kRobustAudioDeviceSelectLogic;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisBigGl;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisDiskManagement;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisForceBetaClient;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisForceDoubleScale;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisLinuxMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisPermitted;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBorealisStorageBallooning;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCalendarView;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCalendarModelDebugMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCameraAppDocScanDlc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCameraAppMultiPageDocScan;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCameraPrivacySwitchNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCaptivePortalUI2022;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCaptureModeDemoTools;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularBypassESimInstallationConnectivityCheck;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularCustomAPNProfiles;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCellularUseSecondEuicc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCheckPasswordsAgainstCryptohomeHelper;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryNudgeSessionReset;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kClipboardHistoryReorder;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kConsumerAutoUpdateToggleAllowed;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDesksCloseAll;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kContextualNudges;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCroshSWA;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrosLanguageSettingsUpdateJapanese;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrosNextWMP;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrosPrivacyHub;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrosPrivacyHubDogfood;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrosPrivacyHubFuture;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniBullseyeUpgrade;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDesksTemplates;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniDiskResizing;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniResetLxdDb;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniUseLxd4;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniMultiContainer;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kCrostiniImeSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCrostiniVirtualKeyboardSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGuestOSGenericInstaller;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kBruschetta;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kBruschettaAlphaMigrate;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2AlwaysUseActiveEligibleHosts;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DedupDeviceLastActivityTime;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptohomeRecoveryFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptohomeRecoveryFlowUI;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kCryptohomeRecoverySetup;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDemoModeSWA;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDeprecateAssistantStylusFeatures;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDeskTemplateSync;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDeviceActiveClient;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDeviceActiveClientDailyCheckMembership;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDeviceActiveClientMonthlyCheckIn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDeviceActiveClientMonthlyCheckMembership;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDeviceForceScheduledReboot;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<int> kDeviceForceScheduledRebootMaxDelay;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDiacriticsOnPhysicalKeyboardLongpress;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableCryptAuthV1DeviceSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableIdleSocketsCloseOnMemoryPressure;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableLacrosTtsSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDisplayAlignAssist;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDockedMagnifier;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDragUnpinnedAppToPin;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDragWindowToNewDesk;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDriveFsBidirectionalNativeMessaging;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDriveFsChromeNetworking;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEapGtcWifiAuthentication;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWA;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEcheSWADebugMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEcheSWAMeasureLatency;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kAudioSettingsPage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableBackgroundBlur;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableDesksTrackpadSwipeImprovements;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableDnsProxy;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableExternalKeyboardsInDiagnostics;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableFilesAppCopyImage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableHostnameSetting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableInputInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableKeyboardBacklightToggle;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableLazyLoginWebUILoading;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableLocalSearchService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableLogControllerForDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableNetworkingInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableOAuthIpp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableOobeChromeVoxHint;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableKioskEnrollmentInOobe;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableKioskLoginScreen;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableOobeNetworkScreenSkip;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableOobeThemeSelection;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableSamlNotificationOnPasswordChangeSuccess;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kEnableSavedDesks;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableAllSystemWebApps;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableTouchpadsInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnableTouchscreensInDiagnosticsApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEnforceAshExtensionKeeplist;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kExoHapticFeedbackSupport;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kExoLockNotification;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kExoOrdinalMotion;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kExperimentalRgbKeyboardPatterns;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFaceMLApp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFamilyLinkOnSchoolDevice;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFastPair;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFastPairLowPower;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<double> kFastPairLowPowerActiveSeconds;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<double> kFastPairLowPowerInactiveSeconds;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFastPairSoftwareScanning;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFastPairSubsequentPairingUX;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFastPairSavedDevices;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFastPairSavedDevicesStrictOptIn;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesAppExperimental;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesExtractArchive;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesInlineSyncStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFilesSinglePartitionFormat;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesTrash;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFilesWebDriveOffice;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFiltersInRecentsV2;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFirmwareUpdaterApp;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFloatingWorkspace;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFullscreenAfterUnlockAllowed;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kFullscreenAlertBubble;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFuseBox;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kFuseBoxDebug;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGlanceables;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGuestOsFiles;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGaiaReauthEndpoint;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kGamepadVibration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingGestureEditing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingLegacyRecognition;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProductivityLauncherImageSearch;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherItemColorSync;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingLegacyRecognitionAllLang;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHandwritingLibraryDlc;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppBackgroundPage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppDiscoverTabNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHelpAppLauncherSearch;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHibernate;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHiddenNetworkMigration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHiddenNetworkWarning;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHideArcMediaNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHideShelfControlsInTabletMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHindiInscriptLayout;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature
    kHoldingSpaceInProgressDownloadsNotificationSuppression;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpacePredictability;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpaceRebrand;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHoldingSpaceSuggestions;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kHotspot;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardNewHeader;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeTrayHideVoiceButton;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeOptionsInSettings;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kImeRuleConfig;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPicker;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPickerClipboard;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPickerExtension;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeSystemEmojiPickerSearchExtension;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImeStylusHandwriting;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImprovedDesksKeyboardShortcuts;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kImprovedLoginErrorHandling;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kJelly;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kJellyroll;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kKioskEnableImeButton;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosOnly;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosPrimary;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kLacrosSupport;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLacrosProfileMigrationForAnyUser;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLacrosProfileMigrationForceOff;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLacrosMoveProfileMigration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLacrosProfileBackwardMigration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherAppSort;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherFolderRenameKeepsSortOrder;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherDismissButtonsOnSortNudgeAndToast;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherHideContinueSection;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherNudgeShortInterval;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kLauncherNudgeSessionReset;
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
extern const base::Feature kMacAddressRandomization;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kManagedDeviceUIRedesign;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppCustomColors;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppPhotosIntegrationImage;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMediaAppPhotosIntegrationVideo;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMemoryPressureMetricsDetail;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FeatureParam<int> kMemoryPressureMetricsDetailLogPeriod;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kManagedTermsOfService;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kEducationEnrollmentOobeFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMicMuteNotifications;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kDisableMessagesCrossDeviceIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMinimumChromeVersion;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kMultilingualTyping;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kNearbyKeepAliveFix;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kMoreVideoCaptureBuffers;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kNewLockScreenReauthLayout;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kNightLight;
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
extern const base::Feature kOobeConsolidatedConsent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOobeHidDetectionRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOobeQuickStart;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOobeNewRecommendApps;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOobeRemoveShutdownButton;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kOsSettingsAppNotificationsPage;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kOverviewButton;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPcieBillboardNotification;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPerDeskShelf;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPerUserMetrics;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPerformantSplitViewResizing;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPersonalizationHub;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPhoneHub;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kPhoneHubCameraRoll;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPhoneHubFeatureSetupErrorHandling;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPhoneHubCallNotification;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPhoneHubMonochromeNotificationIcons;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPreferConstantFrameRate;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kPrivacyIndicators;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProductivityLauncher;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjector;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorManagedUser;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjectorAnnotator;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kProjectorAppDebug;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorExcludeTranscript;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorTutorialVideoView;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorCustomThumbnail;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorManagedUserIgnorePolicy;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorShowShortPseudoTranscript;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorUpdateIndexableText;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorUseOAuthForGetVideoInfo;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorLocalPlayback;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorBleedingEdgeExperience;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kProjectorWebReportCrash;
extern const base::Feature kProjectorUseApiKeyForTranslation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQsRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickDim;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVCBackgroundBlur;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickSettingsNetworkRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockFingerprint;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmit;
// TODO(crbug.com/1104164) - Remove this once most users have their preferences
// backfilled.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmitBackfill;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kRgbKeyboard;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesNotificationAllChannels;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseNotesSuggestionChip;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReleaseTrackUi;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kReverseScrollGestures;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kScalableStatusArea;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSeamlessRefreshRateSwitching;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSecondaryGoogleAccountUsage;
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
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSettingsAppThemeChangeAnimation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShelfAutoHideSeparation;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShelfGesturesWithVirtualKeyboard;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShelfLauncherNudge;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShelfParty;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShelfPalmRejectionSwipeOffset;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShimlessRMAFlow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShimlessRMAEnableStandalone;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShimlessRMAOsUpdate;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShimlessRMADisableDarkMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kShutdownConfirmationBubble;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kSimLockPolicy;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartDimExperimentalComponent;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartLockBluetoothScreenOffFix;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartLockSignInRemoved;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSmartLockUIRevamp;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSnoopingProtection;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kStylusBatteryStatus;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kStartAssistantAudioDecoderOnDemand;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSyncSettingsCategorization;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemChinesePhysicalTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemExtensions;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemJapanesePhysicalTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemTransliterationPhysicalTyping;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemTrayShadow;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kSystemProxyForSystemServices;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTabClusterUI;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTelemetryExtension;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTerminalAlternativeEmulator;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTerminalDev;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTerminalMultiProfile;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTerminalTmuxIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTouchTextEditingRedesign;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kTrafficCountersEnabled;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kTrilinearFiltering;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kUploadOfficeToCloud;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseAuthsessionAuthentication;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseAuthFactors;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseBluetoothSystemInAsh;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kUseLoginShelfWidget;
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
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardMultitouch;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVirtualKeyboardRoundCorners;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kVmPerBootShaderCache;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWakeOnWifiAllowed;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWallpaperFastRefresh;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWallpaperFullScreenPreview;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWallpaperGooglePhotosIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWallpaperPerDesk;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWebUITabStripTabDragIntegration;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiConnectMacAddressRandomization;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncAllowDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWifiSyncAndroid;
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::Feature kWifiSyncApplyDeletes;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWindowsFollowCursor;
COMPONENT_EXPORT(ASH_CONSTANTS) extern const base::Feature kWmMode;

// Keep alphabetized.

COMPONENT_EXPORT(ASH_CONSTANTS) bool AreCaptureModeDemoToolsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool AreContextualNudgesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool AreDesksTemplatesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool AreDesksTrackpadSwipeImprovementsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAutocompleteExtendedSuggestionsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAvatarsCloudMigrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool AreImprovedScreenCaptureSettingsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool DoWindowsFollowCursor();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAdaptiveChargingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAdaptiveChargingForTestingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAdjustSplitViewForVKEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAllowAmbientEQEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeAnimationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeDevUseProdEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAmbientModePhotoPreviewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAppNotificationsPageEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcFuseBoxFileSharingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcInputOverlayEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcInputOverlayBetaEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsArcNetworkDiagnosticsButtonEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAssistantNativeIconsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAssistiveMultiWordEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAudioSettingsPageEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsAudioPeripheralVolumeGranularityEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsAutoNightLightEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBackgroundBlurEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBentoBarEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBluetoothQualityReportEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsBluetoothRevampEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCalendarViewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCalendarModelDebugModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCaptivePortalUI2022Enabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsCheckPasswordsAgainstCryptohomeHelperEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherItemColorSyncEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsClipboardHistoryNudgeSessionResetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsClipboardHistoryReorderEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsConsumerAutoUpdateToggleAllowed();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCrosPrivacyHubEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCrosPrivacyHubDogfoodEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCrosPrivacyHubFutureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCrosPrivacyHubMVPEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCrosNextWMPEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDesksCloseAllEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCryptohomeRecoveryFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCryptohomeRecoveryFlowUIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsCryptohomeRecoverySetupEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDarkLightModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDeepLinkingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDemoModeSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsDeprecateAssistantStylusFeaturesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDeskTemplateSyncEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDisplayAlignmentAssistanceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDragUnpinnedAppToPinEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDragWindowToNewDeskEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsDriveFsMirroringEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEapGtcWifiAuthenticationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWADebugModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEcheSWAMeasureLatencyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsExperimentalRgbKeyboardPatternsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsExternalKeyboardInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFamilyLinkOnSchoolDeviceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairLowPowerEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairSoftwareScanningEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairSubsequentPairingUXEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairSavedDevicesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFastPairSavedDevicesStrictOptInEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFileManagerFuseBoxEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFileManagerFuseBoxDebugEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFilesWebDriveOfficeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFirmwareUpdaterAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFloatingWorkspaceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFullscreenAfterUnlockAllowed();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsFullscreenAlertBubbleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGaiaReauthEndpointEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool AreGlanceablesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHibernateEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHideArcMediaNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHideShelfControlsInTabletModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHoldingSpacePredictabilityEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHoldingSpaceRebrandEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHoldingSpaceSuggestionsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHostnameSettingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsHotspotEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsImprovedDesksKeyboardShortcutsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsInputNoiseCancellationUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsJellyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsJellyrollEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsKeyboardBacklightToggleEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLanguagePacksEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherAppSortEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsLauncherFolderRenameKeepsSortOrderEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsLauncherDismissButtonsOnSortNudgeAndToastEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherHideContinueSectionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherNudgeShortIntervalEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLauncherNudgeSessionResetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLicensePackagedOobeFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsLockScreenHideSensitiveNotificationsSupported();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLockScreenInlineReplyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLockScreenNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsLogControllerForDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMacAddressRandomizationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsManagedDeviceUIRedesignEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsManagedTermsOfServiceEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMicMuteNotificationsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsMinimumChromeVersionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNearbyKeepAliveFixEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNetworkingInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsEducationEnrollmentOobeFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNewLockScreenReauthLayoutEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationExpansionAnimationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsNotificationExperimentalShortTimeoutsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationScrollBarEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationsInContextMenuEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsNotificationsRefreshEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOAuthIppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeChromeVoxHintEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeHidDetectionRevampEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsKioskEnrollmentInOobeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsKioskLoginScreenEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeNetworkScreenSkipEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeConsolidatedConsentEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeQuickStartEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeNewRecommendAppsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeRemoveShutdownButtonEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsOobeThemeSelectionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPcieBillboardNotificationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPciguardUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPerDeskShelfEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubCameraRollEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsPhoneHubFeatureSetupErrorHandlingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsPhoneHubMonochromeNotificationIconsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPerformantSplitViewResizingEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPersonalizationHubEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPhoneHubCallNotificationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitBackfillFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPinAutosubmitFeatureEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPolyDevicePairingAllowed();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsPrivacyIndicatorsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProductivityLauncherEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProductivityLauncherImageSearchEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorAllUserEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorManagedUserEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorAnnotatorEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorAppDebugMode();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorExcludeTranscriptEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorTutorialVideoViewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorCustomThumbnailEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorLocalPlaybackEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsProjectorManagedUserIgnorePolicyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsProjectorShowShortPseudoTranscript();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorUpdateIndexableTextEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsProjectorUseOAuthForGetVideoInfoEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsProjectorWebReportCrashEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsProjectorUseApiKeyForTranslationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQsRevampEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsQuickDimEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsQuickSettingsNetworkRevampEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsReleaseTrackUiEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsReverseScrollGesturesEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsRgbKeyboardEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSamlNotificationOnPasswordChangeSuccessEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSavedDesksEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsScalableStatusAreaEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSeparateNetworkIconsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSettingsAppNotificationSettingsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsSettingsAppThemeChangeAnimationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShelfLauncherNudgeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShelfPalmRejectionSwipeOffsetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSimLockPolicyEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMAFlowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMAStandaloneAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMAOsUpdateEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsShimlessRMADarkModeDisabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSnoopingProtectionEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsStartAssistantAudioDecoderOnDemandEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsImeTrayHideVoiceButtonEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSyncSettingsCategorizationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsSystemTrayShadowEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsStylusBatteryStatusEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTabClusterUIEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTouchpadInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTouchscreenInDiagnosticsAppEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTrafficCountersEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsTrilinearFilteringEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUploadOfficeToCloudEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUseAuthFactorsEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUseLoginShelfWidgetEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsUseStorkSmdsServerAddressEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsVCBackgroundBlurEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWallpaperFastRefreshEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWallpaperFullScreenPreviewEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsWallpaperGooglePhotosIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWallpaperPerDeskEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWebUITabStripTabDragIntegrationEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWifiSyncAndroidEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsWmModeEnabled();
COMPONENT_EXPORT(ASH_CONSTANTS)
bool ShouldArcAndGuestOsFileTasksUseAppService();
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(ASH_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();
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
