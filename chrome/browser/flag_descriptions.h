// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/debug/debugging_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/buildflags.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.
//
// Comments are not necessary. The contents of the strings (which appear in the
// UI) should be good enough documentation for what flags do and when they
// apply. If they aren't, fix them.
//
// Sort flags in each section alphabetically by the k...Name constant. Follow
// that by the k...Description constant and any special values associated with
// that.
//
// Put #ifdefed flags in the appropriate section toward the bottom, don't
// intersperse the file with ifdefs.

namespace flag_descriptions {

// Cross-platform -------------------------------------------------------------

extern const char kAccelerated2dCanvasName[];
extern const char kAccelerated2dCanvasDescription[];

extern const char kAcceleratedVideoDecodeName[];
extern const char kAcceleratedVideoDecodeDescription[];

extern const char kAcceleratedVideoEncodeName[];
extern const char kAcceleratedVideoEncodeDescription[];

#if BUILDFLAG(ENABLE_PDF)
extern const char kAccessiblePDFFormName[];
extern const char kAccessiblePDFFormDescription[];

extern const char kPdfUseSkiaRendererName[];
extern const char kPdfUseSkiaRendererDescription[];
#endif

extern const char kAlignWakeUpsName[];
extern const char kAlignWakeUpsDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAndroidAppIntegrationName[];
extern const char kAndroidAppIntegrationDescription[];

extern const char kAndroidAppIntegrationSafeSearchName[];
extern const char kAndroidAppIntegrationSafeSearchDescription[];

extern const char kAndroidExtendedKeyboardShortcutsName[];
extern const char kAndroidExtendedKeyboardShortcutsDescription[];

extern const char kAnimatedImageResumeName[];
extern const char kAnimatedImageResumeDescription[];

extern const char kAppCollectionFolderRefreshName[];
extern const char kAppCollectionFolderRefreshDescription[];

extern const char kAriaElementReflectionName[];
extern const char kAriaElementReflectionDescription[];

extern const char kAttributionReportingDebugModeName[];
extern const char kAttributionReportingDebugModeDescription[];

extern const char kAppDeduplicationServiceFondueName[];
extern const char kAppDeduplicationServiceFondueDescription[];

extern const char kBackgroundResourceFetchName[];
extern const char kBackgroundResourceFetchDescription[];

extern const char kBrokerFileOperationsOnDiskCacheInNetworkServiceName[];
extern const char kBrokerFileOperationsOnDiskCacheInNetworkServiceDescription[];

extern const char kCertDualVerificationEnabledName[];
extern const char kCertDualVerificationEnabledDescription[];

extern const char kCOLRV1FontsDescription[];

extern const char kChromeRootStoreEnabledName[];
extern const char kChromeRootStoreEnabledDescription[];

extern const char kClickToCallName[];
extern const char kClickToCallDescription[];

extern const char kClipboardUnsanitizedContentName[];
extern const char kClipboardUnsanitizedContentDescription[];

extern const char kClipboardMaximumAgeName[];
extern const char kClipboardMaximumAgeDescription[];

extern const char kContentLanguagesInLanguagePickerName[];
extern const char kContentLanguagesInLanguagePickerDescription[];

extern const char kCustomizeChromeColorExtractionName[];
extern const char kCustomizeChromeColorExtractionDescription[];

extern const char kCustomizeChromeSidePanelName[];
extern const char KCustomizeChromeSidePanelDescription[];

extern const char kDebugHistoryInterventionNoUserActivationName[];
extern const char kDebugHistoryInterventionNoUserActivationDescription[];

extern const char kDIPSName[];
extern const char kDIPSDescription[];

extern const char kDocumentPictureInPictureApiName[];
extern const char kDocumentPictureInPictureApiDescription[];

extern const char kEnableBenchmarkingName[];
extern const char kEnableBenchmarkingDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

extern const char kGainmapHdrImagesName[];
extern const char kGainmapHdrImagesDescription[];

extern const char kIdentityStatusConsistencyName[];
extern const char kIdentityStatusConsistencyDescription[];

extern const char kTangibleSyncName[];
extern const char kTangibleSyncDescription[];

extern const char kMainThreadCompositingPriorityName[];
extern const char kMainThreadCompositingPriorityDescription[];

extern const char kPasspointARCSupportName[];
extern const char kPasspointARCSupportDescription[];

extern const char kPasspointSettingsName[];
extern const char kPasspointSettingsDescription[];

extern const char kPrivacyIndicatorsName[];
extern const char kPrivacyIndicatorsDescription[];

extern const char kEnableBluetoothSerialPortProfileInSerialApiName[];
extern const char kEnableBluetoothSerialPortProfileInSerialApiDescription[];

extern const char kEnableDrDcName[];
extern const char kEnableDrDcDescription[];

extern const char kForceGpuMainThreadToNormalPriorityDrDcName[];
extern const char kForceGpuMainThreadToNormalPriorityDrDcDescription[];

extern const char kEnableDrDcVulkanName[];

extern const char kUseClientGmbInterfaceName[];
extern const char kUseClientGmbInterfaceDescription[];

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kWebFilterInterstitialRefreshName[];
extern const char kWebFilterInterstitialRefreshDescription[];

extern const char kEnableSupervisionOnDesktopName[];
extern const char kEnableSupervisionOnDesktopDescription[];

extern const char kFilterWebsitesForSupervisedUsersOnDesktopName[];
extern const char kFilterWebsitesForSupervisedUsersOnDesktopDescription[];

extern const char kEnableExtensionsPermissionsForSupervisedUsersOnDesktopName[];
extern const char
    kEnableExtensionsPermissionsForSupervisedUsersOnDesktopDescription[];

extern const char kSupervisedPrefsControlledBySupervisedStoreName[];
extern const char kSupervisedPrefsControlledBySupervisedStoreDescription[];

extern const char kEnableManagedByParentUiName[];
extern const char kEnableManagedByParentUiDescription[];

extern const char kClearingCookiesKeepsSupervisedUsersSignedInName[];
extern const char kClearingCookiesKeepsSupervisedUsersSignedInDescription[];
#endif  // ENABLE_SUPERVISED_USERS

extern const char kUpcomingFollowFeaturesName[];
extern const char kUpcomingFollowFeaturesDescription[];

extern const char kUseAndroidStagingSmdsName[];
extern const char kUseAndroidStagingSmdsDescription[];

extern const char kUseStorkSmdsServerAddressName[];
extern const char kUseStorkSmdsServerAddressDescription[];

extern const char kUseWallpaperStagingUrlName[];
extern const char kUseWallpaperStagingUrlDescription[];

extern const char kUseMessagesStagingUrlName[];
extern const char kUseMessagesStagingUrlDescription[];

extern const char kUseCustomMessagesDomainName[];
extern const char kUseCustomMessagesDomainDescription[];

extern const char kEnableAutoDisableAccessibilityName[];
extern const char kEnableAutoDisableAccessibilityDescription[];

extern const char kEnableAutoDisableAccessibilityV2Name[];
extern const char kEnableAutoDisableAccessibilityV2Description[];

extern const char kAutofillAlwaysReturnCloudTokenizedCardName[];
extern const char kAutofillAlwaysReturnCloudTokenizedCardDescription[];

extern const char kAutofillAutoTriggerManualFallbackForCardsName[];
extern const char kAutofillAutoTriggerManualFallbackForCardsDescription[];

extern const char kAutofillEnableCvcStorageAndFillingName[];
extern const char kAutofillEnableCvcStorageAndFillingDescription[];

extern const char kAutofillEnableFIDOProgressDialogName[];
extern const char kAutofillEnableFIDOProgressDialogDescription[];

extern const char kAutofillEnableIbanClientSideUrlFilteringName[];
extern const char kAutofillEnableIbanClientSideUrlFilteringDescription[];

extern const char kAutofillEnableManualFallbackForVirtualCardsName[];
extern const char kAutofillEnableManualFallbackForVirtualCardsDescription[];

extern const char kAutofillEnableMerchantOptOutClientSideUrlFilteringName[];
extern const char
    kAutofillEnableMerchantOptOutClientSideUrlFilteringDescription[];

extern const char kAutofillEnableCardArtImageName[];
extern const char kAutofillEnableCardArtImageDescription[];

extern const char kAutofillEnableCardArtServerSideStretchingName[];
extern const char kAutofillEnableCardArtServerSideStretchingDescription[];

extern const char kAutofillEnableCardProductNameName[];
extern const char kAutofillEnableCardProductNameDescription[];

extern const char kAutofillEnableEmailOtpForVcnYellowPathName[];
extern const char kAutofillEnableEmailOtpForVcnYellowPathDescription[];

extern const char kAutofillEnableNewCardArtAndNetworkImagesName[];
extern const char kAutofillEnableNewCardArtAndNetworkImagesDescription[];

extern const char kAutofillEnableOfferNotificationForPromoCodesName[];
extern const char kAutofillEnableOfferNotificationForPromoCodesDescription[];

extern const char kAutofillEnableOffersInClankKeyboardAccessoryName[];
extern const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[];

extern const char kAutofillEnablePaymentsMandatoryReauthName[];
extern const char kAutofillEnablePaymentsMandatoryReauthDescription[];

extern const char kAutofillEnableRankingFormulaAddressProfilesName[];
extern const char kAutofillEnableRankingFormulaAddressProfilesDescription[];

extern const char kAutofillEnableRankingFormulaCreditCardsName[];
extern const char kAutofillEnableRankingFormulaCreditCardsDescription[];

extern const char kAutofillEnableRemadeDownstreamMetricsName[];
extern const char kAutofillEnableRemadeDownstreamMetricsDescription[];

extern const char kAutofillEnableStickyManualFallbackForCardsName[];
extern const char kAutofillEnableStickyManualFallbackForCardsDescription[];

extern const char kAutofillEnableSupportForLandmarkName[];
extern const char kAutofillEnableSupportForLandmarkDescription[];

extern const char kAutofillEnableUpdateVirtualCardEnrollmentName[];
extern const char kAutofillEnableUpdateVirtualCardEnrollmentDescription[];

extern const char kAutofillEnableVirtualCardFidoEnrollmentName[];
extern const char kAutofillEnableVirtualCardFidoEnrollmentDescription[];

extern const char kAutofillEnableVirtualCardName[];
extern const char kAutofillEnableVirtualCardDescription[];

extern const char kAutofillEnableNewSaveCardBubbleUiName[];
extern const char kAutofillEnableNewSaveCardBubbleUiDescription[];

extern const char
    kAutofillEnableVirtualCardManagementInDesktopSettingsPageName[];
extern const char
    kAutofillEnableVirtualCardManagementInDesktopSettingsPageDescription[];

extern const char kAutofillEnableVirtualCardMetadataName[];
extern const char kAutofillEnableVirtualCardMetadataDescription[];

extern const char kAutofillEnforceDelaysInStrikeDatabaseName[];
extern const char kAutofillEnforceDelaysInStrikeDatabaseDescription[];

extern const char kAutofillFillIbanFieldsName[];
extern const char kAutofillFillIbanFieldsDescription[];

extern const char kAutofillFillMerchantPromoCodeFieldsName[];
extern const char kAutofillFillMerchantPromoCodeFieldsDescription[];

extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[];
extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[];

extern const char kAutofillMoveLegalTermsAndIconForNewCardEnrollmentName[];
extern const char
    kAutofillMoveLegalTermsAndIconForNewCardEnrollmentDescription[];

extern const char kAutofillOfferToSaveCardWithSameLastFourName[];
extern const char kAutofillOfferToSaveCardWithSameLastFourDescription[];

extern const char kAutofillParseIBANFieldsName[];
extern const char kAutofillParseIBANFieldsDescription[];

extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[];
extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[];

extern const char kAutofillPreventOverridingPrefilledValuesName[];
extern const char kAutofillPreventOverridingPrefilledValuesDescription[];

extern const char kAutofillRemoveCardExpirationAndTypeTitlesName[];
extern const char kAutofillRemoveCardExpirationAndTypeTitlesDescription[];

extern const char kAutofillSaveAndFillVPAName[];
extern const char kAutofillSaveAndFillVPADescription[];

extern const char kAutofillShowAutocompleteDeleteButtonName[];
extern const char kAutofillShowAutocompleteDeleteButtonDescription[];

extern const char kAutofillShowManualFallbackInContextMenuName[];
extern const char kAutofillShowManualFallbackInContextMenuDescription[];

extern const char kAutofillSuggestServerCardInsteadOfLocalCardName[];
extern const char kAutofillSuggestServerCardInsteadOfLocalCardDescription[];

extern const char kAutofillTouchToFillForCreditCardsAndroidName[];
extern const char kAutofillTouchToFillForCreditCardsAndroidDescription[];

extern const char kAutofillUpstreamAllowAdditionalEmailDomainsName[];
extern const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[];

extern const char kAutofillUpstreamAllowAllEmailDomainsName[];
extern const char kAutofillUpstreamAllowAllEmailDomainsDescription[];

extern const char kAutofillUpstreamAuthenticatePreflightCallName[];
extern const char kAutofillUpstreamAuthenticatePreflightCallDescription[];

extern const char kAutofillUpstreamUseAlternateSecureDataTypeName[];
extern const char kAutofillUpstreamUseAlternateSecureDataTypeDescription[];

extern const char kAutofillMoreProminentPopupName[];
extern const char kAutofillMoreProminentPopupDescription[];

extern const char kAutofillUseImprovedLabelDisambiguationName[];
extern const char kAutofillUseImprovedLabelDisambiguationDescription[];

extern const char kAutofillVirtualCardsOnTouchToFillAndroidName[];
extern const char kAutofillVirtualCardsOnTouchToFillAndroidDescription[];

extern const char kBackForwardCacheName[];
extern const char kBackForwardCacheDescription[];

extern const char kEnableBackForwardCacheForScreenReaderName[];
extern const char kEnableBackForwardCacheForScreenReaderDescription[];

extern const char kBiometricReauthForPasswordFillingName[];
extern const char kBiometricReauthForPasswordFillingDescription[];

extern const char kFastCheckoutName[];
extern const char kFastCheckoutDescription[];

extern const char kFailFastQuietChipName[];
extern const char kFailFastQuietChipDescription[];

extern const char kBorealisBigGlName[];
extern const char kBorealisBigGlDescription[];

extern const char kBorealisDGPUName[];
extern const char kBorealisDGPUDescription[];

extern const char kBorealisDiskManagementName[];
extern const char kBorealisDiskManagementDescription[];

extern const char kBorealisForceBetaClientName[];
extern const char kBorealisForceBetaClientDescription[];

extern const char kBorealisForceDoubleScaleName[];
extern const char kBorealisForceDoubleScaleDescription[];

extern const char kBorealisLinuxModeName[];
extern const char kBorealisLinuxModeDescription[];

extern const char kBorealisPermittedName[];
extern const char kBorealisPermittedDescription[];

extern const char kBorealisStorageBallooningName[];
extern const char kBorealisStorageBallooningDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kServiceWorkerBypassFetchHandlerName[];
extern const char kServiceWorkerBypassFetchHandlerDescription[];

extern const char kServiceWorkerBypassFetchHandlerForMainResourceName[];
extern const char kServiceWorkerBypassFetchHandlerForMainResourceDescription[];

extern const char kServiceWorkerStaticRouterName[];
extern const char kServiceWorkerStaticRouterDescription[];

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
extern const char kCameraMicPreviewName[];
extern const char kCameraMicPreviewDescription[];
#endif

extern const char kCanvasOopRasterizationName[];
extern const char kCanvasOopRasterizationDescription[];

extern const char kChromeLabsName[];
extern const char kChromeLabsDescription[];

extern const char kChromeRefresh2023Id[];
extern const char kChromeRefresh2023Name[];
extern const char kChromeRefresh2023Description[];

extern const char kChromeWebuiRefresh2023Name[];
extern const char kChromeWebuiRefresh2023Description[];

extern const char kCommerceHintAndroidName[];
extern const char kCommerceHintAndroidDescription[];

extern const char kConsumerAutoUpdateToggleAllowedName[];
extern const char kConsumerAutoUpdateToggleAllowedDescription[];

extern const char kContextMenuSearchWithGoogleLensName[];
extern const char kContextMenuSearchWithGoogleLensDescription[];

extern const char kContextMenuGoogleLensSearchOptimizationsName[];
extern const char kContextMenuGoogleLensSearchOptimizationsDescription[];

extern const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[];
extern const char
    kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[];

extern const char kChromeWhatsNewUIName[];
extern const char kChromeWhatsNewUIDescription[];

extern const char kDisableProcessReuse[];
extern const char kDisableProcessReuseDescription[];

extern const char kDiscountConsentV2Name[];
extern const char kDiscountConsentV2Description[];

extern const char kDisruptiveNotificationPermissionRevocationName[];
extern const char kDisruptiveNotificationPermissionRevocationDescription[];

extern const char kDoubleBufferCompositingName[];
extern const char kDoubleBufferCompositingDescription[];

extern const char kMerchantWidePromotionsName[];
extern const char kMerchantWidePromotionsDescription[];

extern const char kCodeBasedRBDName[];
extern const char kCodeBasedRBDDescription[];

extern const char kCompressionDictionaryTransportName[];
extern const char kCompressionDictionaryTransportDescription[];

extern const char kCompressionDictionaryTransportBackendName[];
extern const char kCompressionDictionaryTransportBackendDescription[];

extern const char kUseDMSAAForTilesName[];
extern const char kUseDMSAAForTilesDescription[];

extern const char kUseDnsHttpsSvcbAlpnName[];
extern const char kUseDnsHttpsSvcbAlpnDescription[];

extern const char kEditContextName[];
extern const char kEditContextDescription[];

extern const char kEnableFirstPartySetsName[];
extern const char kEnableFirstPartySetsDescription[];

extern const char kSHA1ServerSignatureName[];
extern const char kSHA1ServerSignatureDescription[];

extern const char kEncryptedClientHelloName[];
extern const char kEncryptedClientHelloDescription[];

extern const char kIsolatedSandboxedIframesName[];
extern const char kIsolatedSandboxedIframesDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionName[];
extern const char kExperimentalAccessibilityLanguageDetectionDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionDynamicName[];
extern const char
    kExperimentalAccessibilityLanguageDetectionDynamicDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileRec2020[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileSCRGBLinear[];
extern const char kForceColorProfileHDR10[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kDynamicColorGamutName[];
extern const char kDynamicColorGamutDescription[];

extern const char kCooperativeSchedulingName[];
extern const char kCooperativeSchedulingDescription[];

extern const char kDarkenWebsitesCheckboxInThemesSettingName[];
extern const char kDarkenWebsitesCheckboxInThemesSettingDescription[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kDisallowManagedProfileSignoutName[];
extern const char kDisallowManagedProfileSignoutDescription[];
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kViewTransitionName[];
extern const char kViewTransitionDescription[];

extern const char kViewTransitionOnNavigationName[];
extern const char kViewTransitionOnNavigationDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableExperimentalCookieFeaturesName[];
extern const char kEnableExperimentalCookieFeaturesDescription[];

extern const char kEnableRawDrawName[];
extern const char kEnableRawDrawDescription[];

extern const char kEnableDelegatedCompositingName[];
extern const char kEnableDelegatedCompositingDescription[];

extern const char kEnableRemovingAllThirdPartyCookiesName[];
extern const char kEnableRemovingAllThirdPartyCookiesDescription[];

extern const char kDesktopPWAsAdditionalWindowingControlsName[];
extern const char kDesktopPWAsAdditionalWindowingControlsDescription[];

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
extern const char kDesktopPWAsAppHomePageFlagId[];
extern const char kDesktopPWAsAppHomePageName[];
extern const char kDesktopPWAsAppHomePageDescription[];
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

extern const char kDesktopPWAsElidedExtensionsMenuName[];
extern const char kDesktopPWAsElidedExtensionsMenuDescription[];

extern const char kDesktopPWAsLaunchHandlerName[];
extern const char kDesktopPWAsLaunchHandlerDescription[];

extern const char kDesktopPWAsTabStripName[];
extern const char kDesktopPWAsTabStripDescription[];

extern const char kDesktopPWAsTabStripSettingsName[];
extern const char kDesktopPWAsTabStripSettingsDescription[];

extern const char kDesktopPWAsTabStripCustomizationsName[];
extern const char kDesktopPWAsTabStripCustomizationsDescription[];

extern const char kDesktopPWAsSubAppsName[];
extern const char kDesktopPWAsSubAppsDescription[];

extern const char kDesktopPWAsWebBundlesName[];
extern const char kDesktopPWAsWebBundlesDescription[];

extern const char kDesktopPWAsScopeExtensionsName[];
extern const char kDesktopPWAsScopeExtensionsDescription[];

extern const char kDesktopPWAsBorderlessName[];
extern const char kDesktopPWAsBorderlessDescription[];

extern const char kDeviceForceScheduledRebootName[];
extern const char kDeviceForceScheduledRebootDescription[];

extern const char kDevicePostureName[];
extern const char kDevicePostureDescription[];

extern const char kEnablePreinstalledWebAppDuplicationFixerName[];
extern const char kEnablePreinstalledWebAppDuplicationFixerDescription[];

extern const char kEnableTLS13EarlyDataName[];
extern const char kEnableTLS13EarlyDataDescription[];

extern const char kEnableTLS13KyberName[];
extern const char kEnableTLS13KyberDescription[];

extern const char kAccessibilityAcceleratorNotificationsTimeoutName[];
extern const char kAccessibilityAcceleratorNotificationsTimeoutDescription[];

extern const char kAccessibilityServiceName[];
extern const char kAccessibilityServiceDescription[];

extern const char kExperimentalAccessibilityColorEnhancementSettingsName[];
extern const char
    kExperimentalAccessibilityColorEnhancementSettingsDescription[];

extern const char kAccessibilityDeprecateChromeVoxTabsName[];
extern const char kAccessibilityDeprecateChromeVoxTabsDescription[];

extern const char kAccessibilitySelectToSpeakPageMigrationName[];
extern const char kAccessibilitySelectToSpeakPageMigrationDescription[];

extern const char kAccessibilityChromeVoxPageMigrationName[];
extern const char kAccessibilityChromeVoxPageMigrationDescription[];

extern const char kAccessibilitySelectToSpeakPrefsMigrationName[];
extern const char kAccessibilitySelectToSpeakPrefsMigrationDescription[];

extern const char kAccessibilitySelectToSpeakHoverTextImprovementsName[];
extern const char kAccessibilitySelectToSpeakHoverTextImprovementsDescription[];

extern const char kAccessibilityUnserializeOptimizationsName[];
extern const char kAccessibilityUnserializeOptimizationsDescription[];

extern const char kMacCoreLocationBackendName[];
extern const char kMacCoreLocationBackendDescription[];

extern const char kNewMacNotificationAPIName[];
extern const char kNewMacNotificationAPIDescription[];

extern const char kWinrtGeolocationImplementationName[];
extern const char kWinrtGeolocationImplementationDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableTranslateSubFramesName[];
extern const char kEnableTranslateSubFramesDescription[];

extern const char kEnableWebUsbOnExtensionServiceWorkerName[];
extern const char kEnableWebUsbOnExtensionServiceWorkerDescription[];

extern const char kEnableWindowsGamingInputDataFetcherName[];
extern const char kEnableWindowsGamingInputDataFetcherDescription[];

extern const char kBlockInsecurePrivateNetworkRequestsName[];
extern const char kBlockInsecurePrivateNetworkRequestsDescription[];

extern const char kPrivateNetworkAccessSendPreflightsName[];
extern const char kPrivateNetworkAccessSendPreflightsDescription[];

extern const char kPrivateNetworkAccessRespectPreflightResultsName[];
extern const char kPrivateNetworkAccessRespectPreflightResultsDescription[];

extern const char kPrivateNetworkAccessPreflightShortTimeoutName[];
extern const char kPrivateNetworkAccessPreflightShortTimeoutDescription[];

extern const char kDeprecateAltClickName[];
extern const char kDeprecateAltClickDescription[];

extern const char kDeprecateAltBasedSixPackName[];
extern const char kDeprecateAltBasedSixPackDescription[];

extern const char kDeprecateOldKeyboardShortcutsAcceleratorName[];
extern const char kDeprecateOldKeyboardShortcutsAcceleratorDescription[];

extern const char kMemlogName[];
extern const char kMemlogDescription[];
extern const char kMemlogModeMinimal[];
extern const char kMemlogModeAll[];
extern const char kMemlogModeAllRenderers[];
extern const char kMemlogModeBrowser[];
extern const char kMemlogModeGpu[];
extern const char kMemlogModeRendererSampling[];

extern const char kMemlogSamplingRateName[];
extern const char kMemlogSamplingRateDescription[];
extern const char kMemlogSamplingRate10KB[];
extern const char kMemlogSamplingRate50KB[];
extern const char kMemlogSamplingRate100KB[];
extern const char kMemlogSamplingRate500KB[];
extern const char kMemlogSamplingRate1MB[];
extern const char kMemlogSamplingRate5MB[];

extern const char kMemlogStackModeName[];
extern const char kMemlogStackModeDescription[];
extern const char kMemlogStackModeNative[];
extern const char kMemlogStackModeNativeWithThreadNames[];

extern const char kDownloadBubbleName[];
extern const char kDownloadBubbleDescription[];

extern const char kDownloadBubbleV2Name[];
extern const char kDownloadBubbleV2Description[];

extern const char kDownloadRangeName[];
extern const char kDownloadRangeDescription[];

extern const char kEnableEnhancedSafeBrowsingSettingsImprovementsName[];
extern const char kEnableEnhancedSafeBrowsingSettingsImprovementsDescription[];

extern const char kEnableFriendlierSafeBrowsingSettingsName[];
extern const char kEnableFriendlierSafeBrowsingSettingsDescription[];

extern const char kEnableTailoredSecurityUpdatedMessagesName[];
extern const char kEnableTailoredSecurityUpdatedMessagesDescription[];

extern const char kEnableFencedFramesName[];
extern const char kEnableFencedFramesDescription[];

extern const char kEnableFencedFramesDeveloperModeName[];
extern const char kEnableFencedFramesDeveloperModeDescription[];

extern const char kEnableGamepadButtonAxisEventsName[];
extern const char kEnableGamepadButtonAxisEventsDescription[];

extern const char kEnableGamepadMultitouchName[];
extern const char kEnableGamepadMultitouchDescription[];

extern const char kEnableIsolatedWebAppsName[];
extern const char kEnableIsolatedWebAppsDescription[];

extern const char kEnableIsolatedWebAppDevModeName[];
extern const char kEnableIsolatedWebAppDevModeDescription[];

extern const char kEnableIwaControlledFrameName[];
extern const char kEnableIwaControlledFrameDescription[];

extern const char kEnableLensStandaloneFlagId[];
extern const char kEnableLensStandaloneName[];
extern const char kEnableLensStandaloneDescription[];

extern const char kEnableManagedConfigurationWebApiName[];
extern const char kEnableManagedConfigurationWebApiDescription[];

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePenetratingImageSelectionName[];
extern const char kEnablePenetratingImageSelectionDescription[];

extern const char kEnablePerfettoSystemTracingName[];
extern const char kEnablePerfettoSystemTracingDescription[];

extern const char kEnablePeripheralCustomizationName[];
extern const char kEnablePeripheralCustomizationDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableProcessPerSiteUpToMainFrameThresholdName[];
extern const char kEnableProcessPerSiteUpToMainFrameThresholdDescription[];

extern const char kEnableShortcutCustomizationAppName[];
extern const char kEnableShortcutCustomizationAppDescription[];

extern const char kEnableShortcutCustomizationName[];
extern const char kEnableShortcutCustomizationDescription[];

extern const char kEnableSearchCustomizableShortcutsInLauncherName[];
extern const char kEnableSearchCustomizableShortcutsInLauncherDescription[];

extern const char kEnableSearchInShortcutsAppName[];
extern const char kEnableSearchInShortcutsAppDescription[];

extern const char kEnableInputDeviceSettingsSplitName[];
extern const char kEnableInputDeviceSettingsSplitDescription[];

extern const char kExperimentalRgbKeyboardPatternsName[];
extern const char kExperimentalRgbKeyboardPatternsDescription[];

extern const char kRetailCouponsName[];
extern const char kRetailCouponsDescription[];

extern const char kDropInputEventsBeforeFirstPaintName[];
extern const char kDropInputEventsBeforeFirstPaintDescription[];

extern const char kEnableCssSelectorFragmentAnchorName[];
extern const char kEnableCssSelectorFragmentAnchorDescription[];

extern const char kEnablePreferencesAccountStorageName[];
extern const char kEnablePreferencesAccountStorageDescription[];

extern const char kEnableResamplingScrollEventsExperimentalPredictionName[];
extern const char
    kEnableResamplingScrollEventsExperimentalPredictionDescription[];

extern const char kEnableRestrictedWebApisName[];
extern const char kEnableRestrictedWebApisDescription[];

extern const char kEnableWebAuthenticationChromeOSAuthenticatorName[];
extern const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[];

extern const char kEnableZeroCopyTabCaptureName[];
extern const char kEnableZeroCopyTabCaptureDescription[];

extern const char kExperimentalWebAssemblyFeaturesName[];
extern const char kExperimentalWebAssemblyFeaturesDescription[];

extern const char kExperimentalWebAssemblyJSPIName[];
extern const char kExperimentalWebAssemblyJSPIDescription[];

extern const char kEnablePolicyTestPageName[];
extern const char kEnablePolicyTestPageDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmGarbageCollectionName[];
extern const char kEnableWasmGarbageCollectionDescription[];

extern const char kEnableWasmLazyCompilationName[];
extern const char kEnableWasmLazyCompilationDescription[];

extern const char kEnableWasmRelaxedSimdName[];
extern const char kEnableWasmRelaxedSimdDescription[];

extern const char kEnableWasmStringrefName[];
extern const char kEnableWasmStringrefDescription[];

extern const char kEnableWasmTieringName[];
extern const char kEnableWasmTieringDescription[];

extern const char kEvDetailsInPageInfoName[];
extern const char kEvDetailsInPageInfoDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kExtensionsMenuAccessControlName[];
extern const char kExtensionsMenuAccessControlDescription[];
extern const char kWebViewTagMPArchBehaviorName[];
extern const char kWebViewTagMPArchBehaviorDescription[];

extern const char kWebAuthFlowInBrowserTabName[];
extern const char kWebAuthFlowInBrowserTabDescription[];

extern const char kCWSInfoFastCheckName[];
extern const char kCWSInfoFastCheckDescription[];

extern const char kSafetyCheckExtensionsName[];
extern const char kSafetyCheckExtensionsDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kExtensionWebFileHandlersName[];
extern const char kExtensionWebFileHandlersDescription[];
#endif  // IS_CHROMEOS_ASH
#endif  // ENABLE_EXTENSIONS

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFractionalScrollOffsetsName[];
extern const char kFractionalScrollOffsetsDescription[];

extern const char kFedCmName[];
extern const char kFedCmDescription[];

extern const char kFedCmAuthzName[];
extern const char kFedCmAuthzDescription[];

extern const char kFedCmAutoReauthnName[];
extern const char kFedCmAutoReauthnDescription[];

extern const char kFedCmIdPRegistrationName[];
extern const char kFedCmIdPRegistrationDescription[];

extern const char kFedCmIframeSupportName[];
extern const char kFedCmIframeSupportDescription[];

extern const char kFedCmLoginHintName[];
extern const char kFedCmLoginHintDescription[];

extern const char kFedCmMetricsEndpointName[];
extern const char kFedCmMetricsEndpointDescription[];

extern const char kFedCmMultiIdpName[];
extern const char kFedCmMultiIdpDescription[];

extern const char kFedCmSelectiveDisclosureName[];
extern const char kFedCmSelectiveDisclosureDescription[];

extern const char kFedCmRpContextName[];
extern const char kFedCmRpContextDescription[];

extern const char kFedCmUserInfoName[];
extern const char kFedCmUserInfoDescription[];

extern const char kFedCmIdpSigninStatusName[];
extern const char kFedCmIdpSigninStatusDescription[];

extern const char kFedCmWithoutThirdPartyCookiesName[];
extern const char kFedCmWithoutThirdPartyCookiesDescription[];

extern const char kWebIdentityMDocsName[];
extern const char kWebIdentityMDocsDescription[];

extern const char kFileHandlingIconsName[];
extern const char kFileHandlingIconsDescription[];

extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

extern const char kFillingAcrossGroupedSitesName[];
extern const char kFillingAcrossGroupedSitesDescription[];

extern const char kMutationEventsName[];
extern const char kMutationEventsDescription[];

extern const char kHTMLPopoverAttributeName[];
extern const char kHTMLPopoverAttributeDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFullscreenPopupWindowsName[];
extern const char kFullscreenPopupWindowsDescription[];

extern const char kGalleryAppPdfEditNotificationName[];
extern const char kGalleryAppPdfEditNotificationDescription[];

extern const char kMediaRemotingWithoutFullscreenName[];
extern const char kMediaRemotingWithoutFullscreenDescription[];

#if BUILDFLAG(IS_CHROMEOS)
extern const char kGlobalMediaControlsCrOSUpdatedUIName[];
extern const char kGlobalMediaControlsCrOSUpdatedUIDescription[];
#endif

extern const char kGoogleOneOfferFilesBannerName[];
extern const char kGoogleOneOfferFilesBannerDescription[];

extern const char kOpenscreenCastStreamingSessionName[];
extern const char kOpenscreenCastStreamingSessionDescription[];

extern const char kCastStreamingAv1Name[];
extern const char kCastStreamingAv1Description[];

extern const char kCastStreamingHardwareH264Name[];
extern const char kCastStreamingHardwareH264Description[];

extern const char kCastStreamingHardwareVp8Name[];
extern const char kCastStreamingHardwareVp8Description[];

extern const char kCastStreamingPerformanceOverlayName[];
extern const char kCastStreamingPerformanceOverlayDescription[];

extern const char kCastStreamingVp9Name[];
extern const char kCastStreamingVp9Description[];

extern const char kCastEnableStreamingWithHiDPIName[];
extern const char kCastEnableStreamingWithHiDPIDescription[];

extern const char kContextualPageActionsName[];
extern const char kContextualPageActionsDescription[];

extern const char kContextualPageActionsPriceTrackingName[];
extern const char kContextualPageActionsPriceTrackingDescription[];

extern const char kContextualPageActionsReaderModeName[];
extern const char kContextualPageActionsReaderModeDescription[];

extern const char kContextualPageActionsShareModelName[];
extern const char kContextualPageActionsShareModelDescription[];

extern const char kEnableOsIntegrationSubManagersName[];
extern const char kEnableOsIntegrationSubManagersDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];

extern const char kHandwritingGestureEditingName[];
extern const char kHandwritingGestureEditingDescription[];

extern const char kHandwritingLegacyRecognitionName[];
extern const char kHandwritingLegacyRecognitionDescription[];

extern const char kHandwritingLibraryDlcName[];
extern const char kHandwritingLibraryDlcDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHeavyAdPrivacyMitigationsName[];
extern const char kHeavyAdPrivacyMitigationsDescription[];

extern const char kHeavyAdInterventionName[];
extern const char kHeavyAdInterventionDescription[];

extern const char kTabAudioMutingName[];
extern const char kTabAudioMutingDescription[];

extern const char kCrasSplitAlsaUsbInternalName[];
extern const char kCrasSplitAlsaUsbInternalDescription[];

extern const char kRestoreTabsOnFREName[];
extern const char kRestoreTabsOnFREDescription[];

extern const char kStartSurfaceReturnTimeName[];
extern const char kStartSurfaceReturnTimeDescription[];

extern const char kStorageBucketsName[];
extern const char kStorageBucketsDescription[];

extern const char kHiddenNetworkMigrationName[];
extern const char kHiddenNetworkMigrationDescription[];

extern const char kHttpsOnlyModeName[];
extern const char kHttpsOnlyModeDescription[];

extern const char kHttpsFirstModeV2Name[];
extern const char kHttpsFirstModeV2Description[];

extern const char kHttpsFirstModeV2ForEngagedSitesName[];
extern const char kHttpsFirstModeV2ForEngagedSitesDescription[];

extern const char kHttpsUpgradesName[];
extern const char kHttpsUpgradesDescription[];

extern const char kIgnoreGpuBlocklistName[];
extern const char kIgnoreGpuBlocklistDescription[];

extern const char kIgnoreSyncEncryptionKeysLongMissingName[];
extern const char kIgnoreSyncEncryptionKeysLongMissingDescription[];

extern const char kImprovedIncognitoScreenshotName[];
extern const char kImprovedIncognitoScreenshotDescription[];

extern const char kImprovedKeyboardShortcutsName[];
extern const char kImprovedKeyboardShortcutsDescription[];

extern const char kIncognitoReauthenticationForAndroidName[];
extern const char kIncognitoReauthenticationForAndroidDescription[];

extern const char kIncognitoDownloadsWarningName[];
extern const char kIncognitoDownloadsWarningDescription[];

extern const char kIncognitoNtpRevampName[];
extern const char kIncognitoNtpRevampDescription[];

extern const char kIncognitoScreenshotName[];
extern const char kIncognitoScreenshotDescription[];

extern const char kInfobarScrollOptimizationName[];
extern const char kInfobarScrollOptimizationDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kInProductHelpSnoozeName[];
extern const char kInProductHelpSnoozeDescription[];

extern const char kInProductHelpUseClientConfigName[];
extern const char kInProductHelpUseClientConfigDescription[];

extern const char kInsecureDownloadWarningsName[];
extern const char kInsecureDownloadWarningsDescription[];

extern const char kInstallIsolatedWebAppFromUrl[];
extern const char kInstallIsolatedWebAppFromUrlDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kJavascriptExperimentalSharedMemoryName[];
extern const char kJavascriptExperimentalSharedMemoryDescription[];

extern const char kJourneysName[];
extern const char kJourneysDescription[];

extern const char kRenameJourneysName[];
extern const char kRenameJourneysDescription[];

extern const char kJourneysContentClusteringName[];
extern const char kJourneysContentClusteringDescription[];

extern const char kJourneysImagesName[];
extern const char kJourneysImagesDescription[];

extern const char kJourneysLabelsName[];
extern const char kJourneysLabelsDescription[];

extern const char kJourneysOmniboxActionName[];
extern const char kJourneysOmniboxActionDescription[];

extern const char kJourneysOmniboxHistoryClusterProviderName[];
extern const char kJourneysOmniboxHistoryClusterProviderDescription[];

extern const char kJourneysPersistCachesToPrefsName[];
extern const char kJourneysPersistCachesToPrefsDescription[];

extern const char kJourneysShowAllClustersName[];
extern const char kJourneysShowAllClustersDescription[];

extern const char kJourneysIncludeSyncedVisitsName[];
extern const char kJourneysIncludeSyncedVisitsDescription[];

extern const char kJourneysZeroStateFilteringName[];
extern const char kJourneysZeroStateFilteringDescription[];

extern const char kExtractRelatedSearchesFromPrefetchedZPSResponseName[];
extern const char kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[];

extern const char kLargeFaviconFromGoogleName[];
extern const char kLargeFaviconFromGoogleDescription[];

extern const char kLensCameraAssistedSearchName[];
extern const char kLensCameraAssistedSearchDescription[];

extern const char kLensRegionSearchStaticPageName[];
extern const char kLensRegionSearchStaticPageDescription[];

extern const char kLensImageFormatOptimizationsName[];
extern const char kLensImageFormatOptimizationsDescription[];

extern const char kLensImageTranslateName[];
extern const char kLensImageTranslateDescription[];

extern const char kEnableLensPingName[];
extern const char kEnableLensPingDescription[];

extern const char kCscName[];
extern const char kCscDescription[];

extern const char kCscPinnedName[];
extern const char kCscPinnedDescription[];

extern const char kCscVssName[];
extern const char kCscVssDescription[];

extern const char kLensOnQuickActionSearchWidgetName[];
extern const char kLensOnQuickActionSearchWidgetDescription[];

extern const char kLocationBarModelOptimizationsName[];
extern const char kLocationBarModelOptimizationsDescription[];

extern const char kLogJsConsoleMessagesName[];
extern const char kLogJsConsoleMessagesDescription[];

extern const char kUnthrottledNestedTimeoutName[];
extern const char kUnthrottledNestedTimeoutDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMetricsSettingsAndroidName[];
extern const char kMetricsSettingsAndroidDescription[];

extern const char kMojoLinuxChannelSharedMemName[];
extern const char kMojoLinuxChannelSharedMemDescription[];

extern const char kUsernameFirstFlowFallbackCrowdsourcingName[];
extern const char kUsernameFirstFlowFallbackCrowdsourcingDescription[];

extern const char kCanvas2DLayersName[];
extern const char kCanvas2DLayersDescription[];

extern const char kEnableMachineLearningModelLoaderWebPlatformApiName[];
extern const char kEnableMachineLearningModelLoaderWebPlatformApiDescription[];

extern const char kDestroyProfileOnBrowserCloseName[];
extern const char kDestroyProfileOnBrowserCloseDescription[];

extern const char kDestroySystemProfilesName[];
extern const char kDestroySystemProfilesDescription[];

extern const char kNotificationInteractionHistoryName[];
extern const char kNotificationInteractionHistoryDescription[];

extern const char kNotificationSchedulerName[];
extern const char kNotificationSchedulerDescription[];

extern const char kNotificationSchedulerDebugOptionName[];
extern const char kNotificationSchedulerDebugOptionDescription[];
extern const char kNotificationSchedulerImmediateBackgroundTaskDescription[];

extern const char kNotificationsSystemFlagName[];
extern const char kNotificationsSystemFlagDescription[];

extern const char kOrganicRepeatableQueriesName[];
extern const char kOrganicRepeatableQueriesDescription[];

extern const char kOriginAgentClusterDefaultName[];
extern const char kOriginAgentClusterDefaultDescription[];

extern const char kOriginKeyedProcessesByDefaultName[];
extern const char kOriginKeyedProcessesByDefaultDescription[];

extern const char kOmitCorsClientCertName[];
extern const char kOmitCorsClientCertDescription[];

extern const char kOmniboxActionsInSuggestName[];
extern const char kOmniboxActionsInSuggestDescription[];

extern const char kOmniboxAdaptiveSuggestionsCountName[];
extern const char kOmniboxAdaptiveSuggestionsCountDescription[];

extern const char
    kOmniboxAdaptiveSuggestionsVisibleGroupEligibilityUpdateName[];
extern const char
    kOmniboxAdaptiveSuggestionsVisibleGroupEligibilityUpdateDescription[];

extern const char kOmniboxAdaptNarrowTabletWindowsName[];
extern const char kOmniboxAdaptNarrowTabletWindowsDescription[];

extern const char kOmniboxCacheSuggestionResourcesName[];
extern const char kOmniboxCacheSuggestionResourcesDescription[];

extern const char kOmniboxConsumesImeInsetsName[];
extern const char kOmniboxConsumesImeInsetsDescription[];

extern const char kOmniboxCR23ActionChipsName[];
extern const char kOmniboxCR23ActionChipsDescription[];

extern const char kOmniboxCR23ActionChipsIconsName[];
extern const char kOmniboxCR23ActionChipsIconsDescription[];

extern const char kOmniboxCR23ExpandedStateColorsName[];
extern const char kOmniboxCR23ExpandedStateColorsDescription[];

extern const char kOmniboxCR23ExpandedStateHeightName[];
extern const char kOmniboxCR23ExpandedStateHeightDescription[];

extern const char kOmniboxCR23ExpandedStateLayoutName[];
extern const char kOmniboxCR23ExpandedStateLayoutDescription[];

extern const char kOmniboxCR23ExpandedStateShapeName[];
extern const char kOmniboxCR23ExpandedStateShapeDescription[];

extern const char kOmniboxCR23ExpandedStateSuggestIconsName[];
extern const char kOmniboxCR23ExpandedStateSuggestIconsDescription[];

extern const char kOmniboxCR23SteadyStateIconsName[];
extern const char kOmniboxCR23SteadyStateIconsDescription[];

extern const char kOmniboxCR23SuggestionHoverFillShapeName[];
extern const char kOmniboxCR23SuggestionHoverFillShapeDescription[];

extern const char kOmniboxIgnoreIntermediateResultsName[];
extern const char kOmniboxIgnoreIntermediateResultsDescription[];

extern const char kOmniboxDiscardTemporaryInputOnTabSwitchName[];
extern const char kOmniboxDiscardTemporaryInputOnTabSwitchDescription[];

extern const char kOmniboxDomainSuggestionsName[];
extern const char kOmniboxDomainSuggestionsDescription[];

extern const char kOmniboxFuzzyUrlSuggestionsName[];
extern const char kOmniboxFuzzyUrlSuggestionsDescription[];

extern const char kOmniboxGM3SteadyStateBackgroundColorName[];
extern const char kOmniboxGM3SteadyStateBackgroundColorDescription[];

extern const char kOmniboxGM3SteadyStateHeightName[];
extern const char kOmniboxGM3SteadyStateHeightDescription[];

extern const char kOmniboxGM3SteadyStateTextStyleName[];
extern const char kOmniboxGM3SteadyStateTextStyleDescription[];

extern const char kOmniboxGM3SteadyStateTextColorName[];
extern const char kOmniboxGM3SteadyStateTextColorDescription[];

extern const char kOmniboxGroupingFrameworkNonZPSName[];
extern const char kOmniboxGroupingFrameworkZPSName[];
extern const char kOmniboxGroupingFrameworkDescription[];

extern const char kOmniboxInspireMeName[];
extern const char kOmniboxInspireMeDescription[];

extern const char kOmniboxMlLogUrlScoringSignalsName[];
extern const char kOmniboxMlLogUrlScoringSignalsDescription[];

extern const char kOmniboxMlUrlScoringName[];
extern const char kOmniboxMlUrlScoringDescription[];

extern const char kOmniboxMlUrlScoringModelName[];
extern const char kOmniboxMlUrlScoringModelDescription[];

extern const char kOmniboxModernizeVisualUpdateName[];
extern const char kOmniboxModernizeVisualUpdateDescription[];

extern const char kOmniboxMatchToolbarAndStatusBarColorName[];
extern const char kOmniboxMatchToolbarAndStatusBarColorDescription[];

extern const char kOmniboxMostVisitedTilesName[];
extern const char kOmniboxMostVisitedTilesDescription[];

extern const char kOmniboxMostVisitedTilesAddRecycledViewPoolName[];
extern const char kOmniboxMostVisitedTilesAddRecycledViewPoolDescription[];

extern const char kOmniboxMostVisitedTilesTitleWrapAroundName[];
extern const char kOmniboxMostVisitedTilesTitleWrapAroundDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsName[];
extern const char kOmniboxOnDeviceHeadSuggestionsDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

extern const char kOmniboxOnDeviceTailSuggestionsName[];
extern const char kOmniboxOnDeviceTailSuggestionsDescription[];

extern const char kOmniboxRedoCurrentMatchName[];
extern const char kOmniboxRedoCurrentMatchDescription[];

extern const char kOmniboxSuppressClipboardSuggestionAfterFirstUsedName[];
extern const char
    kOmniboxSuppressClipboardSuggestionAfterFirstUsedDescription[];

extern const char kOmniboxWarmRecycledViewPoolName[];
extern const char kOmniboxWarmRecycledViewPoolDescription[];

extern const char kOmniboxRevertModelBeforeClosingPopupName[];
extern const char kOmniboxRevertModelBeforeClosingPopupDescription[];

extern const char kOmniboxUseExistingAutocompleteClientName[];
extern const char kOmniboxUseExistingAutocompleteClientDescription[];

extern const char kOmniboxRichAutocompletionPromisingName[];
extern const char kOmniboxRichAutocompletionPromisingDescription[];

extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[];
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[];

extern const char kOmniboxFocusTriggersSRPZeroSuggestName[];
extern const char kOmniboxFocusTriggersSRPZeroSuggestDescription[];

extern const char kOmniboxFocusTriggersContextualWebZeroSuggestName[];
extern const char kOmniboxFocusTriggersContextualWebZeroSuggestDescription[];

extern const char kOmniboxOnClobberFocusTypeOnContentName[];
extern const char kOmniboxOnClobberFocusTypeOnContentDescription[];

extern const char kOmniboxShortcutBoostName[];
extern const char kOmniboxShortcutBoostDescription[];

extern const char kOmniboxShortcutExpandingName[];
extern const char kOmniboxShortcutExpandingDescription[];

extern const char kOmniboxSimplifiedUiUniformRowHeightName[];
extern const char kOmniboxSimplifiedUiUniformRowHeightDescription[];

extern const char kOmniboxSimplifiedUiSquareSuggestIconName[];
extern const char kOmniboxSimplifiedUiSquareSuggestIconDescription[];

extern const char kOmniboxZeroSuggestPrefetchingName[];
extern const char kOmniboxZeroSuggestPrefetchingDescription[];

extern const char kOmniboxReportAssistedQueryStatsName[];
extern const char kOmniboxReportAssistedQueryStatsDescription[];

extern const char kOmniboxReportSearchboxStatsName[];
extern const char kOmniboxReportSearchboxStatsDescription[];

extern const char kOmniboxZeroSuggestPrefetchingOnSRPName[];
extern const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[];

extern const char kOmniboxZeroSuggestPrefetchingOnWebName[];
extern const char kOmniboxZeroSuggestPrefetchingOnWebDescription[];

extern const char kOmniboxZeroSuggestInMemoryCachingName[];
extern const char kOmniboxZeroSuggestInMemoryCachingDescription[];

extern const char kOmniboxMaxZeroSuggestMatchesName[];
extern const char kOmniboxMaxZeroSuggestMatchesDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxUpdatedConnectionSecurityIndicatorsName[];
extern const char kOmniboxUpdatedConnectionSecurityIndicatorsDescription[];

extern const char kWebUIOmniboxPopupName[];
extern const char kWebUIOmniboxPopupDescription[];

#if !BUILDFLAG(IS_LINUX)
extern const char kWebUiSystemFontName[];
extern const char kWebUiSystemFontDescription[];
#endif

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxDynamicMaxAutocompleteName[];
extern const char kOmniboxDynamicMaxAutocompleteDescription[];

extern const char kOnlyShowNewShortcutsAppName[];
extern const char kOnlyShowNewShortcutsAppDescription[];

extern const char kOneTimePermissionName[];
extern const char kOneTimePermissionDescription[];

extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

extern const char kOptimizationGuideInstallWideModelStoreName[];
extern const char kOptimizationGuideInstallWideModelStoreDescription[];

extern const char kOptimizationGuidePushNotificationName[];
extern const char kOptimizationGuidePushNotificationDescription[];

extern const char kOsFeedbackJellyName[];
extern const char kOsFeedbackJellyDescription[];

extern const char kOsSettingsAppNotificationsPageName[];
extern const char kOsSettingsAppNotificationsPageDescription[];

extern const char kOsSettingsDeprecateSyncMetricsToggleName[];
extern const char kOsSettingsDeprecateSyncMetricsToggleDescription[];

extern const char kOverviewButtonName[];
extern const char kOverviewButtonDescription[];

extern const char kOverviewDeskNavigationName[];
extern const char kOverviewDeskNavigationDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kOverlayStrategiesName[];
extern const char kOverlayStrategiesDescription[];
extern const char kOverlayStrategiesDefault[];
extern const char kOverlayStrategiesNone[];
extern const char kOverlayStrategiesUnoccludedFullscreen[];
extern const char kOverlayStrategiesUnoccluded[];
extern const char kOverlayStrategiesOccludedAndUnoccluded[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kPageContentAnnotationsName[];
extern const char kPageContentAnnotationsDescription[];

extern const char kPageContentAnnotationsPersistSalientImageMetadataName[];
extern const char
    kPageContentAnnotationsPersistSalientImageMetadataDescription[];

extern const char kPageContentAnnotationsRemotePageMetadataName[];
extern const char kPageContentAnnotationsRemotePageMetadataDescription[];

extern const char kPageEntitiesPageContentAnnotationsName[];
extern const char kPageEntitiesPageContentAnnotationsDescription[];

extern const char kPageImageServiceOptimizationGuideSalientImagesName[];
extern const char kPageImageServiceOptimizationGuideSalientImagesDescription[];

extern const char kPageImageServiceSuggestPoweredImagesName[];
extern const char kPageImageServiceSuggestPoweredImagesDescription[];

extern const char kPageInfoAboutThisSiteKeepSidePanelOnSameTabNavs[];
extern const char kPageInfoAboutThisSiteKeepSidePanelOnSameTabNavsDescription[];

extern const char kPageInfoAboutThisSiteNewIconName[];
extern const char kPageInfoAboutThisSiteNewIconDescription[];

extern const char kPageInfoAboutThisSiteNonEnName[];
extern const char kPageInfoAboutThisSiteNonEnDescription[];

extern const char kPageInfoboutThisPageDescriptionPlaceholderName[];
extern const char kPageInfoboutThisPageDescriptionPlaceholderDescription[];

extern const char kPageInfoboutThisPagePersistentEntryName[];
extern const char kPageInfoboutThisPagePersistentEntryDescription[];

extern const char kPageInfoCookiesSubpageName[];
extern const char kPageInfoCookiesSubpageDescription[];

extern const char kPageInfoMoreAboutThisPageName[];
extern const char kPageInfoMoreAboutThisPageDescription[];

extern const char kPageInfoHideSiteSettingsName[];
extern const char kPageInfoHideSiteSettingsDescription[];

extern const char kPageInfoHistoryDesktopName[];
extern const char kPageInfoHistoryDesktopDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPasswordGenerationExperimentName[];
extern const char kPasswordGenerationExperimentDescription[];

extern const char kPasswordsImportM2Name[];
extern const char kPasswordsImportM2Description[];

extern const char kForceEnableFastCheckoutCapabilitiesName[];
extern const char kForceEnableFastCheckoutCapabilitiesDescription[];

extern const char kPdfOcrName[];
extern const char kPdfOcrDescription[];

extern const char kPdfXfaFormsName[];
extern const char kPdfXfaFormsDescription[];

extern const char kBookmarksImprovedSaveFlowName[];
extern const char kBookmarksImprovedSaveFlowDescription[];

extern const char kBookmarksRefreshName[];
extern const char kBookmarksRefreshDescription[];

extern const char kAutoWebContentsDarkModeName[];
extern const char kAutoWebContentsDarkModeDescription[];

extern const char kForcedColorsName[];
extern const char kForcedColorsDescription[];

extern const char kWindowsScrollingPersonalityName[];
extern const char kWindowsScrollingPersonalityDescription[];

extern const char kPermissionChipName[];
extern const char kPermissionChipDescription[];

extern const char kChipLocationBarIconOverrideName[];
extern const char kChipLocationBarIconOverrideDescription[];

extern const char kConfirmationChipName[];
extern const char kConfirmationChipNameDescription[];

extern const char kPermissionPredictionsName[];
extern const char kPermissionPredictionsDescription[];

extern const char kPermissionQuietChipName[];
extern const char kPermissionQuietChipDescription[];

extern const char kRecordPermissionExpirationTimestampsName[];
extern const char kRecordPermissionExpirationTimestampsDescription[];

extern const char kPowerBookmarkBackendName[];
extern const char kPowerBookmarkBackendDescription[];

extern const char kPowerBookmarksSidePanel[];
extern const char kPowerBookmarksSidePanelDescription[];

extern const char kOmniboxTriggerForPrerender2Name[];
extern const char kOmniboxTriggerForPrerender2Description[];

extern const char kSupportSearchSuggestionForPrerender2Name[];
extern const char kSupportSearchSuggestionForPrerender2Description[];

extern const char kEnableOmniboxSearchPrefetchName[];
extern const char kEnableOmniboxSearchPrefetchDescription[];

extern const char kEnableOmniboxClientSearchPrefetchName[];
extern const char kEnableOmniboxClientSearchPrefetchDescription[];

extern const char kPrivacyGuideAndroidName[];
extern const char kPrivacyGuideAndroidDescription[];

extern const char kPrivacySandboxAdsAPIsOverrideName[];
extern const char kPrivacySandboxAdsAPIsOverrideDescription[];

extern const char kPrivacySandboxEnrollmentOverridesName[];
extern const char kPrivacySandboxEnrollmentOverridesDescription[];

extern const char kPrivacySandboxSettings4Name[];
extern const char kPrivacySandboxSettings4Description[];

extern const char kPrivateAggregationDeveloperModeName[];
extern const char kPrivateAggregationDeveloperModeDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kPWAsDefaultOfflinePageName[];
extern const char kPWAsDefaultOfflinePageDescription[];

extern const char kPwaUpdateDialogForAppIconName[];
extern const char kPwaUpdateDialogForAppIconDescription[];

extern const char kRenderDocumentName[];
extern const char kRenderDocumentDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kQuickAppAccessTestUIName[];
extern const char kQuickAppAccessTestUIDescription[];

extern const char kQuickDimName[];
extern const char kQuickDimDescription[];

extern const char kQuickIntensiveWakeUpThrottlingAfterLoadingName[];
extern const char kQuickIntensiveWakeUpThrottlingAfterLoadingDescription[];

extern const char kQuickDeleteForAndroidName[];
extern const char kQuickDeleteForAndroidDescription[];

extern const char kAppListDragAndDropRefactorName[];
extern const char kAppListDragAndDropRefactorDescription[];

extern const char kSettingsAppNotificationSettingsName[];
extern const char kSettingsAppNotificationSettingsDescription[];

extern const char kReadLaterFlagId[];
extern const char kReadLaterName[];
extern const char kReadLaterDescription[];

extern const char kRecordWebAppDebugInfoName[];
extern const char kRecordWebAppDebugInfoDescription[];

extern const char kRestrictGamepadAccessName[];
extern const char kRestrictGamepadAccessDescription[];

extern const char kRoundedDisplay[];
extern const char kRoundedDisplayDescription[];

extern const char kRoundedWindows[];
extern const char kRoundedWindowsDescription[];

extern const char kMBIModeName[];
extern const char kMBIModeDescription[];

extern const char kSafetyCheckNotificationPermissionsName[];
extern const char kSafetyCheckNotificationPermissionsDescription[];

extern const char kSafetyCheckUnusedSitePermissionsName[];
extern const char kSafetyCheckUnusedSitePermissionsDescription[];

extern const char kSafetyHubName[];
extern const char kSafetyHubDescription[];

extern const char kSameAppWindowCycleName[];
extern const char kSameAppWindowCycleDescription[];

extern const char kPartitionedCookiesName[];
extern const char kPartitionedCookiesDescription[];

extern const char kThirdPartyStoragePartitioningName[];
extern const char kThirdPartyStoragePartitioningDescription[];

extern const char kScreenSaverDurationName[];
extern const char kScreenSaverDurationDescription[];

extern const char kScreenSaverPreviewName[];
extern const char kScreenSaverPreviewDescription[];

extern const char kScrollableTabStripFlagId[];
extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kTabScrollingButtonPositionFlagId[];
extern const char kTabScrollingButtonPositionName[];
extern const char kTabScrollingButtonPositionDescription[];

extern const char kScrollableTabStripWithDraggingFlagId[];
extern const char kScrollableTabStripWithDraggingName[];
extern const char kScrollableTabStripWithDraggingDescription[];

extern const char kScrollableTabStripOverflowFlagId[];
extern const char kScrollableTabStripOverflowName[];
extern const char kScrollableTabStripOverflowDescription[];

extern const char kSplitTabStripName[];
extern const char kSplitTabStripDescription[];

extern const char kScrollUnificationName[];
extern const char kScrollUnificationDescription[];

extern const char kSearchResultInlineIconName[];
extern const char kSearchResultInlineIconDescription[];

extern const char kDynamicSearchUpdateAnimationName[];
extern const char kDynamicSearchUpdateAnimationDescription[];

extern const char kSecurePaymentConfirmationDebugName[];
extern const char kSecurePaymentConfirmationDebugDescription[];

extern const char kSendTabToSelfSigninPromoName[];
extern const char kSendTabToSelfSigninPromoDescription[];

extern const char kSidePanelJourneysFlagId[];
extern const char kSidePanelJourneysName[];
extern const char kSidePanelJourneysDescription[];

extern const char kSidePanelJourneysQuerylessFlagId[];
extern const char kSidePanelJourneysQuerylessName[];
extern const char kSidePanelJourneysQuerylessDescription[];

extern const char kSharingDesktopScreenshotsName[];
extern const char kSharingDesktopScreenshotsDescription[];

extern const char kShelfStackedHotseatName[];
extern const char kShelfStackedHotseatDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kShowPerformanceMetricsHudName[];
extern const char kShowPerformanceMetricsHudDescription[];

extern const char kShowOverdrawFeedbackName[];
extern const char kShowOverdrawFeedbackDescription[];

extern const char kSkiaRendererName[];
extern const char kSkiaRendererDescription[];

extern const char kStorageAccessAPIName[];
extern const char kStorageAccessAPIDescription[];

extern const char kIsolateOriginsName[];
extern const char kIsolateOriginsDescription[];

extern const char kIsolationByDefaultName[];
extern const char kIsolationByDefaultDescription[];

extern const char kSiteIsolationOptOutName[];
extern const char kSiteIsolationOptOutDescription[];
extern const char kSiteIsolationOptOutChoiceDefault[];
extern const char kSiteIsolationOptOutChoiceOptOut[];

extern const char kSkiaGraphiteName[];
extern const char kSkiaGraphiteDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSplitCacheByNetworkIsolationKeyName[];
extern const char kSplitCacheByNetworkIsolationKeyDescription[];

extern const char kStoragePressureEventName[];
extern const char kStoragePressureEventDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kStylusBatteryStatusName[];
extern const char kStylusBatteryStatusDescription[];

extern const char kSupportTool[];
extern const char kSupportToolDescription[];

extern const char kSupportToolCopyTokenButton[];
extern const char kSupportToolCopyTokenButtonDescription[];

extern const char kSupportToolScreenshot[];
extern const char kSupportToolScreenshotDescription[];

extern const char kSuppressToolbarCapturesName[];
extern const char kSuppressToolbarCapturesDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSystemSoundsName[];
extern const char kSystemSoundsDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncAutofillWalletUsageDataName[];
extern const char kSyncAutofillWalletUsageDataDescription[];

extern const char kSyncEnableHistoryDataTypeName[];
extern const char kSyncEnableHistoryDataTypeDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncTrustedVaultPassphrasePromoName[];
extern const char kSyncTrustedVaultPassphrasePromoDescription[];

extern const char kSystemProxyForSystemServicesName[];
extern const char kSystemProxyForSystemServicesDescription[];

extern const char kTabDragDropName[];
extern const char kTabDragDropDescription[];

extern const char kTabEngagementReportingName[];
extern const char kTabEngagementReportingDescription[];

extern const char kTabGridLayoutAndroidName[];
extern const char kTabGridLayoutAndroidDescription[];

extern const char kCommerceDeveloperName[];
extern const char kCommerceDeveloperDescription[];

extern const char kCommerceMerchantViewerAndroidName[];
extern const char kCommerceMerchantViewerAndroidDescription[];

extern const char kTabGroupsContinuationAndroidName[];
extern const char kTabGroupsContinuationAndroidDescription[];

extern const char kTabGroupsSaveName[];
extern const char kTabGroupsSaveDescription[];

extern const char kTabHoverCardImageSettingsName[];
extern const char kTabHoverCardImageSettingsDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabSearchFuzzySearchName[];
extern const char kTabSearchFuzzySearchDescription[];

extern const char kDiscoverFeedMultiColumnAndroidName[];
extern const char kDiscoverFeedMultiColumnAndroidDescription[];

extern const char kTabStripRedesignAndroidName[];
extern const char kTabStripRedesignAndroidDescription[];

extern const char kTabletToolbarReorderingAndroidName[];
extern const char kTabletToolbarReorderingAndroidDescription[];

extern const char kEmptyStatesAndroidName[];
extern const char kEmptyStatesAndroidDescription[];

extern const char kFoldableJankFixAndroidName[];
extern const char kFoldableJankFixAndroidDescription[];

extern const char kTabStripStartupRefactoringName[];
extern const char kTabStripStartupRefactoringDescription[];

extern const char kBaselineGM3SurfaceColorsName[];
extern const char kBaselineGM3SurfaceColorsDescription[];

extern const char kDelayTempStripRemovalName[];
extern const char kDelayTempStripRemovalDescription[];

extern const char kTextBasedAudioDescriptionName[];
extern const char kTextBasedAudioDescriptionDescription[];

extern const char kTextInShelfName[];
extern const char kTextInShelfDescription[];

extern const char kTintCompositedContentName[];
extern const char kTintCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesName[];
extern const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesDescription[];

extern const char kThumbnailCacheRefactorName[];
extern const char kThumbnailCacheRefactorDescription[];

extern const char kNewBaseUrlInheritanceBehaviorName[];
extern const char kNewBaseUrlInheritanceBehaviorDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kTranslateAssistContentName[];
extern const char kTranslateAssistContentDescription[];

extern const char kTranslateForceTriggerOnEnglishName[];
extern const char kTranslateForceTriggerOnEnglishDescription[];

extern const char kTranslateIntentName[];
extern const char kTranslateIntentDescription[];

extern const char kTreatInsecureOriginAsSecureName[];
extern const char kTreatInsecureOriginAsSecureDescription[];

extern const char kPrivateStateTokensName[];
extern const char kPrivateStateTokensDescription[];

extern const char kUnifiedPasswordManagerAndroidName[];
extern const char kUnifiedPasswordManagerAndroidDescription[];

extern const char kUnifiedPasswordManagerAndroidReenrollmentName[];
extern const char kUnifiedPasswordManagerAndroidReenrollmentDescription[];

extern const char kUnsafeWebGPUName[];
extern const char kUnsafeWebGPUDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUIEnableSharedImageCacheForGpuName[];
extern const char kUIEnableSharedImageCacheForGpuDescription[];

extern const char kUseNAT64ForIPv4LiteralName[];
extern const char kUseNAT64ForIPv4LiteralDescription[];

extern const char kUserBypassUIName[];
extern const char kUserBypassUIDescription[];

extern const char kUserNotesSidePanelName[];
extern const char kUserNotesSidePanelDescription[];

extern const char kUseSearchClickForRightClickName[];
extern const char kUseSearchClickForRightClickDescription[];

extern const char kVideoConferenceName[];
extern const char kVideoConferenceDescription[];

extern const char kVcBackgroundReplaceName[];
extern const char kVcBackgroundReplaceDescription[];

extern const char kVcSegmentationModelName[];
extern const char kVcSegmentationModelDescription[];

extern const char kVcLightIntensityName[];
extern const char kVcLightIntensityDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kGlobalVaapiLockName[];
extern const char kGlobalVaapiLockDescription[];

extern const char kVmPerBootShaderCacheDescription[];
extern const char kVmPerBootShaderCacheName[];

extern const char kVp9kSVCHWDecodingName[];
extern const char kVp9kSVCHWDecodingDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kTaskManagerEndProcessDisabledForExtensionName[];
extern const char kTaskManagerEndProcessDisabledForExtensionDescription[];

extern const char kWallpaperFastRefreshName[];
extern const char kWallpaperFastRefreshDescription[];

extern const char kWallpaperGooglePhotosSharedAlbumsName[];
extern const char kWallpaperGooglePhotosSharedAlbumsDescription[];

extern const char kWallpaperPerDeskName[];
extern const char kWallpaperPerDeskDescription[];

extern const char kWebBluetoothName[];
extern const char kWebBluetoothDescription[];

extern const char kWebBluetoothNewPermissionsBackendName[];
extern const char kWebBluetoothNewPermissionsBackendDescription[];

extern const char kWebContentsCaptureHiDpiName[];
extern const char kWebContentsCaptureHiDpiDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kWebOtpBackendName[];
extern const char kWebOtpBackendDescription[];
extern const char kWebOtpBackendSmsVerification[];
extern const char kWebOtpBackendUserConsent[];
extern const char kWebOtpBackendAuto[];

extern const char kWebglDeveloperExtensionsName[];
extern const char kWebglDeveloperExtensionsDescription[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebGpuDeveloperFeaturesName[];
extern const char kWebGpuDeveloperFeaturesDescription[];

extern const char kWebPaymentsExperimentalFeaturesName[];
extern const char kWebPaymentsExperimentalFeaturesDescription[];

extern const char kIgnoreCSPInWebPaymentAPIName[];
extern const char kIgnoreCSPInWebPaymentAPIDescription[];

extern const char kAddIdentityInCanMakePaymentEventName[];
extern const char kAddIdentityInCanMakePaymentEventDescription[];

extern const char kAppStoreBillingDebugName[];
extern const char kAppStoreBillingDebugDescription[];

extern const char kWebrtcHideLocalIpsWithMdnsName[];
extern const char kWebrtcHideLocalIpsWithMdnsDecription[];

extern const char kWebRtcAllowInputVolumeAdjustmentName[];
extern const char kWebRtcAllowInputVolumeAdjustmentDescription[];

extern const char kWebRtcApmDownmixCaptureAudioMethodName[];
extern const char kWebRtcApmDownmixCaptureAudioMethodDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcUseMinMaxVEADimensionsName[];
extern const char kWebrtcUseMinMaxVEADimensionsDescription[];

extern const char kWebUsbDeviceDetectionName[];
extern const char kWebUsbDeviceDetectionDescription[];

extern const char kWebXrForceRuntimeName[];
extern const char kWebXrForceRuntimeDescription[];

extern const char kWebXrRuntimeChoiceNone[];
extern const char kWebXrRuntimeChoiceCardboard[];
extern const char kWebXrRuntimeChoiceGVR[];
extern const char kWebXrRuntimeChoiceOpenXR[];

extern const char kWebXrIncubationsName[];
extern const char kWebXrIncubationsDescription[];

extern const char kWindowLayoutMenu[];
extern const char kWindowLayoutMenuDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

extern const char kSharedHighlightingAmpName[];
extern const char kSharedHighlightingAmpDescription[];

extern const char kSharedHighlightingManagerName[];
extern const char kSharedHighlightingManagerDescription[];

extern const char kDraw1PredictedPoint12Ms[];
extern const char kDraw2PredictedPoints6Ms[];
extern const char kDraw1PredictedPoint6Ms[];
extern const char kDraw2PredictedPoints3Ms[];
extern const char kDrawPredictedPointsDescription[];
extern const char kDrawPredictedPointsName[];

extern const char kSanitizerApiName[];
extern const char kSanitizerApiDescription[];

extern const char kUsePassthroughCommandDecoderName[];
extern const char kUsePassthroughCommandDecoderDescription[];

extern const char kPassthroughYuvRgbConversionName[];
extern const char kPassthroughYuvRgbConversionDescription[];

extern const char kUseMultiPlaneFormatForHardwareVideoName[];
extern const char kUseMultiPlaneFormatForHardwareVideoDescription[];

extern const char kUseMultiPlaneFormatForSoftwareVideoName[];
extern const char kUseMultiPlaneFormatForSoftwareVideoDescription[];

extern const char kReduceAcceptLanguageName[];
extern const char kReduceAcceptLanguageDescription[];

extern const char kSkipServiceWorkerFetchHandlerName[];
extern const char kSkipServiceWorkerFetchHandlerDescription[];

extern const char kWebSQLAccessName[];
extern const char kWebSQLAccessDescription[];

extern const char kUseGpuSchedulerDfsName[];
extern const char kUseGpuSchedulerDfsDescription[];

extern const char kUseIDNA2008NonTransitionalName[];
extern const char kUseIDNA2008NonTransitionalDescription[];

extern const char kPolicyMergeMultiSourceName[];
extern const char kPolicyMergeMultiSourceDescription[];

extern const char kEnableVariationsGoogleGroupFilteringName[];
extern const char kEnableVariationsGoogleGroupFilteringDescription[];

// Android --------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

extern const char kAccessibilityPageZoomName[];
extern const char kAccessibilityPageZoomDescription[];
extern const char kAccessibilityPerformanceFilteringName[];
extern const char kAccessibilityPerformanceFilteringDescription[];

extern const char kAdaptiveButtonInTopToolbarName[];
extern const char kAdaptiveButtonInTopToolbarDescription[];
extern const char kAdaptiveButtonInTopToolbarTranslateName[];
extern const char kAdaptiveButtonInTopToolbarTranslateDescription[];
extern const char kAdaptiveButtonInTopToolbarAddToBookmarksName[];
extern const char kAdaptiveButtonInTopToolbarAddToBookmarksDescription[];
extern const char kAdaptiveButtonInTopToolbarCustomizationName[];
extern const char kAdaptiveButtonInTopToolbarCustomizationDescription[];

extern const char kAddToHomescreenIPHName[];
extern const char kAddToHomescreenIPHDescription[];

extern const char kAImageReaderName[];
extern const char kAImageReaderDescription[];

extern const char kAndroidForceAppLanguagePromptName[];
extern const char kAndroidForceAppLanguagePromptDescription[];

extern const char kAndroidSurfaceControlName[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAnimatedImageDragShadowName[];
extern const char kAnimatedImageDragShadowDescription[];
extern const char kAndroidImprovedBookmarksName[];
extern const char kAndroidImprovedBookmarksDescription[];

extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

extern const char kAppMenuMobileSiteOptionName[];
extern const char kAppMenuMobileSiteOptionDescription[];

extern const char kBackGestureActivityTabProviderName[];
extern const char kBackGestureActivityTabProviderDescription[];

extern const char kBackGestureRefactorActivityAndroidName[];
extern const char kBackGestureRefactorActivityAndroidDescription[];

extern const char kBackGestureRefactorAndroidName[];
extern const char kBackGestureRefactorAndroidDescription[];

extern const char kCCTBrandTransparencyName[];
extern const char kCCTBrandTransparencyDescription[];

extern const char kCCTIncognitoName[];
extern const char kCCTIncognitoDescription[];

extern const char kCCTIncognitoAvailableToThirdPartyName[];
extern const char kCCTIncognitoAvailableToThirdPartyDescription[];

extern const char kCCTPageInsightsHubName[];
extern const char kCCTPageInsightsHubDescription[];

extern const char kCCTResizable90MaximumHeightName[];
extern const char kCCTResizable90MaximumHeightDescription[];
extern const char kCCTResizableForThirdPartiesName[];
extern const char kCCTResizableForThirdPartiesDescription[];
extern const char kCCTResizableSideSheetName[];
extern const char kCCTResizableSideSheetDescription[];
extern const char kCCTResizableSideSheetDiscoverFeedSettingsName[];
extern const char kCCTResizableSideSheetDiscoverFeedSettingsDescription[];
extern const char kCCTResizableSideSheetForThirdPartiesName[];
extern const char kCCTResizableSideSheetForThirdPartiesDescription[];

extern const char kCCTRetainingStateInMemoryName[];
extern const char kCCTRetainingStateInMemoryDescription[];

extern const char kCCTRealTimeEngagementSignalsName[];
extern const char kCCTRealTimeEngagementSignalsDescription[];
extern const char kCCTRealTimeEngagementSignalsAlternativeImplName[];
extern const char kCCTRealTimeEngagementSignalsAlternativeImplDescription[];

extern const char kCCTTextFragmentLookupApiEnabledName[];
extern const char kCCTTextFragmentLookupApiEnabledDescription[];

extern const char kChimeAlwaysShowNotificationDescription[];
extern const char kChimeAlwaysShowNotificationName[];

extern const char kChimeAndroidSdkDescription[];
extern const char kChimeAndroidSdkName[];

extern const char kCloseTabSuggestionsName[];
extern const char kCloseTabSuggestionsDescription[];

extern const char kCriticalPersistedTabDataName[];
extern const char kCriticalPersistedTabDataDescription[];

extern const char kContextMenuPopupForAllScreenSizesName[];
extern const char kContextMenuPopupForAllScreenSizesDescription[];

extern const char kContextualSearchForceCaptionName[];
extern const char kContextualSearchForceCaptionDescription[];

extern const char kContextualSearchSuppressShortViewName[];
extern const char kContextualSearchSuppressShortViewDescription[];

extern const char kConvertTrackpadEventsToMouseName[];
extern const char kConvertTrackpadEventsToMouseDescription[];

extern const char kCormorantName[];
extern const char kCormorantDescription[];

extern const char kDefaultViewportIsDeviceWidthName[];
extern const char kDefaultViewportIsDeviceWidthDescription[];

extern const char kDeprecatedExternalPickerFunctionName[];
extern const char kDeprecatedExternalPickerFunctionDescription[];

extern const char kDrawEdgeToEdgeName[];
extern const char kDrawEdgeToEdgeDescription[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAndroidGamepadVibrationName[];
extern const char kEnableAndroidGamepadVibrationDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnablePasswordsAccountStorageName[];
extern const char kEnablePasswordsAccountStorageDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kExternalNavigationDebugLogsName[];
extern const char kExternalNavigationDebugLogsDescription[];

extern const char kFeatureNotificationGuideName[];
extern const char kFeatureNotificationGuideDescription[];

extern const char kFeatureNotificationGuideSkipCheckForLowEngagedUsersName[];
extern const char
    kFeatureNotificationGuideSkipCheckForLowEngagedUsersDescription[];

extern const char kFeedBackToTopName[];
extern const char kFeedBackToTopDescription[];

extern const char kFeedBottomSyncBannerName[];
extern const char kFeedBottomSyncBannerDescription[];

extern const char kFeedBoCSigninInterstitialName[];
extern const char kFeedBoCSigninInterstitialDescription[];

extern const char kFeedHeaderStickToTopName[];
extern const char kFeedHeaderStickToTopDescription[];

extern const char kFeedLoadingPlaceholderName[];
extern const char kFeedLoadingPlaceholderDescription[];

extern const char kFeedStampName[];
extern const char kFeedStampDescription[];

extern const char kFeedCloseRefreshName[];
extern const char kFeedCloseRefreshDescription[];

extern const char kFeedDynamicColorsName[];
extern const char kFeedDynamicColorsDescription[];

extern const char kFeedDiscoFeedEndpointName[];
extern const char kFeedDiscoFeedEndpointDescription[];

extern const char kForceOffTextAutosizingName[];
extern const char kForceOffTextAutosizingDescription[];

extern const char kInfoCardAcknowledgementTrackingName[];
extern const char kInfoCardAcknowledgementTrackingDescription[];

extern const char kInstanceSwitcherName[];
extern const char kInstanceSwitcherDescription[];

extern const char kInstantStartName[];
extern const char kInstantStartDescription[];

extern const char kInterestFeedV2Name[];
extern const char kInterestFeedV2Description[];

extern const char kInterestFeedV2HeartsName[];
extern const char kInterestFeedV2HeartsDescription[];

extern const char kInterestFeedV2AutoplayName[];
extern const char kInterestFeedV2AutoplayDescription[];

extern const char kMediaPickerAdoptionStudyName[];
extern const char kMediaPickerAdoptionStudyDescription[];

extern const char kMessagesForAndroidAdsBlockedName[];
extern const char kMessagesForAndroidAdsBlockedDescription[];

extern const char kMessagesForAndroidInfrastructureName[];
extern const char kMessagesForAndroidInfrastructureDescription[];

extern const char kMessagesForAndroidOfferNotificationName[];
extern const char kMessagesForAndroidOfferNotificationDescription[];

extern const char kMessagesForAndroidPermissionUpdateName[];
extern const char kMessagesForAndroidPermissionUpdateDescription[];

extern const char kMessagesForAndroidPopupBlockedName[];
extern const char kMessagesForAndroidPopupBlockedDescription[];

extern const char kMessagesForAndroidPWAInstallName[];
extern const char kMessagesForAndroidPWAInstallDescription[];

extern const char kMessagesForAndroidSaveCardName[];
extern const char kMessagesForAndroidSaveCardDescription[];

extern const char kMessagesForAndroidStackingAnimationName[];
extern const char kMessagesForAndroidStackingAnimationDescription[];

extern const char kNetworkServiceInProcessName[];
extern const char kNetworkServiceInProcessDescription[];

extern const char kNotificationPermissionRationaleName[];
extern const char kNotificationPermissionRationaleDescription[];

extern const char kNotificationPermissionRationaleBottomSheetName[];
extern const char kNotificationPermissionRationaleBottomSheetDescription[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kOmahaMinSdkVersionAndroidName[];
extern const char kOmahaMinSdkVersionAndroidDescription[];
extern const char kOmahaMinSdkVersionAndroidMinSdk1Description[];
extern const char kOmahaMinSdkVersionAndroidMinSdk1000Description[];

extern const char kPageInfoHistoryName[];
extern const char kPageInfoHistoryDescription[];

extern const char kPageInfoAboutThisSiteImprovedBottomSheetName[];
extern const char kPageInfoAboutThisSiteImprovedBottomSheetDescription[];

extern const char kPasswordEditDialogWithDetailsName[];
extern const char kPasswordEditDialogWithDetailsDescription[];

extern const char kPasswordGenerationBottomSheetName[];
extern const char kPasswordGenerationBottomSheetDescription[];

extern const char kPasswordsInCredManName[];
extern const char kPasswordsInCredManDescription[];

extern const char kPolicyLogsPageAndroidName[];
extern const char kPolicyLogsPageAndroidDescription[];

extern const char kQueryTilesName[];
extern const char kQueryTilesDescription[];
extern const char kQueryTilesNTPName[];
extern const char kQueryTilesNTPDescription[];
extern const char kQueryTilesOnStartName[];
extern const char kQueryTilesOnStartDescription[];
extern const char kQueryTilesSingleTierName[];
extern const char kQueryTilesSingleTierDescription[];
extern const char kQueryTilesEnableQueryEditingName[];
extern const char kQueryTilesEnableQueryEditingDescription[];
extern const char kQueryTilesEnableTrendingName[];
extern const char kQueryTilesEnableTrendingDescription[];
extern const char kQueryTilesCountryCode[];
extern const char kQueryTilesCountryCodeDescription[];
extern const char kQueryTilesCountryCodeUS[];
extern const char kQueryTilesCountryCodeIndia[];
extern const char kQueryTilesCountryCodeBrazil[];
extern const char kQueryTilesCountryCodeNigeria[];
extern const char kQueryTilesCountryCodeIndonesia[];
extern const char kQueryTilesInstantFetchName[];
extern const char kQueryTilesInstantFetchDescription[];
extern const char kQueryTilesRankTilesName[];
extern const char kQueryTilesRankTilesDescription[];
extern const char kQueryTilesSegmentationName[];
extern const char kQueryTilesSegmentationDescription[];
extern const char kQueryTilesSwapTrendingName[];
extern const char kQueryTilesSwapTrendingDescription[];

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAllArticles[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

extern const char kReaderModeInCCTName[];
extern const char kReaderModeInCCTDescription[];

extern const char kRecoverFromNeverSaveAndroidName[];
extern const char kRecoverFromNeverSaveAndroidDescription[];

extern const char kReduceUserAgentAndroidVersionDeviceModelName[];
extern const char kReduceUserAgentAndroidVersionDeviceModelDescription[];

extern const char kReengagementNotificationName[];
extern const char kReengagementNotificationDescription[];

extern const char kRelatedSearchesName[];
extern const char kRelatedSearchesDescription[];

extern const char kRequestDesktopSiteAdditionsName[];
extern const char kRequestDesktopSiteAdditionsDescription[];

extern const char kRequestDesktopSiteDefaultsName[];
extern const char kRequestDesktopSiteDefaultsDescription[];

extern const char kRequestDesktopSiteDefaultsDowngradeName[];
extern const char kRequestDesktopSiteDefaultsDowngradeDescription[];

extern const char kRequestDesktopSiteDefaultsLoggingName[];
extern const char kRequestDesktopSiteDefaultsLoggingDescription[];

extern const char kRequestDesktopSiteExceptionsName[];
extern const char kRequestDesktopSiteExceptionsDescription[];

extern const char kRequestDesktopSiteExceptionsDowngradeName[];
extern const char kRequestDesktopSiteExceptionsDowngradeDescription[];

extern const char kRequestDesktopSitePerSiteIphName[];
extern const char kRequestDesktopSitePerSiteIphDescription[];

extern const char kRequestDesktopSiteZoomName[];
extern const char kRequestDesktopSiteZoomDescription[];

extern const char kRevokeNotificationsPermissionIfDisabledOnAppLevelName[];
extern const char
    kRevokeNotificationsPermissionIfDisabledOnAppLevelDescription[];

extern const char kSafeModeForCachedFlagsName[];
extern const char kSafeModeForCachedFlagsDescription[];

extern const char kSafeSitesFilterBehaviorPolicyAndroidName[];
extern const char kSafeSitesFilterBehaviorPolicyAndroidDescription[];

extern const char kScreenshotsForAndroidV2Name[];
extern const char kScreenshotsForAndroidV2Description[];

extern const char kSecurePaymentConfirmationAndroidName[];
extern const char kSecurePaymentConfirmationAndroidDescription[];

extern const char kShowScrollableMVTOnNTPAndroidName[];
extern const char kShowScrollableMVTOnNTPAndroidDescription[];

extern const char kSendTabToSelfV2Name[];
extern const char kSendTabToSelfV2Description[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kShareSheetCustomActionsPolishName[];
extern const char kShareSheetCustomActionsPolishDescription[];

extern const char kShareSheetMigrationAndroidName[];
extern const char kShareSheetMigrationAndroidDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kSlimCompositorName[];
extern const char kSlimCompositorDescription[];

extern const char kSmartSuggestionForLargeDownloadsName[];
extern const char kSmartSuggestionForLargeDownloadsDescription[];

extern const char kStartSurfaceAndroidName[];
extern const char kStartSurfaceAndroidDescription[];

extern const char kFeedPositionAndroidName[];
extern const char kFeedPositionAndroidDescription[];

extern const char kSearchResumptionModuleAndroidName[];
extern const char kSearchResumptionModuleAndroidDescription[];

extern const char kSiteDataImprovementsName[];
extern const char kSiteDataImprovementsDescription[];

extern const char kStartSurfaceDisabledFeedImprovementName[];
extern const char kStartSurfaceDisabledFeedImprovementDescription[];

extern const char kStartSurfaceOnTabletName[];
extern const char kStartSurfaceOnTabletDescription[];

extern const char kStartSurfaceRefactorName[];
extern const char kStartSurfaceRefactorDescription[];

extern const char kStartSurfaceSpareTabName[];
extern const char kStartSurfaceSpareTabDescription[];

extern const char kStartSurfaceWithAccessibilityName[];
extern const char kStartSurfaceWithAccessibilityDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kStylusRichGesturesName[];
extern const char kStylusRichGesturesDescription[];

extern const char kSurfaceControlMagnifierName[];
extern const char kSurfaceControlMagnifierDescription[];

extern const char kSurfacePolishName[];
extern const char kSurfacePolishDescription[];

extern const char kTabGroupsForTabletsName[];
extern const char kTabGroupsForTabletsDescription[];

extern const char kThumbnailPlaceholderName[];
extern const char kThumbnailPlaceholderDescription[];

extern const char kTouchDragAndContextMenuName[];
extern const char kTouchDragAndContextMenuDescription[];

extern const char kTranslateMessageUIName[];
extern const char kTranslateMessageUIDescription[];

extern const char kTwaPostMessageName[];
extern const char kTwaPostMessageDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuTypeName[];
extern const char kUpdateMenuTypeDescription[];
extern const char kUpdateMenuTypeNone[];
extern const char kUpdateMenuTypeUpdateAvailable[];
extern const char kUpdateMenuTypeUnsupportedOSVersion[];

extern const char kVideoTutorialsName[];
extern const char kVideoTutorialsDescription[];

extern const char kWebAuthnAndroidCredManName[];
extern const char kWebAuthnAndroidCredManDescription[];

extern const char kWebApkInstallFailureNotificationName[];
extern const char kWebApkInstallFailureNotificationDescription[];

extern const char kWebApkInstallFailureRetryName[];
extern const char kWebApkInstallFailureRetryDescription[];

extern const char kWebFeedName[];
extern const char kWebFeedDescription[];

extern const char kWebFeedAwarenessName[];
extern const char kWebFeedAwarenessDescription[];

extern const char kWebFeedOnboardingName[];
extern const char kWebFeedOnboardingDescription[];

extern const char kWebFeedSortName[];
extern const char kWebFeedSortDescription[];

extern const char kWebXrSharedBuffersName[];
extern const char kWebXrSharedBuffersDescription[];

extern const char kXsurfaceMetricsReportingName[];
extern const char kXsurfaceMetricsReportingDescription[];

#if BUILDFLAG(ENABLE_VR) && BUILDFLAG(ENABLE_OPENXR)
extern const char kOpenXRName[];
extern const char kOpenXRDescription[];
#endif

// Non-Android ----------------------------------------------------------------

#else  // !BUILDFLAG(IS_ANDROID)

extern const char kAccessCodeCastFreezeUiName[];
extern const char kAccessCodeCastFreezeUiDescription[];

extern const char kAppManagementAppDetailsName[];
extern const char kAppManagementAppDetailsDescription[];

extern const char kAllowAllSitesToInitiateMirroringName[];
extern const char kAllowAllSitesToInitiateMirroringDescription[];

extern const char kDialMediaRouteProviderName[];
extern const char kDialMediaRouteProviderDescription[];

extern const char kMediaRouterOtrInstanceName[];
extern const char kMediaRouterOtrInstanceDescription[];

extern const char kCastMirroringTargetPlayoutDelayName[];
extern const char kCastMirroringTargetPlayoutDelayDescription[];
extern const char kCastMirroringTargetPlayoutDelayDefault[];
extern const char kCastMirroringTargetPlayoutDelay100ms[];
extern const char kCastMirroringTargetPlayoutDelay150ms[];
extern const char kCastMirroringTargetPlayoutDelay200ms[];
extern const char kCastMirroringTargetPlayoutDelay250ms[];
extern const char kCastMirroringTargetPlayoutDelay300ms[];
extern const char kCastMirroringTargetPlayoutDelay350ms[];

extern const char kEnablePolicyTestPageName[];
extern const char kEnablePolicyTestPageDescription[];

extern const char kCopyLinkToTextName[];
extern const char kCopyLinkToTextDescription[];

extern const char kDesktopPartialTranslateName[];
extern const char kDesktopPartialTranslateDescription[];

extern const char kEnableAccessibilityLiveCaptionName[];
extern const char kEnableAccessibilityLiveCaptionDescription[];

extern const char kReadAnythingName[];
extern const char kReadAnythingDescription[];

extern const char kReadAnythingWithScreen2xName[];
extern const char kReadAnythingWithScreen2xDescription[];

extern const char kEnableWebHidOnExtensionServiceWorkerName[];
extern const char kEnableWebHidOnExtensionServiceWorkerDescription[];

extern const char kGlobalMediaControlsCastStartStopName[];
extern const char kGlobalMediaControlsCastStartStopDescription[];

extern const char kHeuristicMemorySaverName[];
extern const char kHeuristicMemorySaverDescription[];

extern const char kHideIncognitoMediaMetadataName[];
extern const char kHideIncognitoMediaMetadataDescription[];

extern const char kHighEfficiencyMultistateModeAvailableName[];
extern const char kHighEfficiencyMultistateModeAvailableDescription[];

extern const char kHighEfficiencyDiscardedTabTreatmentName[];
extern const char kHighEfficiencyDiscardedTabTreatmentDescription[];

extern const char kHighEfficiencyMemoryUsageInHovercardsName[];
extern const char kHighEfficiencyMemoryUsageInHovercardsDescription[];

extern const char kHighEfficiencyDiscardExceptionsImprovementsName[];
extern const char kHighEfficiencyDiscardExceptionsImprovementsDescription[];

extern const char kHighEfficiencySavingsReportingImprovementsName[];
extern const char kHighEfficiencySavingsReportingImprovementsDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kIOSPromoPasswordBubbleName[];
extern const char kIOSPromoPasswordBubbleDecription[];
#endif

extern const char kMuteNotificationSnoozeActionName[];
extern const char kMuteNotificationSnoozeActionDescription[];

extern const char kNtpAlphaBackgroundCollectionsName[];
extern const char kNtpAlphaBackgroundCollectionsDescription[];

extern const char kNtpBackgroundImageErrorDetectionName[];
extern const char kNtpBackgroundImageErrorDetectionDescription[];

extern const char kNtpCacheOneGoogleBarName[];
extern const char kNtpCacheOneGoogleBarDescription[];

extern const char kNtpChromeCartModuleName[];
extern const char kNtpChromeCartModuleDescription[];

extern const char kNtpComprehensiveThemeRealboxName[];
extern const char kNtpComprehensiveThemeRealboxDescription[];

extern const char kNtpDesktopLensName[];
extern const char kNtpDesktopLensDescription[];

extern const char kNtpDriveModuleName[];
extern const char kNtpDriveModuleDescription[];

#if !defined(OFFICIAL_BUILD)
extern const char kNtpDummyModulesName[];
extern const char kNtpDummyModulesDescription[];
#endif

extern const char kNtpHistoryClustersModuleName[];
extern const char kNtpHistoryClustersModuleDescription[];

extern const char kNtpHistoryClustersModuleUseModelRankingName[];
extern const char kNtpHistoryClustersModuleUseModelRankingDescription[];

extern const char kNtpChromeCartInHistoryClustersModuleName[];
extern const char kNtpChromeCartInHistoryClustersModuleDescription[];

extern const char kNtpChromeCartHistoryClusterCoexistName[];
extern const char kNtpChromeCartHistoryClusterCoexistDescription[];

extern const char kNtpMiddleSlotPromoDismissalName[];
extern const char kNtpMiddleSlotPromoDismissalDescription[];

extern const char kNtpModulesDragAndDropName[];
extern const char kNtpModulesDragAndDropDescription[];

extern const char kNtpModulesFirstRunExperienceName[];
extern const char kNtpModulesFirstRunExperienceDescription[];

extern const char kNtpModulesHeaderIconName[];
extern const char kNtpModulesHeaderIconDescription[];

extern const char kNtpModulesRedesignedName[];
extern const char kNtpModulesRedesignedDescription[];

extern const char kNtpPhotosModuleOptInTitleName[];
extern const char kNtpPhotosModuleOptInTitleDescription[];

extern const char kNtpPhotosModuleOptInArtWorkName[];
extern const char kNtpPhotosModuleOptInArtWorkDescription[];

extern const char kNtpPhotosModuleName[];
extern const char kNtpPhotosModuleDescription[];

extern const char kNtpPhotosModuleSoftOptOutName[];
extern const char kNtpPhotosModuleSoftOptOutDescription[];

extern const char kNtpRealboxIsTallName[];
extern const char kNtpRealboxIsTallDescription[];

extern const char kNtpRealboxMatchSearchboxThemeName[];
extern const char kNtpRealboxMatchSearchboxThemeDescription[];

extern const char kNtpRealboxPedalsName[];
extern const char kNtpRealboxPedalsDescription[];

extern const char kNtpRealboxWidthBehaviorName[];
extern const char kNtpRealboxWidthBehaviorDescription[];

extern const char kNtpRealboxRoundedCornersName[];
extern const char kNtpRealboxRoundedCornersDescription[];

extern const char kNtpRealboxUseGoogleGIconName[];
extern const char kNtpRealboxUseGoogleGIconDescription[];

extern const char kNtpRecipeTasksModuleName[];
extern const char kNtpRecipeTasksModuleDescription[];

extern const char kNtpReducedLogoSpaceName[];
extern const char kNtpReducedLogoSpaceDescription[];

extern const char kNtpSafeBrowsingModuleName[];
extern const char kNtpSafeBrowsingModuleDescription[];

extern const char kNtpSingleRowShortcutsName[];
extern const char kNtpSingleRowShortcutsDescription[];

extern const char kNtpWideModulesName[];
extern const char kNtpWideModulesDescription[];

extern const char kEnableReaderModeName[];
extern const char kEnableReaderModeDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kLayoutExtractionName[];
extern const char kLayoutExtractionDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescription[];

extern const char kPasswordsGroupingName[];
extern const char kPasswordsGroupingDescription[];

extern const char kPasswordManagerRedesignName[];
extern const char kPasswordManagerRedesignDescription[];

extern const char kPermissionStorageAccessAPIName[];
extern const char kPermissionStorageAccessAPIDescription[];

extern const char kRealboxSecondaryZeroSuggestName[];
extern const char kRealboxSecondaryZeroSuggestDescription[];

extern const char kSCTAuditingName[];
extern const char kSCTAuditingDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kSettingsEnableGetTheMostOutOfChromeName[];
extern const char kSettingsEnableGetTheMostOutOfChromeDescription[];
#endif

extern const char kWebAppDedupeInstallUrlsName[];
extern const char kWebAppDedupeInstallUrlsDescription[];

extern const char kWebAppManifestImmediateUpdatingName[];
extern const char kWebAppManifestImmediateUpdatingDescription[];

extern const char kWebAppSyncGeneratedIconBackgroundFixName[];
extern const char kWebAppSyncGeneratedIconBackgroundFixDescription[];

extern const char kWebAppSyncGeneratedIconRetroactiveFixName[];
extern const char kWebAppSyncGeneratedIconRetroactiveFixDescription[];

extern const char kWebAppSyncGeneratedIconUpdateFixName[];
extern const char kWebAppSyncGeneratedIconUpdateFixDescription[];

extern const char kWebAuthenticationPermitEnterpriseAttestationName[];
extern const char kWebAuthenticationPermitEnterpriseAttestationDescription[];

extern const char kDevToolsTabTargetLiteralName[];
extern const char kDevToolsTabTargetLiteralDescription[];

#endif  // BUILDFLAG(IS_ANDROID)

// Windows --------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kCloudApAuthAttachAsHeaderName[];
extern const char kCloudApAuthAttachAsHeaderDescription[];

extern const char kEnableMediaFoundationVideoCaptureName[];
extern const char kEnableMediaFoundationVideoCaptureDescription[];

extern const char kHardwareSecureDecryptionName[];
extern const char kHardwareSecureDecryptionDescription[];

extern const char kHardwareSecureDecryptionExperimentName[];
extern const char kHardwareSecureDecryptionExperimentDescription[];

extern const char kHardwareSecureDecryptionFallbackName[];
extern const char kHardwareSecureDecryptionFallbackDescription[];

extern const char kMediaFoundationClearName[];
extern const char kMediaFoundationClearDescription[];

extern const char kRawAudioCaptureName[];
extern const char kRawAudioCaptureDescription[];

extern const char kRunVideoCaptureServiceInBrowserProcessName[];
extern const char kRunVideoCaptureServiceInBrowserProcessDescription[];

extern const char kUseAngleDescriptionWindows[];

extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];
extern const char kUseAngleD3D11on12[];

extern const char kUseWaitableSwapChainName[];
extern const char kUseWaitableSwapChainDescription[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

extern const char kWindows11MicaTitlebarName[];
extern const char kWindows11MicaTitlebarDescription[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kLaunchWindowsNativeHostsDirectlyName[];
extern const char kLaunchWindowsNativeHostsDirectlyDescription[];
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kPrintWithPostScriptType42FontsName[];
extern const char kPrintWithPostScriptType42FontsDescription[];

extern const char kPrintWithReducedRasterizationName[];
extern const char kPrintWithReducedRasterizationDescription[];

extern const char kReadPrinterCapabilitiesWithXpsName[];
extern const char kReadPrinterCapabilitiesWithXpsDescription[];

extern const char kUseXpsForPrintingName[];
extern const char kUseXpsForPrintingDescription[];

extern const char kUseXpsForPrintingFromPdfName[];
extern const char kUseXpsForPrintingFromPdfDescription[];
#endif  // BUILDFLAG(ENABLE_PRINTING)

#endif  // BUILDFLAG(IS_WIN)

// Mac ------------------------------------------------------------------------

#if BUILDFLAG(IS_MAC)

extern const char kBiometricAuthenticationInSettingsName[];
extern const char kBiometricAuthenticationInSettingsDescription[];

extern const char kCr2023MacFontSmoothingName[];
extern const char kCr2023MacFontSmoothingDescription[];

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kCupsIppPrintingBackendName[];
extern const char kCupsIppPrintingBackendDescription[];
#endif  // BUILDFLAG(ENABLE_PRINTING)

extern const char kEnableUniversalLinksName[];
extern const char kEnableUniversalLinksDescription[];

extern const char kImmersiveFullscreenName[];
extern const char kImmersiveFullscreenDescription[];

extern const char kMacSyscallSandboxName[];
extern const char kMacSyscallSandboxDescription[];

extern const char kRetryGetVideoCaptureDeviceInfosName[];
extern const char kRetryGetVideoCaptureDeviceInfosDescription[];

extern const char kScreenTimeName[];
extern const char kScreenTimeDescription[];

extern const char kUseAngleDescriptionMac[];
extern const char kUseAngleMetal[];

extern const char kSystemColorChooserName[];
extern const char kSystemColorChooserDescription[];

#endif  // BUILDFLAG(IS_MAC)

// Windows and Mac ------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

extern const char kUseAngleName[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];

extern const char kBiometricAuthenticationForFillingName[];
extern const char kBiometricAuthenticationForFillingDescription[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// ChromeOS -------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAdaptiveChargingName[];
extern const char kAdaptiveChargingDescription[];

extern const char kAdaptiveChargingForTestingName[];
extern const char kAdaptiveChargingForTestingDescription[];

extern const char kAllowDevtoolsInSystemUIName[];
extern const char kAllowDevtoolsInSystemUIDescription[];

extern const char kAllowEapDefaultCasWithoutSubjectVerificationName[];
extern const char kAllowEapDefaultCasWithoutSubjectVerificationDescription[];

extern const char kAllowRepeatedUpdatesName[];
extern const char kAllowRepeatedUpdatesDescription[];

extern const char kAllowScrollSettingsName[];
extern const char kAllowScrollSettingsDescription[];
extern const char kAltClickAndSixPackCustomizationName[];
extern const char kAltClickAndSixPackCustomizationDescription[];
extern const char kAlwaysEnableHdcpName[];
extern const char kAlwaysEnableHdcpDescription[];
extern const char kAlwaysEnableHdcpDefault[];
extern const char kAlwaysEnableHdcpType0[];
extern const char kAlwaysEnableHdcpType1[];

extern const char kAmbientModeThrottleAnimationName[];
extern const char kAmbientModeThrottleAnimationDescription[];

extern const char kApnRevampName[];
extern const char kApnRevampDescription[];

extern const char kAppLaunchAutomationName[];
extern const char kAppLaunchAutomationDescription[];

extern const char kArcArcOnDemandExperimentName[];
extern const char kArcArcOnDemandExperimentDescription[];

extern const char kArcCustomTabsExperimentName[];
extern const char kArcCustomTabsExperimentDescription[];

extern const char kArcDocumentsProviderUnknownSizeName[];
extern const char kArcDocumentsProviderUnknownSizeDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcFixupWindowName[];
extern const char kArcFixupWindowDescription[];

extern const char kArcGameModeName[];
extern const char kArcGameModeDescription[];

extern const char kArcIdleManagerName[];
extern const char kArcIdleManagerDescription[];

extern const char kArcInstantResponseWindowOpenName[];
extern const char kArcInstantResponseWindowOpenDescription[];

extern const char kArcKeyboardShortcutHelperIntegrationName[];
extern const char kArcKeyboardShortcutHelperIntegrationDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcNearbyShareFuseBoxName[];
extern const char kArcNearbyShareFuseBoxDescription[];

extern const char kArcRtVcpuDualCoreName[];
extern const char kArcRtVcpuDualCoreDesc[];

extern const char kArcRtVcpuQuadCoreName[];
extern const char kArcRtVcpuQuadCoreDesc[];

extern const char kArcSwitchToKeyMintDaemonName[];
extern const char kArcSwitchToKeyMintDaemonDesc[];

extern const char kArcSwitchToKeyMintOnTName[];
extern const char kArcSwitchToKeyMintOnTDesc[];

extern const char kArcSyncInstallPriorityName[];
extern const char kArcSyncInstallPriorityDescription[];

extern const char kArcVmmSwapKBShortcutName[];
extern const char kArcVmmSwapKBShortcutDesc[];

extern const char kArcEnableAAudioMMAPName[];
extern const char kArcEnableAAudioMMAPDescription[];

extern const char kArcAAudioMMAPLowLatencyName[];
extern const char kArcAAudioMMAPLowLatencyDescription[];

extern const char kArcEnableVirtioBlkForDataName[];
extern const char kArcEnableVirtioBlkForDataDesc[];

extern const char kArcExternalStorageAccessName[];
extern const char kArcExternalStorageAccessDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAudioAPNoiseCancellationName[];
extern const char kAudioAPNoiseCancellationDescription[];

extern const char kAudioFlexibleLoopbackName[];
extern const char kAudioFlexibleLoopbackDescription[];

extern const char kAudioHFPMicSRName[];
extern const char kAudioHFPMicSRDescription[];

extern const char kAudioHFPNbsWarningName[];
extern const char kAudioHFPNbsWarningDescription[];

extern const char kAudioHFPSwbName[];
extern const char kAudioHFPSwbDescription[];

extern const char kAudioHFPOffloadName[];
extern const char kAudioHFPOffloadDescription[];

extern const char kAutoFramingOverrideName[];
extern const char kAutoFramingOverrideDescription[];

extern const char kAutocompleteExtendedSuggestionsName[];
extern const char kAutocompleteExtendedSuggestionsDescription[];

extern const char kAutocorrectByDefaultName[];
extern const char kAutocorrectByDefaultDescription[];

extern const char kAutocorrectParamsTuningName[];
extern const char kAutocorrectParamsTuningDescription[];

extern const char kAutocorrectToggleName[];
extern const char kAutocorrectToggleDescription[];

extern const char kAutocorrectUseReplaceSurroundingTextName[];
extern const char kAutocorrectUseReplaceSurroundingTextDescription[];

extern const char kAvatarsCloudMigrationName[];
extern const char kAvatarsCloudMigrationDescription[];

extern const char kBluetoothFixA2dpPacketSizeName[];
extern const char kBluetoothFixA2dpPacketSizeDescription[];

extern const char kBluetoothQualityReportName[];
extern const char kBluetoothQualityReportDescription[];

extern const char kBluetoothWbsDogfoodName[];
extern const char kBluetoothWbsDogfoodDescription[];

extern const char kBluetoothCoredumpName[];
extern const char kBluetoothCoredumpDescription[];

extern const char kBluetoothFlossCoredumpName[];
extern const char kBluetoothFlossCoredumpDescription[];

extern const char kRobustAudioDeviceSelectLogicName[];
extern const char kRobustAudioDeviceSelectLogicDescription[];

extern const char kBluetoothUseFlossName[];
extern const char kBluetoothUseFlossDescription[];

extern const char kBluetoothUseLLPrivacyName[];
extern const char kBluetoothUseLLPrivacyDescription[];

extern const char kBluetoothLongAutosuspendName[];
extern const char kBluetoothLongAutosuspendDescription[];

extern const char kCalendarJellyName[];
extern const char kCalendarJellyDescription[];

extern const char kCaptureModeAudioMixingName[];
extern const char kCaptureModeAudioMixingDescription[];

extern const char kCaptureModeDemoToolsName[];
extern const char kCaptureModeDemoToolsDescription[];

extern const char kCaptureModeGifRecordingName[];
extern const char kCaptureModeGifRecordingDescription[];

extern const char kCrosBatterySaverAlwaysOnName[];
extern const char kCrosBatterySaverAlwaysOnDescription[];

extern const char kCrosBatterySaverName[];
extern const char kCrosBatterySaverDescription[];

extern const char kCrosWebAppShortcutUiUpdateName[];
extern const char kCrosWebAppShortcutUiUpdateDescription[];

extern const char kDeskButtonName[];
extern const char kDeskButtonDescription[];

extern const char kDesksTemplatesName[];
extern const char kDesksTemplatesDescription[];

extern const char kDnsOverHttpsWithIdentifiersReuseOldPolicyName[];
extern const char kDnsOverHttpsWithIdentifiersReuseOldPolicyDescription[];

extern const char kPreferConstantFrameRateName[];
extern const char kPreferConstantFrameRateDescription[];

extern const char kMoreVideoCaptureBuffersName[];
extern const char kMoreVideoCaptureBuffersDescription[];

extern const char kForceControlFaceAeName[];
extern const char kForceControlFaceAeDescription[];

extern const char kCellularBypassESimInstallationConnectivityCheckName[];
extern const char kCellularBypassESimInstallationConnectivityCheckDescription[];

extern const char kCellularUseSecondEuiccName[];
extern const char kCellularUseSecondEuiccDescription[];

extern const char kClipboardHistoryLongpressName[];
extern const char kClipboardHistoryLongpressDescription[];

extern const char kComponentUpdaterTestRequestName[];
extern const char kComponentUpdaterTestRequestDescription[];

extern const char kContextualNudgesName[];
extern const char kContextualNudgesDescription[];

extern const char kCroshSWAName[];
extern const char kCroshSWADescription[];

extern const char kCrosOnDeviceGrammarCheckName[];
extern const char kCrosOnDeviceGrammarCheckDescription[];

extern const char kSystemExtensionsName[];
extern const char kSystemExtensionsDescription[];

extern const char kEnableServiceWorkersForChromeUntrustedName[];
extern const char kEnableServiceWorkersForChromeUntrustedDescription[];

extern const char kEnterpriseReportingUIName[];
extern const char kEnterpriseReportingUIDescription[];

extern const char kPermissiveUsbPassthroughName[];
extern const char kPermissiveUsbPassthroughDescription[];

extern const char kCrostiniContainerInstallName[];
extern const char kCrostiniContainerInstallDescription[];

extern const char kCrostiniGpuSupportName[];
extern const char kCrostiniGpuSupportDescription[];

extern const char kCrostiniResetLxdDbName[];
extern const char kCrostiniResetLxdDbDescription[];

extern const char kCrostiniMultiContainerName[];
extern const char kCrostiniMultiContainerDescription[];

extern const char kCrostiniImeSupportName[];
extern const char kCrostiniImeSupportDescription[];

extern const char kCrostiniQtImeSupportName[];
extern const char kCrostiniQtImeSupportDescription[];

extern const char kCrostiniVirtualKeyboardSupportName[];
extern const char kCrostiniVirtualKeyboardSupportDescription[];

extern const char kCrostiniUseLxd5Name[];
extern const char kCrostiniUseLxd5Description[];

extern const char kBruschettaName[];
extern const char kBruschettaDescription[];

extern const char kBruschettaAlphaMigrateName[];
extern const char kBruschettaAlphaMigrateDescription[];

extern const char kBruschettaInstallerDownloadStrategyName[];
extern const char kBruschettaInstallerDownloadStrategyDescription[];

extern const char kCameraAppTimeLapseName[];
extern const char kCameraAppTimeLapseDescription[];

extern const char kDisableBufferBWCompressionName[];
extern const char kDisableBufferBWCompressionDescription[];

extern const char kDisableCameraFrameRotationAtSourceName[];
extern const char kDisableCameraFrameRotationAtSourceDescription[];

extern const char kForceSpectreVariant2MitigationName[];
extern const char kForceSpectreVariant2MitigationDescription[];

extern const char kDisableCancelAllTouchesName[];
extern const char kDisableCancelAllTouchesDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisplayAlignmentAssistanceName[];
extern const char kDisplayAlignmentAssistanceDescription[];

extern const char kFastPairName[];
extern const char kFastPairDescription[];

extern const char kFastPairHandshakeRefactorName[];
extern const char kFastPairHandshakeRefactorDescription[];

extern const char kFastPairHIDName[];
extern const char kFastPairHIDDescription[];

extern const char kFastPairLowPowerName[];
extern const char kFastPairLowPowerDescription[];

extern const char kFastPairSoftwareScanningName[];
extern const char kFastPairSoftwareScanningDescription[];

extern const char kFastPairSavedDevicesName[];
extern const char kFastPairSavedDevicesDescription[];

extern const char kFrameSinkDesktopCapturerInCrdName[];
extern const char kFrameSinkDesktopCapturerInCrdDescription[];

extern const char kUseHDRTransferFunctionName[];
extern const char kUseHDRTransferFunctionDescription[];
extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kQuickSettingsPWANotificationsName[];
extern const char kQuickSettingsPWANotificationsDescription[];

extern const char kDriveFsChromeNetworkingName[];
extern const char kDriveFsChromeNetworkingDescription[];

extern const char kDriveFsShowCSEFilesName[];
extern const char kDriveFsShowCSEFilesDescription[];

extern const char kEnableAssistantRoutinesName[];
extern const char kEnableAssistantRoutinesDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kDisableDnsProxyName[];
extern const char kDisableDnsProxyDescription[];

extern const char kEnableRFC8925Name[];
extern const char kEnableRFC8925Description[];

extern const char kEnableEdidBasedDisplayIdsName[];
extern const char kEnableEdidBasedDisplayIdsDescription[];

extern const char kEnableExternalKeyboardsInDiagnosticsAppName[];
extern const char kEnableExternalKeyboardsInDiagnosticsAppDescription[];

extern const char kEnableGetDebugdLogsInParallelName[];
extern const char kEnableGetDebugdLogsInParallelDescription[];

extern const char kEnableHostnameSettingName[];
extern const char kEnableHostnameSettingDescription[];

extern const char kEnableGesturePropertiesDBusServiceName[];
extern const char kEnableGesturePropertiesDBusServiceDescription[];

extern const char kEnableGoogleAssistantDspName[];
extern const char kEnableGoogleAssistantDspDescription[];

extern const char kEnableGoogleAssistantStereoInputName[];
extern const char kEnableGoogleAssistantStereoInputDescription[];

extern const char kEnableGoogleAssistantAecName[];
extern const char kEnableGoogleAssistantAecDescription[];

extern const char kEnableInputEventLoggingName[];
extern const char kEnableInputEventLoggingDescription[];

extern const char kDiagnosticsAppJellyName[];
extern const char kDiagnosticsAppJellyDescription[];

extern const char kEnableKeyboardBacklightToggleName[];
extern const char kEnableKeyboardBacklightToggleDescription[];

extern const char kEnableLibinputToHandleTouchpadName[];
extern const char kEnableLibinputToHandleTouchpadDescription[];

extern const char kEnableNeuralPalmAdaptiveHoldName[];
extern const char kEnableNeuralPalmAdaptiveHoldDescription[];

extern const char kEnableFakeKeyboardHeuristicName[];
extern const char kEnableFakeKeyboardHeuristicDescription[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableEdgeDetectionName[];
extern const char kEnableEdgeDetectionDescription[];

extern const char kEnableOsFeedbackName[];
extern const char kEnableOsFeedbackDescription[];

extern const char kEnableNewShortcutMappingName[];
extern const char kEnableNewShortcutMappingDescription[];

extern const char kEnablePalmSuppressionName[];
extern const char kEnablePalmSuppressionDescription[];

extern const char kEnablePerDeskZOrderName[];
extern const char kEnablePerDeskZOrderDescription[];

extern const char kEnableSeamlessRefreshRateSwitchingName[];
extern const char kEnableSeamlessRefreshRateSwitchingDescription[];

extern const char kEnableTouchpadsInDiagnosticsAppName[];
extern const char kEnableTouchpadsInDiagnosticsAppDescription[];

extern const char kEnableTouchscreensInDiagnosticsAppName[];
extern const char kEnableTouchscreensInDiagnosticsAppDescription[];

extern const char kEnableZramWriteback[];
extern const char kEnableZramWritebackDescription[];

extern const char kEnableSuspendToDisk[];
extern const char kEnableSuspendToDiskDescription[];

extern const char kEapGtcWifiAuthenticationName[];
extern const char kEapGtcWifiAuthenticationDescription[];

extern const char kAudioPeripheralVolumeGranularityName[];
extern const char kAudioPeripheralVolumeGranularityDescription[];

extern const char kEcheSWAName[];
extern const char kEcheSWADescription[];

extern const char kEcheLauncherName[];
extern const char kEcheLauncherDescription[];

extern const char kEcheLauncherListViewName[];
extern const char kEcheLauncherListViewDescription[];

extern const char kEcheLauncherIconsInMoreAppsButtonName[];
extern const char kEcheLauncherIconsInMoreAppsButtonDescription[];

extern const char kEcheNetworkConnectionStateName[];
extern const char kEcheNetworkConnectionStateDescription[];

extern const char kEcheSWADebugModeName[];
extern const char kEcheSWADebugModeDescription[];

extern const char kEcheSWAMeasureLatencyName[];
extern const char kEcheSWAMeasureLatencyDescription[];

extern const char kEcheSWASendStartSignalingName[];
extern const char kEcheSWASendStartSignalingDescription[];

extern const char kEcheSWADisableStunServerName[];
extern const char kEcheSWADisableStunServerDescription[];

extern const char kEcheSWACheckAndroidNetworkInfoName[];
extern const char kEcheSWACheckAndroidNetworkInfoDescription[];

extern const char kEcheSWAProcessAndroidAccessibilityTreeName[];
extern const char kEcheSWAProcessAndroidAccessibilityTreeDescription[];

extern const char kEnableNotificationImageDragName[];
extern const char kEnableNotificationImageDragDescription[];

extern const char kEnableOAuthIppName[];
extern const char kEnableOAuthIppDescription[];

extern const char kEnableVariableRefreshRateName[];
extern const char kEnableVariableRefreshRateDescription[];

extern const char kEnforceAshExtensionKeeplistName[];
extern const char kEnforceAshExtensionKeeplistDescription[];

extern const char kEolIncentiveName[];
extern const char kEolIncentiveDescription[];

extern const char kEolResetDismissedPrefsName[];
extern const char kEolResetDismissedPrefsDescription[];

extern const char kExoConsumedByImeByFlagName[];
extern const char kExoConsumedByImeByFlagDescription[];

extern const char kExoGamepadVibrationName[];
extern const char kExoGamepadVibrationDescription[];

extern const char kExoOrdinalMotionName[];
extern const char kExoOrdinalMotionDescription[];

extern const char kExoSurroundingTextOffsetName[];
extern const char kExoSurroundingTextOffsetDescription[];

extern const char kExperimentalAccessibilityDictationContextCheckingName[];
extern const char
    kExperimentalAccessibilityDictationContextCheckingDescription[];

extern const char kExperimentalAccessibilityGoogleTtsLanguagePacksName[];
extern const char kExperimentalAccessibilityGoogleTtsLanguagePacksDescription[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char kExposeOutOfProcessVideoDecodingToLacrosName[];
extern const char kExposeOutOfProcessVideoDecodingToLacrosDescription[];

extern const char kFederatedServiceName[];
extern const char kFederatedServiceDescription[];

extern const char kFileTransferEnterpriseConnectorName[];
extern const char kFileTransferEnterpriseConnectorDescription[];

extern const char kFilesAppExperimentalName[];
extern const char kFilesAppExperimentalDescription[];

extern const char kFilesConflictDialogName[];
extern const char kFilesConflictDialogDescription[];

extern const char kFilesDriveShortcutsName[];
extern const char kFilesDriveShortcutsDescription[];

extern const char kFilesExtractArchiveName[];
extern const char kFilesExtractArchiveDescription[];

extern const char kFilesInlineSyncStatusName[];
extern const char kFilesInlineSyncStatusDescription[];

extern const char kFilesInlineSyncStatusProgressEventsName[];
extern const char kFilesInlineSyncStatusProgressEventsDescription[];

extern const char kFilesSearchV2Name[];
extern const char kFilesSearchV2Description[];

extern const char kFilesSinglePartitionFormatName[];
extern const char kFilesSinglePartitionFormatDescription[];

extern const char kFilesTrashDriveName[];
extern const char kFilesTrashDriveDescription[];

extern const char kFirmwareUpdateJellyName[];
extern const char kFirmwareUpdateJellyDescription[];

extern const char kFirstPartyVietnameseInputName[];
extern const char kFirstPartyVietnameseInputDescription[];

extern const char kFocusFollowsCursorName[];
extern const char kFocusFollowsCursorDescription[];

extern const char kForceReSyncDriveName[];
extern const char kForceReSyncDriveDescription[];

extern const char kFrameThrottleFpsName[];
extern const char kFrameThrottleFpsDescription[];
extern const char kFrameThrottleFpsDefault[];
extern const char kFrameThrottleFps5[];
extern const char kFrameThrottleFps10[];
extern const char kFrameThrottleFps15[];
extern const char kFrameThrottleFps20[];
extern const char kFrameThrottleFps25[];
extern const char kFrameThrottleFps30[];

extern const char kFuseBoxDebugName[];
extern const char kFuseBoxDebugDescription[];

extern const char kHelpAppAppsDiscoveryName[];
extern const char kHelpAppAppsDiscoveryDescription[];

extern const char kHelpAppAutoTriggerInstallDialogName[];
extern const char kHelpAppAutoTriggerInstallDialogDescription[];

extern const char kHelpAppLauncherSearchName[];
extern const char kHelpAppLauncherSearchDescription[];

extern const char kHelpAppWelcomeTipsName[];
extern const char kHelpAppWelcomeTipsDescription[];

extern const char kHotspotName[];
extern const char kHotspotDescription[];

extern const char kDiacriticsOnPhysicalKeyboardLongpressName[];
extern const char kDiacriticsOnPhysicalKeyboardLongpressDescription[];

extern const char kDiacriticsOnPhysicalKeyboardLongpressDefaultOnName[];
extern const char kDiacriticsOnPhysicalKeyboardLongpressDefaultOnDescription[];

extern const char kDiacriticsUseReplaceSurroundingTextName[];
extern const char kDiacriticsUseReplaceSurroundingTextDescription[];

extern const char kHoldingSpacePredictabilityName[];
extern const char kHoldingSpacePredictabilityDescription[];

extern const char kHoldingSpaceRefreshName[];
extern const char kHoldingSpaceRefreshDescription[];

extern const char kHoldingSpaceSuggestionsName[];
extern const char kHoldingSpaceSuggestionsDescription[];

extern const char kImeAssistEmojiEnhancedName[];
extern const char kImeAssistEmojiEnhancedDescription[];

extern const char kImeAssistMultiWordName[];
extern const char kImeAssistMultiWordDescription[];

extern const char kImeAssistMultiWordExpandedName[];
extern const char kImeAssistMultiWordExpandedDescription[];

extern const char kImeFstDecoderParamsUpdateName[];
extern const char kImeFstDecoderParamsUpdateDescription[];

extern const char kImeTrayHideVoiceButtonName[];
extern const char kImeTrayHideVoiceButtonDescription[];

extern const char kLacrosMoveProfileMigrationName[];
extern const char kLacrosMoveProfileMigrationDescription[];

extern const char kLacrosProfileMigrationForceOffName[];
extern const char kLacrosProfileMigrationForceOffDescription[];

extern const char kVirtualKeyboardNewHeaderName[];
extern const char kVirtualKeyboardNewHeaderDescription[];

extern const char kHindiInscriptLayoutName[];
extern const char kHindiInscriptLayoutDescription[];

extern const char kImeSystemEmojiPickerExtensionName[];
extern const char kImeSystemEmojiPickerExtensionDescription[];

extern const char kImeSystemEmojiPickerGIFSupportName[];
extern const char kImeSystemEmojiPickerGIFSupportDescription[];

extern const char kImeSystemEmojiPickerClipboardName[];
extern const char kImeSystemEmojiPickerClipboardDescription[];

extern const char kImeSystemEmojiPickerSearchExtensionName[];
extern const char kImeSystemEmojiPickerSearchExtensionDescription[];

extern const char kImeStylusHandwritingName[];
extern const char kImeStylusHandwritingDescription[];

extern const char kImeUsEnglishModelUpdateName[];
extern const char kImeUsEnglishModelUpdateDescription[];

extern const char kJellyColorsName[];
extern const char kJellyColorsDescription[];

extern const char kLacrosAvailabilityIgnoreName[];
extern const char kLacrosAvailabilityIgnoreDescription[];

extern const char kLacrosOnlyName[];
extern const char kLacrosOnlyDescription[];

extern const char kLacrosPrimaryName[];
extern const char kLacrosPrimaryDescription[];

extern const char kLacrosStabilityName[];
extern const char kLacrosStabilityDescription[];

extern const char kLacrosSelectionName[];
extern const char kLacrosSelectionDescription[];
extern const char kLacrosSelectionRootfsDescription[];
extern const char kLacrosSelectionStatefulDescription[];

extern const char kLacrosSelectionPolicyIgnoreName[];
extern const char kLacrosSelectionPolicyIgnoreDescription[];

extern const char kLacrosSupportName[];
extern const char kLacrosSupportDescription[];

extern const char kLacrosWaylandLoggingName[];
extern const char kLacrosWaylandLoggingDescription[];

extern const char kLauncherItemSuggestName[];
extern const char kLauncherItemSuggestDescription[];

extern const char kLimitShelfItemsToActiveDeskName[];
extern const char kLimitShelfItemsToActiveDeskDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kEnableHardwareMirrorModeName[];
extern const char kEnableHardwareMirrorModeDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMediaAppPdfSignatureName[];
extern const char kMediaAppPdfSignatureDescription[];

extern const char kMeteredShowToggleName[];
extern const char kMeteredShowToggleDescription[];

extern const char kMicrophoneMuteNotificationsName[];
extern const char kMicrophoneMuteNotificationsDescription[];

extern const char kMicrophoneMuteSwitchDeviceName[];
extern const char kMicrophoneMuteSwitchDeviceDescription[];

extern const char kMultiZoneRgbKeyboardName[];
extern const char kMultiZoneRgbKeyboardDescription[];

extern const char kMultilingualTypingName[];
extern const char kMultilingualTypingDescription[];

extern const char kNearbySharingSelfShareName[];
extern const char kNearbySharingSelfShareDescription[];

extern const char kOobeHidDetectionRevampName[];
extern const char kOobeHidDetectionRevampDescription[];

extern const char kOsSettingsAppBadgingToggleName[];
extern const char kOsSettingsAppBadgingToggleDescription[];

extern const char kOsSettingsRevampWayfindingName[];
extern const char kOsSettingsRevampWayfindingDescription[];

extern const char kPcieBillboardNotificationName[];
extern const char kPcieBillboardNotificationDescription[];

extern const char kPerformantSplitViewResizing[];
extern const char kPerformantSplitViewResizingDescription[];

extern const char kPhoneHubCallNotificationName[];
extern const char kPhoneHubCallNotificationDescription[];

extern const char kPhoneHubCameraRollName[];
extern const char kPhoneHubCameraRollDescription[];

extern const char kPhoneHubFeatureSetupErrorHandlingName[];
extern const char kPhoneHubFeatureSetupErrorHandlingDescription[];

extern const char kPhoneHubNudgeName[];
extern const char kPhoneHubNudgeDescription[];

extern const char kPolicyProvidedTrustAnchorsAllowedAtLockScreenName[];
extern const char kPolicyProvidedTrustAnchorsAllowedAtLockScreenDescription[];

extern const char kPreferDcheckName[];
extern const char kPreferDcheckDescription[];

extern const char kPrinterSettingsPrinterStatusName[];
extern const char kPrinterSettingsPrinterStatusDescription[];

extern const char kPrinterSettingsRevampName[];
extern const char kPrinterSettingsRevampDescription[];

extern const char kPrintPreviewDiscoveredPrintersName[];
extern const char kPrintPreviewDiscoveredPrintersDescription[];

extern const char kPrintingPpdChannelName[];
extern const char kPrintingPpdChannelDescription[];

extern const char kPrintManagementJellyName[];
extern const char kPrintManagementJellyDescription[];

extern const char kPrintManagementSetupAssistanceName[];
extern const char kPrintManagementSetupAssistanceDescription[];

extern const char kProductivityLauncherName[];
extern const char kProductivityLauncherDescription[];

extern const char kProductivityLauncherImageSearchName[];
extern const char kProductivityLauncherImageSearchDescription[];

extern const char kProjectorName[];
extern const char kProjectorDescription[];

extern const char kProjectorLocalPlaybackName[];
extern const char kProjectorLocalPlaybackDescription[];

extern const char kProjectorAppDebugName[];
extern const char kProjectorAppDebugDescription[];

extern const char kProjectorServerSideSpeechRecognitionName[];
extern const char kProjectorServerSideSpeechRecognitionDescription[];

extern const char kQsRevampName[];
extern const char kQsRevampDescription[];

extern const char kReleaseNotesNotificationAllChannelsName[];
extern const char kReleaseNotesNotificationAllChannelsDescription[];

extern const char kRenderArcNotificationsByChromeName[];
extern const char kRenderArcNotificationsByChromeDescription[];

extern const char kArcWindowPredictorName[];
extern const char kArcWindowPredictorDescription[];

extern const char kArcInputOverlayNameBeta[];
extern const char kArcInputOverlayDescriptionBeta[];

extern const char kArcInputOverlayNameAlphaV2[];
extern const char kArcInputOverlayDescriptionAlphaV2[];

extern const char kScalableIphName[];
extern const char kScalableIphDescription[];

extern const char kScanningAppJellyName[];
extern const char kScanningAppJellyDescription[];

extern const char kSecondaryGoogleAccountUsageName[];
extern const char kSecondaryGoogleAccountUsageDescription[];

extern const char kShelfAutoHideSeparationName[];
extern const char kShelfAutoHideSeparationDescription[];

extern const char kShimlessRMAComplianceCheckName[];
extern const char kShimlessRMAComplianceCheckDescription[];

extern const char kShimlessRMAOsUpdateName[];
extern const char kShimlessRMAOsUpdateDescription[];

extern const char kShimlessRMADisableDarkModeName[];
extern const char kShimlessRMADisableDarkModeDescription[];

extern const char kShimlessRMADiagnosticPageName[];
extern const char kShimlessRMADiagnosticPageDescription[];

extern const char kShortcutCustomizationJellyName[];
extern const char kShortcutCustomizationJellyDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];

extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kMediaDynamicCgroupName[];
extern const char kMediaDynamicCgroupDescription[];

extern const char kMissiveStorageName[];
extern const char kMissiveStorageDescription[];

extern const char kShowBluetoothDebugLogToggleName[];
extern const char kShowBluetoothDebugLogToggleDescription[];

extern const char kBluetoothSessionizedMetricsName[];
extern const char kBluetoothSessionizedMetricsDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSnoopingProtectionName[];
extern const char kSnoopingProtectionDescription[];

extern const char kSpeakOnMuteOptInNudgePrefsResetName[];
extern const char kSpeakOnMuteOptInNudgePrefsResetDescription[];

extern const char kSpectreVariant2MitigationName[];
extern const char kSpectreVariant2MitigationDescription[];

extern const char kSystemJapanesePhysicalTypingName[];
extern const char kSystemJapanesePhysicalTypingDescription[];

extern const char kSystemLiveCaptionName[];
extern const char kSystemLiveCaptionDescription[];

extern const char kSystemNudgeV2Name[];
extern const char kSystemNudgeV2Description[];

extern const char kCaptivePortalErrorPageName[];
extern const char kCaptivePortalErrorPageDescription[];

extern const char kTerminalAlternativeEmulatorName[];
extern const char kTerminalAlternativeEmulatorDescription[];

extern const char kTerminalDevName[];
extern const char kTerminalDevDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTimeOfDayScreenSaverName[];
extern const char kTimeOfDayScreenSaverDescription[];

extern const char kTimeOfDayWallpaperName[];
extern const char kTimeOfDayWallpaperDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kTrafficCountersEnabledName[];
extern const char kTrafficCountersEnabledDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUploadOfficeToCloudName[];
extern const char kUploadOfficeToCloudDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kUXStudy1Name[];
extern const char kUXStudy1Description[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVaapiWebPImageDecodeAccelerationName[];
extern const char kVaapiWebPImageDecodeAccelerationDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardDisabledName[];
extern const char kVirtualKeyboardDisabledDescription[];

extern const char kVirtualKeyboardMultitouchName[];
extern const char kVirtualKeyboardMultitouchDescription[];

extern const char kVirtualKeyboardRoundCornersName[];
extern const char kVirtualKeyboardRoundCornersDescription[];

extern const char kWakeOnWifiAllowedName[];
extern const char kWakeOnWifiAllowedDescription[];

extern const char kWebAppsCrosapiName[];
extern const char kWebAppsCrosapiDescription[];

extern const char kWelcomeScreenName[];
extern const char kWelcomeScreenDescription[];

extern const char kWifiConnectMacAddressRandomizationName[];
extern const char kWifiConnectMacAddressRandomizationDescription[];

extern const char kLauncherGameSearchName[];
extern const char kLauncherGameSearchDescription[];

extern const char kLauncherKeywordExtractionScoring[];
extern const char kLauncherKeywordExtractionScoringDescription[];

extern const char kLauncherLocalImageSearchName[];
extern const char kLauncherLocalImageSearchDescription[];

extern const char kLauncherLocalImageSearchOcrName[];
extern const char kLauncherLocalImageSearchOcrDescription[];

extern const char kLauncherLocalImageSearchIcaName[];
extern const char kLauncherLocalImageSearchIcaDescription[];

extern const char kLauncherFuzzyMatchAcrossProvidersName[];
extern const char kLauncherFuzzyMatchAcrossProvidersDescription[];

extern const char kLauncherFuzzyMatchForOmniboxName[];
extern const char kLauncherFuzzyMatchForOmniboxDescription[];

extern const char kLauncherSystemInfoAnswerCardsName[];
extern const char kLauncherSystemInfoAnswerCardsDescription[];

extern const char kMacAddressRandomizationName[];
extern const char kMacAddressRandomizationDescription[];

extern const char kSmdsSupportName[];
extern const char kSmdsSupportDescription[];

extern const char kSmdsSupportEuiccUploadName[];
extern const char kSmdsSupportEuiccUploadDescription[];

extern const char kSmdsDbusMigrationName[];
extern const char kSmdsDbusMigrationDescription[];

extern const char kOobeJellyName[];
extern const char kOobeJellyDescription[];

extern const char kOobeSimonName[];
extern const char kOobeSimonDescription[];

extern const char kLibAssistantV2MigrationName[];
extern const char kLibAssistantV2MigrationDescription[];
// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kExperimentalWebAppProfileIsolationName[];
extern const char kExperimentalWebAppProfileIsolationDescription[];

extern const char kExperimentalWebAppStoragePartitionIsolationName[];
extern const char kExperimentalWebAppStoragePartitionIsolationDescription[];

extern const char kLacrosAuraCaptureName[];
extern const char kLacrosAuraCaptureDescription[];

extern const char kLacrosMergeIcuDataFileName[];
extern const char kLacrosMergeIcuDataFileDescription[];
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
extern const char kGetAllScreensMediaName[];
extern const char kGetAllScreensMediaDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kRunOnOsLoginName[];
extern const char kRunOnOsLoginDescription[];

extern const char kPreventCloseName[];
extern const char kPreventCloseDescription[];

extern const char kKeepAliveName[];
extern const char kKeepAliveDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kCrOSDspBasedAecAllowedName[];
extern const char kCrOSDspBasedAecAllowedDescription[];

extern const char kCrOSDspBasedNsAllowedName[];
extern const char kCrOSDspBasedNsAllowedDescription[];

extern const char kCrOSDspBasedAgcAllowedName[];
extern const char kCrOSDspBasedAgcAllowedDescription[];

extern const char kCrosPrivacyHubName[];
extern const char kCrosPrivacyHubDescription[];

extern const char kCrosPrivacyHubV0Name[];
extern const char kCrosPrivacyHubV0Description[];

extern const char kCrOSEnforceSystemAecName[];
extern const char kCrOSEnforceSystemAecDescription[];

extern const char kCrOSEnforceSystemAecAgcName[];
extern const char kCrOSEnforceSystemAecAgcDescription[];

extern const char kCrOSEnforceSystemAecNsName[];
extern const char kCrOSEnforceSystemAecNsDescription[];

extern const char kCrOSEnforceSystemAecNsAgcName[];
extern const char kCrOSEnforceSystemAecNsAgcDescription[];

extern const char kDisableIdleSocketsCloseOnMemoryPressureName[];
extern const char kDisableIdleSocketsCloseOnMemoryPressureDescription[];

extern const char kDisableOfficeEditingComponentAppName[];
extern const char kDisableOfficeEditingComponentAppDescription[];

extern const char kIntentChipSkipsPickerName[];
extern const char kIntentChipSkipsPickerDescription[];

extern const char kKioskEnableAppServiceName[];
extern const char kKioskEnableAppServiceDescription[];

extern const char kLacrosColorManagementName[];
extern const char kLacrosColorManagementDescription[];

extern const char kLinkCapturingInfoBarName[];
extern const char kLinkCapturingInfoBarDescription[];

extern const char kLinkCapturingUiUpdateName[];
extern const char kLinkCapturingUiUpdateDescription[];

extern const char kMessagesPreinstallName[];
extern const char kMessagesPreinstallDescription[];

extern const char kOneGroupPerRendererName[];
extern const char kOneGroupPerRendererDescription[];

extern const char kPreinstalledWebAppWindowExperimentName[];
extern const char kPreinstalledWebAppWindowExperimentDescription[];

extern const char kDisableQuickAnswersV2TranslationName[];
extern const char kDisableQuickAnswersV2TranslationDescription[];

extern const char kQuickAnswersRichCardName[];
extern const char kQuickAnswersRichCardDescription[];

extern const char kSyncChromeOSExplicitPassphraseSharingName[];
extern const char kSyncChromeOSExplicitPassphraseSharingDescription[];

extern const char kTouchTextEditingRedesignName[];
extern const char kTouchTextEditingRedesignDescription[];

extern const char kIgnoreUiGainsName[];
extern const char kIgnoreUiGainsDescription[];

extern const char kShowForceRespectUiGainsToggleName[];
extern const char kShowForceRespectUiGainsToggleDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
extern const char kVaapiVP9kSVCEncoderName[];
extern const char kVaapiVP9kSVCEncoderDescription[];
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
extern const char kChromeOSDirectVideoDecoderName[];
extern const char kChromeOSDirectVideoDecoderDescription[];
extern const char kChromeOSHWVBREncodingName[];
extern const char kChromeOSHWVBREncodingDescription[];
#if defined(ARCH_CPU_ARM_FAMILY)
extern const char kPreferGLImageProcessorName[];
extern const char kPreferGLImageProcessorDescription[];
#endif  // defined(ARCH_CPU_ARM_FAMILY
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kZeroCopyVideoCaptureName[];
extern const char kZeroCopyVideoCaptureDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)
extern const char kRevampedPasswordManagementBubbleName[];
extern const char kRevampedPasswordManagementBubbleDescription[];

extern const char kSideSearchName[];
extern const char kSideSearchDescription[];

extern const char kSearchWebInSidePanelName[];
extern const char kSearchWebInSidePanelDescription[];
#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
extern const char kQuickCommandsName[];
extern const char kQuickCommandsDescription[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // defined (OS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
extern const char kWebShareName[];
extern const char kWebShareDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
extern const char kWebBluetoothConfirmPairingSupportName[];
extern const char kWebBluetoothConfirmPairingSupportDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
extern const char kOzonePlatformHintChoiceDefault[];
extern const char kOzonePlatformHintChoiceAuto[];
extern const char kOzonePlatformHintChoiceX11[];
extern const char kOzonePlatformHintChoiceWayland[];

extern const char kOzonePlatformHintName[];
extern const char kOzonePlatformHintDescription[];
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
extern const char kSkipUndecryptablePasswordsName[];
extern const char kSkipUndecryptablePasswordsDescription[];

extern const char kForcePasswordInitialSyncWhenDecryptionFailsName[];
extern const char kForcePasswordInitialSyncWhenDecryptionFailsDescription[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)
extern const char kFollowingFeedSidepanelName[];
extern const char kFollowingFeedSidepanelDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
extern const char kLocalWebApprovalsName[];
extern const char kLocalWebApprovalsDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kEnableProtoApiForClassifyUrlName[];
extern const char kEnableProtoApiForClassifyUrlDescription[];
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
extern const char kUseOutOfProcessVideoDecodingName[];
extern const char kUseOutOfProcessVideoDecodingDescription[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Feature flags --------------------------------------------------------------

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
extern const char kChromeWideEchoCancellationName[];
extern const char kChromeWideEchoCancellationDescription[];
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_CARDBOARD)
extern const char kEnableCardboardName[];
extern const char kEnableCardboardDescription[];
#endif  // ENABLE_CARDBOARD

#if BUILDFLAG(ENABLE_NACL)
extern const char kNaclName[];
extern const char kNaclDescription[];

extern const char kVerboseLoggingInNaclName[];
extern const char kVerboseLoggingInNaclDescription[];
extern const char kVerboseLoggingInNaclChoiceDefault[];
extern const char kVerboseLoggingInNaclChoiceLow[];
extern const char kVerboseLoggingInNaclChoiceMedium[];
extern const char kVerboseLoggingInNaclChoiceHigh[];
extern const char kVerboseLoggingInNaclChoiceHighest[];
extern const char kVerboseLoggingInNaclChoiceDisabled[];
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_OOP_PRINTING)
extern const char kEnableOopPrintDriversName[];
extern const char kEnableOopPrintDriversDescription[];
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)
extern const char kPaintPreviewDemoName[];
extern const char kPaintPreviewDemoDescription[];
#endif  // ENABLE_PAINT_PREVIEW && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kWebUITabStripFlagId[];
extern const char kWebUITabStripName[];
extern const char kWebUITabStripDescription[];

extern const char kWebUITabStripContextMenuAfterTapName[];
extern const char kWebUITabStripContextMenuAfterTapDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kWebUITabStripTabDragIntegrationName[];
extern const char kWebUITabStripTabDragIntegrationDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
extern const char kElasticOverscrollName[];
extern const char kElasticOverscrollDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
extern const char kUIDebugToolsName[];
extern const char kUIDebugToolsDescription[];

extern const char kDesktopDetailedLanguageSettingsName[];
extern const char kDesktopDetailedLanguageSettingsDescription[];

extern const char kSyncPollImmediatelyOnEveryStartupName[];
extern const char kSyncPollImmediatelyOnEveryStartupDescription[];

extern const char kSyncPromoAfterSigninInterceptName[];
extern const char kSyncPromoAfterSigninInterceptDescription[];

extern const char kSigninInterceptBubbleV2Name[];
extern const char kSigninInterceptBubbleV2Description[];
#endif

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
extern const char kDataRetentionPoliciesDisableSyncTypesNeededName[];
extern const char kDataRetentionPoliciesDisableSyncTypesNeededDescription[];
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
extern const char kWebrtcPipeWireCapturerName[];
extern const char kWebrtcPipeWireCapturerDescription[];
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kChromeKioskEnableLacrosName[];
extern const char kChromeKioskEnableLacrosDescription[];

extern const char kWebKioskEnableLacrosName[];
extern const char kWebKioskEnableLacrosDescription[];

extern const char kDisableLacrosTtsSupportName[];
extern const char kDisableLacrosTtsSupportDescription[];

extern const char kPromiseIconsName[];
extern const char kPromiseIconsDescription[];

extern const char kEnableAudioFocusEnforcementName[];
extern const char kEnableAudioFocusEnforcementDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
