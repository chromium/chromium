// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/check_op.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/buildflags.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

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

extern const char kEnableMediaInternalsName[];
extern const char kEnableMediaInternalsDescription[];

#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kAccessiblePDFFormName[];
extern const char kAccessiblePDFFormDescription[];
#endif  // BUILDFLAG(ENABLE_PLUGINS)

extern const char kAccountIdMigrationName[];
extern const char kAccountIdMigrationDescription[];

extern const char kAlignFontDisplayAutoTimeoutWithLCPGoalName[];
extern const char kAlignFontDisplayAutoTimeoutWithLCPGoalDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAllowSyncXHRInPageDismissalName[];
extern const char kAllowSyncXHRInPageDismissalDescription[];

extern const char kAnimatedImageResumeName[];
extern const char kAnimatedImageResumeDescription[];

extern const char kAriaElementReflectionName[];
extern const char kAriaElementReflectionDescription[];

extern const char kCOLRV1FontsName[];
extern const char kCOLRV1FontsDescription[];

extern const char kCSSContainerQueriesName[];
extern const char kCSSContainerQueriesDescription[];

extern const char kContentLanguagesInLanguagePickerName[];
extern const char kContentLanguagesInLanguagePickerDescription[];

extern const char kConversionMeasurementApiName[];
extern const char kConversionMeasurementApiDescription[];

extern const char kConversionMeasurementDebugModeName[];
extern const char kConversionMeasurementDebugModeDescription[];

extern const char kDefaultChromeAppUninstallSyncName[];
extern const char kDefaultChromeAppUninstallSyncDescription[];

extern const char kDeprecateMenagerieAPIName[];
extern const char kDeprecateMenagerieAPIDescription[];

extern const char kDetectedSourceLanguageOptionName[];
extern const char kDetectedSourceLanguageOptionDescription[];

extern const char kDetectFormSubmissionOnFormClearName[];
extern const char kDetectFormSubmissionOnFormClearDescription[];

extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

extern const char kEnableBluetoothSerialPortProfileInSerialApiName[];
extern const char kEnableBluetoothSerialPortProfileInSerialApiDescription[];

extern const char kEnableFtpName[];
extern const char kEnableFtpDescription[];

extern const char kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedName[];
extern const char
    kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedDescription[];

extern const char kEnableSignedExchangeSubresourcePrefetchName[];
extern const char kEnableSignedExchangeSubresourcePrefetchDescription[];

extern const char kEnableSignedExchangePrefetchCacheForNavigationsName[];
extern const char kEnableSignedExchangePrefetchCacheForNavigationsDescription[];

extern const char kAudioWorkletRealtimeThreadName[];
extern const char kAudioWorkletRealtimeThreadDescription[];

extern const char kUpdatedCellularActivationUiName[];
extern const char kUpdatedCellularActivationUiDescription[];

extern const char kUseLookalikesForNavigationSuggestionsName[];
extern const char kUseLookalikesForNavigationSuggestionsDescription[];

extern const char kUseWallpaperStagingUrlName[];
extern const char kUseWallpaperStagingUrlDescription[];

extern const char kUseMessagesStagingUrlName[];
extern const char kUseMessagesStagingUrlDescription[];

extern const char kUseCustomMessagesDomainName[];
extern const char kUseCustomMessagesDomainDescription[];

extern const char kAndroidPictureInPictureAPIName[];
extern const char kAndroidPictureInPictureAPIDescription[];

extern const char kAppCacheName[];
extern const char kAppCacheDescription[];

extern const char kAutofillAlwaysReturnCloudTokenizedCardName[];
extern const char kAutofillAlwaysReturnCloudTokenizedCardDescription[];

extern const char kAutofillAssistantChromeEntryName[];
extern const char kAutofillAssistantChromeEntryDescription[];

extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

extern const char kAutofillEnableGoogleIssuedCardName[];
extern const char kAutofillEnableGoogleIssuedCardDescription[];

extern const char kAutofillEnableOfferNotificationName[];
extern const char kAutofillEnableOfferNotificationDescription[];

extern const char kAutofillEnableOfferNotificationCrossTabTrackingName[];
extern const char kAutofillEnableOfferNotificationCrossTabTrackingDescription[];

extern const char kAutofillEnableOffersInClankKeyboardAccessoryName[];
extern const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[];

extern const char kAutofillEnableOffersInDownstreamName[];
extern const char kAutofillEnableOffersInDownstreamDescription[];

extern const char kAutofillEnableToolbarStatusChipName[];
extern const char kAutofillEnableToolbarStatusChipDescription[];

extern const char kAutofillEnableVirtualCardName[];
extern const char kAutofillEnableVirtualCardDescription[];

extern const char kAutofillFixOfferInIncognitoName[];
extern const char kAutofillFixOfferInIncognitoDescription[];

extern const char kAutofillParseMerchantPromoCodeFieldsName[];
extern const char kAutofillParseMerchantPromoCodeFieldsDescription[];

extern const char kAutofillProfileClientValidationName[];
extern const char kAutofillProfileClientValidationDescription[];

extern const char kAutofillProfileServerValidationName[];
extern const char kAutofillProfileServerValidationDescription[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

extern const char kAutofillSaveAndFillVPAName[];
extern const char kAutofillSaveAndFillVPADescription[];

extern const char kAutofillUseImprovedLabelDisambiguationName[];
extern const char kAutofillUseImprovedLabelDisambiguationDescription[];

extern const char kAutoScreenBrightnessName[];
extern const char kAutoScreenBrightnessDescription[];

extern const char kAvatarToolbarButtonName[];
extern const char kAvatarToolbarButtonDescription[];

extern const char kBackForwardCacheName[];
extern const char kBackForwardCacheDescription[];

extern const char kBentoName[];
extern const char kBentoDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kChangePasswordAffiliationInfoName[];
extern const char kChangePasswordAffiliationInfoDescription[];

extern const char kCheckOfflineCapabilityName[];
extern const char kCheckOfflineCapabilityDescription[];

extern const char kChromeLabsName[];
extern const char kChromeLabsDescription[];

extern const char kCompositeAfterPaintName[];
extern const char kCompositeAfterPaintDescription[];

extern const char kComputePressureAPIName[];
extern const char kComputePressureAPIDescription[];

extern const char kContextMenuGoogleLensChipName[];
extern const char kContextMenuGoogleLensChipDescription[];

extern const char kContextMenuSearchWithGoogleLensName[];
extern const char kContextMenuSearchWithGoogleLensDescription[];

extern const char kContextMenuShopWithGoogleLensName[];
extern const char kContextMenuShopWithGoogleLensDescription[];

extern const char kContextMenuSearchAndShopWithGoogleLensName[];
extern const char kContextMenuSearchAndShopWithGoogleLensDescription[];

extern const char kContextMenuTranslateWithGoogleLensName[];
extern const char kContextMenuTranslateWithGoogleLensDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kClientStorageAccessContextAuditingName[];
extern const char kClientStorageAccessContextAuditingDescription[];

extern const char kClipboardFilenamesName[];
extern const char kClipboardFilenamesDescription[];

extern const char kConditionalTabStripAndroidName[];
extern const char kConditionalTabStripAndroidDescription[];

extern const char kClearCrossBrowsingContextGroupMainFrameNameName[];
extern const char kClearCrossBrowsingContextGroupMainFrameNameDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kChromeTipsInMainMenuName[];
extern const char kChromeTipsInMainMenuDescription[];

extern const char kChromeTipsInMainMenuNewBadgeName[];
extern const char kChromeTipsInMainMenuNewBadgeDescription[];
#endif

extern const char kDarkLightTestName[];
extern const char kDarkLightTestDescription[];

extern const char kDecodeJpeg420ImagesToYUVName[];
extern const char kDecodeJpeg420ImagesToYUVDescription[];

extern const char kDecodeLossyWebPImagesToYUVName[];
extern const char kDecodeLossyWebPImagesToYUVDescription[];

extern const char kDetectTargetEmbeddingLookalikesName[];
extern const char kDetectTargetEmbeddingLookalikesDescription[];

extern const char kDoubleBufferCompositingName[];
extern const char kDoubleBufferCompositingDescription[];

extern const char kDnsOverHttpsName[];
extern const char kDnsOverHttpsDescription[];

extern const char kDnsHttpssvcName[];
extern const char kDnsHttpssvcDescription[];

extern const char kEnableFirstPartySetsName[];
extern const char kEnableFirstPartySetsDescription[];

extern const char kEnablePasswordsAccountStorageName[];
extern const char kEnablePasswordsAccountStorageDescription[];

extern const char kEnablePasswordsAccountStorageIPHName[];
extern const char kEnablePasswordsAccountStorageIPHDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionName[];
extern const char kExperimentalAccessibilityLanguageDetectionDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionDynamicName[];
extern const char
    kExperimentalAccessibilityLanguageDetectionDynamicDescription[];

extern const char kFontAccessAPIName[];
extern const char kFontAccessAPIDescription[];

extern const char kFontAccessPersistentName[];
extern const char kFontAccessPersistentDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileSCRGBLinear[];
extern const char kForceColorProfileHDR10[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kDynamicColorGamutName[];
extern const char kDynamicColorGamutDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kCooperativeSchedulingName[];
extern const char kCooperativeSchedulingDescription[];

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kDarkenWebsitesCheckboxInThemesSettingName[];
extern const char kDarkenWebsitesCheckboxInThemesSettingDescription[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kDocumentTransitionName[];
extern const char kDocumentTransitionDescription[];

extern const char kEnableAccessibilityObjectModelName[];
extern const char kEnableAccessibilityObjectModelDescription[];

extern const char kEnableAudioFocusEnforcementName[];
extern const char kEnableAudioFocusEnforcementDescription[];

extern const char kEnableAutofillAccountWalletStorageName[];
extern const char kEnableAutofillAccountWalletStorageDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardAuthenticationName[];
extern const char kEnableAutofillCreditCardAuthenticationDescription[];

extern const char kEnableAutofillCreditCardCvcPromptGoogleLogoName[];
extern const char kEnableAutofillCreditCardCvcPromptGoogleLogoDescription[];

extern const char kEnableAutofillCreditCardUploadFeedbackName[];
extern const char kEnableAutofillCreditCardUploadFeedbackDescription[];

extern const char
    kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersName[];
extern const char
    kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersDescription
        [];

extern const char
    kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersName[];
extern const char
    kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersDescription[];

extern const char kEnableAutofillPasswordInfoBarAccountIndicationFooterName[];
extern const char
    kEnableAutofillPasswordInfoBarAccountIndicationFooterDescription[];

extern const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterName[];
extern const char
    kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription[];

extern const char kEnableExperimentalCookieFeaturesName[];
extern const char kEnableExperimentalCookieFeaturesDescription[];

extern const char kEnableSaveDataName[];
extern const char kEnableSaveDataDescription[];

extern const char kEnableNavigationPredictorName[];
extern const char kEnableNavigationPredictorDescription[];

extern const char kEnablePreconnectToSearchName[];
extern const char kEnablePreconnectToSearchDescription[];

extern const char kEnableRemovingAllThirdPartyCookiesName[];
extern const char kEnableRemovingAllThirdPartyCookiesDescription[];

extern const char kEnableBrowsingDataLifetimeManagerName[];
extern const char kEnableBrowsingDataLifetimeManagerDescription[];

extern const char kDataReductionProxyServerAlternative1[];
extern const char kDataReductionProxyServerAlternative2[];
extern const char kDataReductionProxyServerAlternative3[];
extern const char kDataReductionProxyServerAlternative4[];
extern const char kDataReductionProxyServerAlternative5[];
extern const char kDataReductionProxyServerAlternative6[];
extern const char kDataReductionProxyServerAlternative7[];
extern const char kDataReductionProxyServerAlternative8[];
extern const char kDataReductionProxyServerAlternative9[];
extern const char kDataReductionProxyServerAlternative10[];
extern const char kEnableDataReductionProxyServerExperimentName[];
extern const char kEnableDataReductionProxyServerExperimentDescription[];

extern const char kColorProviderRedirectionName[];
extern const char kColorProviderRedirectionDescription[];

extern const char kDesktopPWAsAppIconShortcutsMenuName[];
extern const char kDesktopPWAsAppIconShortcutsMenuDescription[];

extern const char kDesktopPWAsAppIconShortcutsMenuUIName[];
extern const char kDesktopPWAsAppIconShortcutsMenuUIDescription[];

extern const char kDesktopPWAsAttentionBadgingCrOSName[];
extern const char kDesktopPWAsAttentionBadgingCrOSDescription[];
extern const char kDesktopPWAsAttentionBadgingCrOSApiAndNotifications[];
extern const char kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications[];
extern const char kDesktopPWAsAttentionBadgingCrOSApiOnly[];
extern const char kDesktopPWAsAttentionBadgingCrOSNotificationsOnly[];

extern const char kDesktopPWAsRemoveStatusBarName[];
extern const char kDesktopPWAsRemoveStatusBarDescription[];

extern const char kDesktopPWAsElidedExtensionsMenuName[];
extern const char kDesktopPWAsElidedExtensionsMenuDescription[];

extern const char kDesktopPWAsFlashAppNameInsteadOfOriginName[];
extern const char kDesktopPWAsFlashAppNameInsteadOfOriginDescription[];

extern const char kDesktopPWAsLinkCapturingName[];
extern const char kDesktopPWAsLinkCapturingDescription[];

extern const char kDesktopPWAsTabStripName[];
extern const char kDesktopPWAsTabStripDescription[];

extern const char kDesktopPWAsTabStripLinkCapturingName[];
extern const char kDesktopPWAsTabStripLinkCapturingDescription[];

extern const char kDesktopPWAsRunOnOsLoginName[];
extern const char kDesktopPWAsRunOnOsLoginDescription[];

extern const char kDesktopPWAsProtocolHandlingName[];
extern const char kDesktopPWAsProtocolHandlingDescription[];

extern const char kDesktopPWAsUrlHandlingName[];
extern const char kDesktopPWAsUrlHandlingDescription[];

extern const char kDesktopPWAsWindowControlsOverlayName[];
extern const char kDesktopPWAsWindowControlsOverlayDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription[];

extern const char kEnableSyncRequiresPoliciesLoadedName[];
extern const char kEnableSyncRequiresPoliciesLoadedDescription[];

extern const char kEnableTLS13EarlyDataName[];
extern const char kEnableTLS13EarlyDataDescription[];

extern const char kPostQuantumCECPQ2Name[];
extern const char kPostQuantumCECPQ2Description[];

extern const char kMacCoreLocationImplementationName[];
extern const char kMacCoreLocationImplementationDescription[];

extern const char kMacCoreLocationBackendName[];
extern const char kMacCoreLocationBackendDescription[];

extern const char kNewMacNotificationAPIName[];
extern const char kNewMacNotificationAPIDescription[];

extern const char kNotificationsViaHelperAppName[];
extern const char kNotificationsViaHelperAppDescription[];

extern const char kWinrtGeolocationImplementationName[];
extern const char kWinrtGeolocationImplementationDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableLayoutNGName[];
extern const char kEnableLayoutNGDescription[];

extern const char kEnableLayoutNGTableName[];
extern const char kEnableLayoutNGTableDescription[];

extern const char kEnableLazyFrameLoadingName[];
extern const char kEnableLazyFrameLoadingDescription[];

extern const char kEnableLazyImageLoadingName[];
extern const char kEnableLazyImageLoadingDescription[];

extern const char kEnableMediaSessionServiceName[];
extern const char kEnableMediaSessionServiceDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableNetworkServiceInProcessName[];
extern const char kEnableNetworkServiceInProcessDescription[];

extern const char kEnableTranslateSubFramesName[];
extern const char kEnableTranslateSubFramesDescription[];

extern const char kEnableWindowsGamingInputDataFetcherName[];
extern const char kEnableWindowsGamingInputDataFetcherDescription[];

extern const char kBlockInsecurePrivateNetworkRequestsName[];
extern const char kBlockInsecurePrivateNetworkRequestsDescription[];

extern const char kCrossOriginOpenerPolicyReportingName[];
extern const char kCrossOriginOpenerPolicyReportingDescription[];

extern const char kCrossOriginOpenerPolicyAccessReportingName[];
extern const char kCrossOriginOpenerPolicyAccessReportingDescription[];

extern const char kCrossOriginIsolatedName[];
extern const char kCrossOriginIsolatedDescription[];

extern const char kDeprecateAltClickName[];
extern const char kDeprecateAltClickDescription[];

extern const char kDiagnosticsAppName[];
extern const char kDiagnosticsAppDescription[];

extern const char kDisableKeepaliveFetchName[];
extern const char kDisableKeepaliveFetchDescription[];

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
extern const char kMemlogStackModeMixed[];
extern const char kMemlogStackModeNative[];
extern const char kMemlogStackModeNativeWithThreadNames[];
extern const char kMemlogStackModePseudo[];

extern const char kDownloadAutoResumptionNativeName[];
extern const char kDownloadAutoResumptionNativeDescription[];

extern const char kDownloadLaterName[];
extern const char kDownloadLaterDescription[];

extern const char kDownloadLaterDebugOnWifiName[];
extern const char kDownloadLaterDebugOnWifiNameDescription[];

extern const char kEnableLoginDetectionName[];
extern const char kEnableLoginDetectionDescription[];

extern const char kEnableManagedConfigurationWebApiName[];
extern const char kEnableManagedConfigurationWebApiDescription[];

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePciguardUiName[];
extern const char kEnablePciguardUiDescription[];

extern const char kEnablePortalsName[];
extern const char kEnablePortalsDescription[];

extern const char kEnablePortalsCrossOriginName[];
extern const char kEnablePortalsCrossOriginDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableShortcutCustomizationAppName[];
extern const char kEnableShortcutCustomizationAppDescription[];

extern const char kEnableSRPIsolatedPrerendersName[];
extern const char kEnableSRPIsolatedPrerendersDescription[];

extern const char kEnableSRPIsolatedPrerenderProbingName[];
extern const char kEnableSRPIsolatedPrerenderProbingDescription[];

extern const char kEnableSRPIsolatedPrerendersNSPName[];
extern const char kEnableSRPIsolatedPrerendersNSPDescription[];

extern const char kReduceHorizontalFlingVelocityName[];
extern const char kReduceHorizontalFlingVelocityDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];
extern const char kEnableResamplingScrollEventsExperimentalPredictionName[];
extern const char
    kEnableResamplingScrollEventsExperimentalPredictionDescription[];

extern const char kEnableRestrictedWebApisName[];
extern const char kEnableRestrictedWebApisDescription[];

extern const char kEnableSubresourceRedirectName[];
extern const char kEnableSubresourceRedirectDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kEnableWebAuthenticationCableV2SupportName[];
extern const char kEnableWebAuthenticationCableV2SupportDescription[];

extern const char kEnableWebAuthenticationChromeOSAuthenticatorName[];
extern const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[];

extern const char kExperimentalWebAssemblyFeaturesName[];
extern const char kExperimentalWebAssemblyFeaturesDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmLazyCompilationName[];
extern const char kEnableWasmLazyCompilationDescription[];

extern const char kEnableWasmSimdName[];
extern const char kEnableWasmSimdDescription[];

extern const char kEnableWasmThreadsName[];
extern const char kEnableWasmThreadsDescription[];

extern const char kEnableWasmTieringName[];
extern const char kEnableWasmTieringDescription[];

extern const char kEvDetailsInPageInfoName[];
extern const char kEvDetailsInPageInfoDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

extern const char kExperimentalAccessibilityLabelsName[];
extern const char kExperimentalAccessibilityLabelsDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

extern const char kExtensionsCheckupName[];
extern const char kExtensionsCheckupDescription[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFilteringScrollPredictionName[];
extern const char kFilteringScrollPredictionDescription[];

extern const char kFractionalScrollOffsetsName[];
extern const char kFractionalScrollOffsetsDescription[];

extern const char kFreezeUserAgentName[];
extern const char kFreezeUserAgentDescription[];

extern const char kForceEffectiveConnectionTypeName[];
extern const char kForceEffectiveConnectionTypeDescription[];
extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionTypeSlow2GOnCellularDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

extern const char kFileHandlingAPIName[];
extern const char kFileHandlingAPIDescription[];

extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kFocusMode[];
extern const char kFocusModeDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFormControlsDarkModeName[];
extern const char kFormControlsDarkModeDescription[];

extern const char kFormControlsRefreshName[];
extern const char kFormControlsRefreshDescription[];

extern const char kGlobalMediaControlsName[];
extern const char kGlobalMediaControlsDescription[];

extern const char kGlobalMediaControlsForCastName[];
extern const char kGlobalMediaControlsForCastDescription[];

extern const char kGlobalMediaControlsForChromeOSName[];
extern const char kGlobalMediaControlsForChromeOSDescription[];

extern const char kGlobalMediaControlsPictureInPictureName[];
extern const char kGlobalMediaControlsPictureInPictureDescription[];

extern const char kGlobalMediaControlsSeamlessTransferName[];
extern const char kGlobalMediaControlsSeamlessTransferDescription[];

extern const char kGlobalMediaControlsModernUIName[];
extern const char kGlobalMediaControlsModernUIDescription[];

extern const char kGlobalMediaControlsOverlayControlsName[];
extern const char kGlobalMediaControlsOverlayControlsDescription[];

extern const char kGoogleLensSdkIntentName[];
extern const char kGoogleLensSdkIntentDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];

extern const char kHandwritingGestureEditingName[];
extern const char kHandwritingGestureEditingDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHeavyAdPrivacyMitigationsName[];
extern const char kHeavyAdPrivacyMitigationsDescription[];

extern const char kHeavyAdInterventionName[];
extern const char kHeavyAdInterventionDescription[];

extern const char kTabSwitcherOnReturnName[];
extern const char kTabSwitcherOnReturnDescription[];

extern const char kHideShelfControlsInTabletModeName[];
extern const char kHideShelfControlsInTabletModeDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kIgnoreGpuBlocklistName[];
extern const char kIgnoreGpuBlocklistDescription[];

extern const char kImprovedCookieControlsName[];
extern const char kImprovedCookieControlsDescription[];

extern const char kImprovedCookieControlsForThirdPartyCookieBlockingName[];
extern const char
    kImprovedCookieControlsForThirdPartyCookieBlockingDescription[];

extern const char kImprovedKeyboardShortcutsName[];
extern const char kImprovedKeyboardShortcutsDescription[];

extern const char kCompositorThreadedScrollbarScrollingName[];
extern const char kCompositorThreadedScrollbarScrollingDescription[];

extern const char kImpulseScrollAnimationsName[];
extern const char kImpulseScrollAnimationsDescription[];

extern const char kIncognitoBrandConsistencyForDesktopName[];
extern const char kIncognitoBrandConsistencyForDesktopDescription[];

extern const char kIncognitoScreenshotName[];
extern const char kIncognitoScreenshotDescription[];

extern const char kInheritNativeThemeFromParentWidgetName[];
extern const char kInheritNativeThemeFromParentWidgetDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kInsertKeyToggleModeName[];
extern const char kInsertKeyToggleModeDescription[];

extern const char kInstalledAppsInCbdName[];
extern const char kInstalledAppsInCbdDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kKerberosSettingsSectionName[];
extern const char kKerberosSettingsSectionDescription[];

extern const char kLegacyTLSEnforcedName[];
extern const char kLegacyTLSEnforcedDescription[];

extern const char kLensCameraAssistedSearchName[];
extern const char kLensCameraAssistedSearchDescription[];

extern const char kLiteVideoName[];
extern const char kLiteVideoDescription[];

extern const char kLiteVideoDownlinkBandwidthKbpsName[];
extern const char kLiteVideoDownlinkBandwidthKbpsDescription[];

extern const char kLiteVideoForceOverrideDecisionName[];
extern const char kLiteVideoForceOverrideDecisionDescription[];

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kLogJsConsoleMessagesName[];
extern const char kLogJsConsoleMessagesDescription[];

extern const char kMediaHistoryName[];
extern const char kMediaHistoryDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMediaSessionWebRTCName[];
extern const char kMediaSessionWebRTCDescription[];

extern const char kMemoriesName[];
extern const char kMemoriesDescription[];

extern const char kMemoriesDebugName[];
extern const char kMemoriesDebugDescription[];

extern const char kMetricsSettingsAndroidName[];
extern const char kMetricsSettingsAndroidDescription[];

extern const char kMixedFormsDisableAutofillName[];
extern const char kMixedFormsDisableAutofillDescription[];

extern const char kMixedFormsInterstitialName[];
extern const char kMixedFormsInterstitialDescription[];

extern const char kMobileIdentityConsistencyName[];
extern const char kMobileIdentityConsistencyDescription[];

extern const char kMobileIdentityConsistencyFREName[];
extern const char kMobileIdentityConsistencyFREDescription[];

extern const char kMobileIdentityConsistencyVarName[];
extern const char kMobileIdentityConsistencyVarDescription[];

extern const char kMobilePwaInstallUseBottomSheetName[];
extern const char kMobilePwaInstallUseBottomSheetDescription[];

extern const char kMojoLinuxChannelSharedMemName[];
extern const char kMojoLinuxChannelSharedMemDescription[];

extern const char kMouseSubframeNoImplicitCaptureName[];
extern const char kMouseSubframeNoImplicitCaptureDescription[];

extern const char kUseOfHashAffiliationFetcherName[];
extern const char kUseOfHashAffiliationFetcherDescription[];

extern const char kUsernameFirstFlowName[];
extern const char kUsernameFirstFlowDescription[];

extern const char kNewCanvas2DAPIName[];
extern const char kNewCanvas2DAPIDescription[];

extern const char kNewProfilePickerName[];
extern const char kNewProfilePickerDescription[];

extern const char kSignInProfileCreationName[];
extern const char kSignInProfileCreationDescription[];

extern const char kSignInProfileCreationEnterpriseName[];
extern const char kSignInProfileCreationEnterpriseDescription[];

extern const char kSyncingCompromisedCredentialsName[];
extern const char kSyncingCompromisedCredentialsDescription[];

extern const char kDestroyProfileOnBrowserCloseName[];
extern const char kDestroyProfileOnBrowserCloseDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNewTabstripAnimationName[];
extern const char kNewTabstripAnimationDescription[];

extern const char kNotificationIndicatorName[];
extern const char kNotificationIndicatorDescription[];

extern const char kNotificationSchedulerName[];
extern const char kNotificationSchedulerDescription[];

extern const char kNotificationSchedulerDebugOptionName[];
extern const char kNotificationSchedulerDebugOptionDescription[];
extern const char kNotificationSchedulerImmediateBackgroundTaskDescription[];

extern const char kNotificationsSystemFlagName[];
extern const char kNotificationsSystemFlagDescription[];

extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

extern const char kOmniboxAdaptiveSuggestionsCountName[];
extern const char kOmniboxAdaptiveSuggestionsCountDescription[];

extern const char kOmniboxAssistantVoiceSearchName[];
extern const char kOmniboxAssistantVoiceSearchDescription[];

extern const char kOmniboxBookmarkPathsName[];
extern const char kOmniboxBookmarkPathsDescription[];

extern const char kOmniboxClobberTriggersContextualWebZeroSuggestName[];
extern const char kOmniboxClobberTriggersContextualWebZeroSuggestDescription[];

extern const char kOmniboxCompactSuggestionsName[];
extern const char kOmniboxCompactSuggestionsDescription[];

extern const char kOmniboxDisableCGIParamMatchingName[];
extern const char kOmniboxDisableCGIParamMatchingDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxDefaultTypedNavigationsToHttpsName[];
extern const char kOmniboxDefaultTypedNavigationsToHttpsDescription[];

extern const char kOmniboxKeywordSpaceTriggeringName[];
extern const char kOmniboxKeywordSpaceTriggeringDescription[];

extern const char kOmniboxExperimentalSuggestScoringName[];
extern const char kOmniboxExperimentalSuggestScoringDescription[];

extern const char kOmniboxLocalZeroSuggestFrecencyRankingName[];
extern const char kOmniboxLocalZeroSuggestFrecencyRankingDescription[];

extern const char kOmniboxMostVisitedTilesName[];
extern const char kOmniboxMostVisitedTilesDescription[];

extern const char kOmniboxNativeVoiceSuggestProviderName[];
extern const char kOmniboxNativeVoiceSuggestProviderDescription[];

extern const char kOmniboxRichAutocompletionName[];
extern const char kOmniboxRichAutocompletionDescription[];
extern const char kOmniboxRichAutocompletionMinCharName[];
extern const char kOmniboxRichAutocompletionMinCharDescription[];
extern const char kOmniboxRichAutocompletionShowAdditionalTextName[];
extern const char kOmniboxRichAutocompletionShowAdditionalTextDescription[];
extern const char kOmniboxRichAutocompletionSplitName[];
extern const char kOmniboxRichAutocompletionSplitDescription[];
extern const char kOmniboxRichAutocompletionPreferUrlsOverPrefixesName[];
extern const char kOmniboxRichAutocompletionPreferUrlsOverPrefixesDescription[];
extern const char kOmniboxRichAutocompletionPromisingName[];
extern const char kOmniboxRichAutocompletionPromisingDescription[];

extern const char kOmniboxRichEntitiesInLauncherName[];
extern const char kOmniboxRichEntitiesInLauncherDescription[];

extern const char kOmniboxOnFocusSuggestionsContextualWebName[];
extern const char kOmniboxOnFocusSuggestionsContextualWebDescription[];

extern const char kOmniboxShortBookmarkSuggestionsName[];
extern const char kOmniboxShortBookmarkSuggestionsDescription[];

extern const char kOmniboxSearchEngineLogoName[];
extern const char kOmniboxSearchEngineLogoDescription[];

extern const char kOmniboxSearchReadyIncognitoName[];
extern const char kOmniboxSearchReadyIncognitoDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

extern const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPName[];
extern const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription[];

extern const char kOmniboxUIHideSteadyStateUrlSchemeName[];
extern const char kOmniboxUIHideSteadyStateUrlSchemeDescription[];

extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[];

extern const char kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverName[];
extern const char
    kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverDescription[];

extern const char
    kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionName[];
extern const char
    kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionDescription[];

extern const char kOmniboxUIMaybeElideToRegistrableDomainName[];
extern const char kOmniboxUIMaybeElideToRegistrableDomainDescription[];

extern const char kOmniboxMaxZeroSuggestMatchesName[];
extern const char kOmniboxMaxZeroSuggestMatchesDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxDynamicMaxAutocompleteName[];
extern const char kOmniboxDynamicMaxAutocompleteDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

extern const char kOmniboxUISwapTitleAndUrlName[];
extern const char kOmniboxUISwapTitleAndUrlDescription[];

extern const char kOmniboxWebUIOmniboxPopupName[];
extern const char kOmniboxWebUIOmniboxPopupDescription[];

extern const char kEnableSearchPrefetchName[];
extern const char kEnableSearchPrefetchDescription[];

extern const char kOopRasterizationName[];
extern const char kOopRasterizationDescription[];

extern const char kOopRasterizationDDLName[];
extern const char kOopRasterizationDDLDescription[];

extern const char kOptimizationGuideModelDownloadingName[];
extern const char kOptimizationGuideModelDownloadingDescription[];

extern const char kOsSettingsDeepLinkingName[];
extern const char kOsSettingsDeepLinkingDescription[];

extern const char kEnableDeJellyName[];
extern const char kEnableDeJellyDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kOverlayStrategiesName[];
extern const char kOverlayStrategiesDescription[];
extern const char kOverlayStrategiesDefault[];
extern const char kOverlayStrategiesNone[];
extern const char kOverlayStrategiesUnoccludedFullscreen[];
extern const char kOverlayStrategiesUnoccluded[];
extern const char kOverlayStrategiesOccludedAndUnoccluded[];

extern const char kOverrideLanguagePrefsForHrefTranslateName[];
extern const char kOverrideLanguagePrefsForHrefTranslateDescription[];
extern const char kOverrideSitePrefsForHrefTranslateName[];
extern const char kOverrideSitePrefsForHrefTranslateDescription[];

extern const char kUpdateHoverAtBeginFrameName[];
extern const char kUpdateHoverAtBeginFrameDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kPageInfoV2DesktopName[];
extern const char kPageInfoV2DesktopDescription[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPasswordChangeInSettingsName[];
extern const char kPasswordChangeInSettingsDescription[];

extern const char kPasswordChangeName[];
extern const char kPasswordChangeDescription[];

extern const char kPassiveEventListenerDefaultName[];
extern const char kPassiveEventListenerDefaultDescription[];
extern const char kPassiveEventListenerTrue[];
extern const char kPassiveEventListenerForceAllTrue[];

extern const char kPassiveEventListenersDueToFlingName[];
extern const char kPassiveEventListenersDueToFlingDescription[];

extern const char kPassiveDocumentEventListenersName[];
extern const char kPassiveDocumentEventListenersDescription[];

extern const char kPassiveDocumentWheelEventListenersName[];
extern const char kPassiveDocumentWheelEventListenersDescription[];

extern const char kPasswordImportName[];
extern const char kPasswordImportDescription[];

extern const char kPasswordScriptsFetchingName[];
extern const char kPasswordScriptsFetchingDescription[];

extern const char kPdfXfaFormsName[];
extern const char kPdfXfaFormsDescription[];

extern const char kForceWebContentsDarkModeName[];
extern const char kForceWebContentsDarkModeDescription[];

extern const char kForcedColorsName[];
extern const char kForcedColorsDescription[];

extern const char kPercentBasedScrollingName[];
extern const char kPercentBasedScrollingDescription[];

extern const char kPermissionChipName[];
extern const char kPermissionChipDescription[];

extern const char kPermissionChipGestureSensitiveName[];
extern const char kPermissionChipGestureSensitiveDescription[];

extern const char kPermissionChipRequestTypeSensitiveName[];
extern const char kPermissionChipRequestTypeSensitiveDescription[];

extern const char kPermissionPredictionsName[];
extern const char kPermissionPredictionsDescription[];

extern const char kPointerLockOptionsName[];
extern const char kPointerLockOptionsDescription[];

extern const char kPrerender2Name[];
extern const char kPrerender2Description[];

extern const char kPrintServerScalingName[];
extern const char kPrintServerScalingDescription[];

extern const char kPrivacyAdvisorName[];
extern const char kPrivacyAdvisorDescription[];

extern const char kPrivacySandboxSettingsName[];
extern const char kPrivacySandboxSettingsDescription[];

extern const char kSafetyCheckWeakPasswordsName[];
extern const char kSafetyCheckWeakPasswordsDescription[];

extern const char kProminentDarkModeActiveTabTitleName[];
extern const char kProminentDarkModeActiveTabTitleDescription[];

extern const char kPromoBrowserCommandsName[];
extern const char kPromoBrowserCommandsDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kQuietNotificationPromptsName[];
extern const char kQuietNotificationPromptsDescription[];

extern const char kAbusiveNotificationPermissionRevocationName[];
extern const char kAbusiveNotificationPermissionRevocationDescription[];

extern const char kContentSettingsRedesignName[];
extern const char kContentSettingsRedesignDescription[];

extern const char kRawClipboardName[];
extern const char kRawClipboardDescription[];

extern const char kReadLaterFlagId[];
extern const char kReadLaterName[];
extern const char kReadLaterDescription[];

extern const char kReadLaterNewBadgePromoName[];
extern const char kReadLaterNewBadgePromoDescription[];

extern const char kRecordWebAppDebugInfoName[];
extern const char kRecordWebAppDebugInfoDescription[];

extern const char kRewriteLevelDBOnDeletionName[];
extern const char kRewriteLevelDBOnDeletionDescription[];

extern const char kRestrictGamepadAccessName[];
extern const char kRestrictGamepadAccessDescription[];

extern const char kMBIModeName[];
extern const char kMBIModeDescription[];

extern const char kIntensiveWakeUpThrottlingName[];
extern const char kIntensiveWakeUpThrottlingDescription[];

extern const char kPrinterStatusName[];
extern const char kPrinterStatusDescription[];

extern const char kPrinterStatusDialogName[];
extern const char kPrinterStatusDialogDescription[];

extern const char kSafeBrowsingRealTimeUrlLookupEnterpriseGaEndpointName[];
extern const char
    kSafeBrowsingRealTimeUrlLookupEnterpriseGaEndpointDescription[];

extern const char kSafetyTipName[];
extern const char kSafetyTipDescription[];

extern const char kSchemefulSameSiteName[];
extern const char kSchemefulSameSiteDescription[];

extern const char kScreenCaptureTestName[];
extern const char kScreenCaptureTestDescription[];

extern const char kScrollableTabStripFlagId[];
extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kScrollableTabStripButtonsName[];
extern const char kScrollableTabStripButtonsDescription[];

extern const char kScrollUnificationName[];
extern const char kScrollUnificationDescription[];

extern const char kSearchHistoryLinkName[];
extern const char kSearchHistoryLinkDescription[];

extern const char kSecurePaymentConfirmationDebugName[];
extern const char kSecurePaymentConfirmationDebugDescription[];

extern const char kSendTabToSelfWhenSignedInName[];
extern const char kSendTabToSelfWhenSignedInDescription[];

extern const char kSidePanelName[];
extern const char kSidePanelDescription[];

extern const char kSidePanelPrototypeName[];
extern const char kSidePanelPrototypeDescription[];

extern const char kSharedClipboardUIName[];
extern const char kSharedClipboardUIDescription[];

extern const char kSharingHubDesktopAppMenuName[];
extern const char kSharingHubDesktopAppMenuDescription[];

extern const char kSharingHubDesktopOmniboxName[];
extern const char kSharingHubDesktopOmniboxDescription[];

extern const char kSharingPeerConnectionReceiverName[];
extern const char kSharingPeerConnectionReceiverDescription[];

extern const char kSharingPeerConnectionSenderName[];
extern const char kSharingPeerConnectionSenderDescription[];

extern const char kSharingPreferVapidName[];
extern const char kSharingPreferVapidDescription[];

extern const char kSharingQRCodeGeneratorName[];
extern const char kSharingQRCodeGeneratorDescription[];

extern const char kSharingSendViaSyncName[];
extern const char kSharingSendViaSyncDescription[];

extern const char kSharingDeviceExpirationName[];
extern const char kSharingDeviceExpirationDescription[];

extern const char kShelfHoverPreviewsName[];
extern const char kShelfHoverPreviewsDescription[];

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

extern const char kHistoryManipulationIntervention[];
extern const char kHistoryManipulationInterventionDescription[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

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

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kWebOTPCrossDeviceName[];
extern const char kWebOTPCrossDeviceDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kSplitCacheByNetworkIsolationKeyName[];
extern const char kSplitCacheByNetworkIsolationKeyDescription[];

extern const char kStopInBackgroundName[];
extern const char kStopInBackgroundDescription[];

extern const char kStoragePressureEventName[];
extern const char kStoragePressureEventDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kStylusBatteryStatusName[];
extern const char kStylusBatteryStatusDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncAutofillWalletOfferDataName[];
extern const char kSyncAutofillWalletOfferDataDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kTabEngagementReportingName[];
extern const char kTabEngagementReportingDescription[];

extern const char kTabGridLayoutAndroidName[];
extern const char kTabGridLayoutAndroidDescription[];

extern const char kCommerceMerchantViewerAndroidName[];
extern const char kCommerceMerchantViewerAndroidDescription[];

extern const char kTabGroupsAndroidName[];
extern const char kTabGroupsAndroidDescription[];

extern const char kTabGroupsContinuationAndroidName[];
extern const char kTabGroupsContinuationAndroidDescription[];

extern const char kTabGroupsUiImprovementsAndroidName[];
extern const char kTabGroupsUiImprovementsAndroidDescription[];

extern const char kTabToGTSAnimationAndroidName[];
extern const char kTabToGTSAnimationAndroidDescription[];

extern const char kTabGroupsAutoCreateName[];
extern const char kTabGroupsAutoCreateDescription[];

extern const char kTabGroupsCollapseName[];
extern const char kTabGroupsCollapseDescription[];

extern const char kTabGroupsCollapseFreezingName[];
extern const char kTabGroupsCollapseFreezingDescription[];

extern const char kTabGroupsFeedbackName[];
extern const char kTabGroupsFeedbackDescription[];

extern const char kTabGroupsNewBadgePromoName[];
extern const char kTabGroupsNewBadgePromoDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabOutlinesInLowContrastThemesName[];
extern const char kTabOutlinesInLowContrastThemesDescription[];

extern const char kTextFragmentColorChangeName[];
extern const char kTextFragmentColorChangeDescription[];

extern const char kTFLiteLanguageDetectionName[];
extern const char kTFLiteLanguageDetectionDescription[];

extern const char kTintCompositedContentName[];
extern const char kTintCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kTouchpadOverscrollHistoryNavigationName[];
extern const char kTouchpadOverscrollHistoryNavigationDescription[];

extern const char kTraceUploadUrlName[];
extern const char kTraceUploadUrlDescription[];
extern const char kTraceUploadUrlChoiceOther[];
extern const char kTraceUploadUrlChoiceEmloading[];
extern const char kTraceUploadUrlChoiceQa[];
extern const char kTraceUploadUrlChoiceTesting[];

extern const char kTranslateForceTriggerOnEnglishName[];
extern const char kTranslateForceTriggerOnEnglishDescription[];

extern const char kTranslateBubbleUIName[];
extern const char kTranslateBubbleUIDescription[];

extern const char kTreatInsecureOriginAsSecureName[];
extern const char kTreatInsecureOriginAsSecureDescription[];

extern const char kTreatUnsafeDownloadsAsActiveName[];
extern const char kTreatUnsafeDownloadsAsActiveDescription[];

extern const char kTrustTokensName[];
extern const char kTrustTokensDescription[];

extern const char kTurnOffStreamingMediaCachingOnBatteryName[];
extern const char kTurnOffStreamingMediaCachingOnBatteryDescription[];

extern const char kTurnOffStreamingMediaCachingAlwaysName[];
extern const char kTurnOffStreamingMediaCachingAlwaysDescription[];

extern const char kUnsafeWebGPUName[];
extern const char kUnsafeWebGPUDescription[];

extern const char kUnsafeFastJSCallsName[];
extern const char kUnsafeFastJSCallsDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUseFirstPartySetName[];
extern const char kUseFirstPartySetDescription[];

extern const char kUseSearchClickForRightClickName[];
extern const char kUseSearchClickForRightClickDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWallpaperWebUIName[];
extern const char kWallpaperWebUIDescription[];

extern const char kWebBluetoothNewPermissionsBackendName[];
extern const char kWebBluetoothNewPermissionsBackendDescription[];

extern const char kWebBundlesName[];
extern const char kWebBundlesDescription[];

extern const char kWebIdName[];
extern const char kWebIdDescription[];

extern const char kWebOtpBackendName[];
extern const char kWebOtpBackendDescription[];
extern const char kWebOtpBackendSmsVerification[];
extern const char kWebOtpBackendUserConsent[];
extern const char kWebOtpBackendAuto[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebPaymentsExperimentalFeaturesName[];
extern const char kWebPaymentsExperimentalFeaturesDescription[];

extern const char kWebPaymentsMinimalUIName[];
extern const char kWebPaymentsMinimalUIDescription[];

extern const char kAppStoreBillingDebugName[];
extern const char kAppStoreBillingDebugDescription[];

extern const char kWebrtcCaptureMultiChannelApmName[];
extern const char kWebrtcCaptureMultiChannelApmDescription[];

extern const char kWebrtcHideLocalIpsWithMdnsName[];
extern const char kWebrtcHideLocalIpsWithMdnsDecription[];

extern const char kWebrtcHybridAgcName[];
extern const char kWebrtcHybridAgcDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];

extern const char kWebrtcUseMinMaxVEADimensionsName[];
extern const char kWebrtcUseMinMaxVEADimensionsDescription[];

extern const char kWebXrForceRuntimeName[];
extern const char kWebXrForceRuntimeDescription[];

extern const char kWebXrRuntimeChoiceNone[];
extern const char kWebXrRuntimeChoiceOpenXR[];

extern const char kWebXrIncubationsName[];
extern const char kWebXrIncubationsDescription[];

extern const char kWindowsFollowCursorName[];
extern const char kWindowsFollowCursorDescription[];

extern const char kWindowNamingName[];
extern const char kWindowNamingDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

extern const char kSharedHighlightingUseBlocklistName[];
extern const char kSharedHighlightingUseBlocklistDescription[];

extern const char kSharedHighlightingV2Name[];
extern const char kSharedHighlightingV2Description[];

extern const char kPreemptiveLinkToTextGenerationName[];
extern const char kPreemptiveLinkToTextGenerationDescription[];

extern const char kDraw1PredictedPoint12Ms[];
extern const char kDraw2PredictedPoints6Ms[];
extern const char kDraw1PredictedPoint6Ms[];
extern const char kDraw2PredictedPoints3Ms[];
extern const char kDrawPredictedPointsDefault[];
extern const char kDrawPredictedPointsDescription[];
extern const char kDrawPredictedPointsName[];

extern const char kSanitizerApiName[];
extern const char kSanitizerApiDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAddToHomescreenIPHName[];
extern const char kAddToHomescreenIPHDescription[];

extern const char kAImageReaderName[];
extern const char kAImageReaderDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidDetailedLanguageSettingsName[];
extern const char kAndroidDetailedLanguageSettingsDescription[];

extern const char kAndroidLayoutChangeTabReparentingName[];
extern const char kAndroidLayoutChangeTabReparentingDescription[];

extern const char kAndroidManagedByMenuItemName[];
extern const char kAndroidManagedByMenuItemDescription[];

extern const char kAndroidPartnerCustomizationPhenotypeName[];
extern const char kAndroidPartnerCustomizationPhenotypeDescription[];

extern const char kAndroidSurfaceControlName[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAppNotificationStatusMessagingName[];
extern const char kAppNotificationStatusMessagingDescription[];

extern const char kAssistantIntentPageUrlName[];
extern const char kAssistantIntentPageUrlDescription[];

extern const char kAssistantIntentTranslateInfoName[];
extern const char kAssistantIntentTranslateInfoDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kAutofillAssistantDirectActionsName[];
extern const char kAutofillAssistantDirectActionsDescription[];

extern const char kAutofillAssistantProactiveHelpName[];
extern const char kAutofillAssistantProactiveHelpDescription[];

extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

extern const char kAppMenuMobileSiteOptionName[];
extern const char kAppMenuMobileSiteOptionDescription[];

extern const char kBackgroundTaskComponentUpdateName[];
extern const char kBackgroundTaskComponentUpdateDescription[];

extern const char kBentoOfflineName[];
extern const char kBentoOfflineDescription[];

extern const char kBookmarkBottomSheetName[];
extern const char kBookmarkBottomSheetDescription[];

extern const char kCCTIncognitoName[];
extern const char kCCTIncognitoDescription[];

extern const char kCCTIncognitoAvailableToThirdPartyName[];
extern const char kCCTIncognitoAvailableToThirdPartyDescription[];

extern const char kCCTTargetTranslateLanguageName[];
extern const char kCCTTargetTranslateLanguageDescription[];

extern const char kChimeAlwaysShowNotificationDescription[];
extern const char kChimeAlwaysShowNotificationName[];

extern const char kChimeAndroidSdkDescription[];
extern const char kChimeAndroidSdkName[];

extern const char kContinuousSearchName[];
extern const char kContinuousSearchDescription[];

extern const char kChromeShareHighlightsAndroidName[];
extern const char kChromeShareHighlightsAndroidDescription[];

extern const char kChromeShareLongScreenshotName[];
extern const char kChromeShareLongScreenshotDescription[];

extern const char kChromeShareQRCodeName[];
extern const char kChromeShareQRCodeDescription[];

extern const char kChromeShareScreenshotName[];
extern const char kChromeShareScreenshotDescription[];

extern const char kChromeSharingHubName[];
extern const char kChromeSharingHubDescription[];

extern const char kChromeSharingHubV15Name[];
extern const char kChromeSharingHubV15Description[];

extern const char kClipboardSuggestionContentHiddenName[];
extern const char kClipboardSuggestionContentHiddenDescription[];

extern const char kClearOldBrowsingDataName[];
extern const char kClearOldBrowsingDataDescription[];

extern const char kCloseTabSuggestionsName[];
extern const char kCloseTabSuggestionsDescription[];

extern const char kCriticalPersistedTabDataName[];
extern const char kCriticalPersistedTabDataDescription[];

extern const char kContextMenuPerformanceInfoAndRemoteHintFetchingName[];
extern const char kContextMenuPerformanceInfoAndRemoteHintFetchingDescription[];

extern const char kContextualSearchDebugName[];
extern const char kContextualSearchDebugDescription[];

extern const char kContextualSearchForceCaptionName[];
extern const char kContextualSearchForceCaptionDescription[];

extern const char kContextualSearchLiteralSearchTapName[];
extern const char kContextualSearchLiteralSearchTapDescription[];

extern const char kContextualSearchLongpressResolveName[];
extern const char kContextualSearchLongpressResolveDescription[];

extern const char kContextualSearchMlTapSuppressionName[];
extern const char kContextualSearchMlTapSuppressionDescription[];

extern const char kContextualSearchRankerQueryName[];
extern const char kContextualSearchRankerQueryDescription[];

extern const char kContextualSearchSecondTapName[];
extern const char kContextualSearchSecondTapDescription[];

extern const char kContextualSearchThinWebViewImplementationName[];
extern const char kContextualSearchThinWebViewImplementationDescription[];

extern const char kContextualSearchTranslationsName[];
extern const char kContextualSearchTranslationsDescription[];

extern const char kCpuAffinityRestrictToLittleCoresName[];
extern const char kCpuAffinityRestrictToLittleCoresDescription[];

extern const char kDecoupleSyncFromAndroidAutoSyncName[];
extern const char kDecoupleSyncFromAndroidAutoSyncDescription[];

extern const char kDirectActionsName[];
extern const char kDirectActionsDescription[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAutofillRefreshStyleName[];
extern const char kEnableAutofillRefreshStyleDescription[];

extern const char kEnableAndroidSpellcheckerDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnableUseAaudioDriverName[];
extern const char kEnableUseAaudioDriverDescription[];

extern const char kEphemeralTabUsingBottomSheetName[];
extern const char kEphemeralTabUsingBottomSheetDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kFillingPasswordsFromAnyOriginName[];
extern const char kFillingPasswordsFromAnyOriginDescription[];

extern const char kHomepagePromoCardName[];
extern const char kHomepagePromoCardDescription[];

extern const char kInstantStartName[];
extern const char kInstantStartDescription[];

extern const char kIntentBlockExternalFormRedirectsNoGestureName[];
extern const char kIntentBlockExternalFormRedirectsNoGestureDescription[];

extern const char kInterestFeedContentSuggestionsName[];
extern const char kInterestFeedContentSuggestionsDescription[];

extern const char kInterestFeedNoticeCardAutoDismissName[];
extern const char kInterestFeedNoticeCardAutoDismissDescription[];

extern const char kInterestFeedV2Name[];
extern const char kInterestFeedV2Description[];

extern const char kInterestFeedV2HeartsName[];
extern const char kInterestFeedV2HeartsDescription[];

extern const char kInterestFeedV2AutoplayName[];
extern const char kInterestFeedV2AutoplayDescription[];

extern const char kFeedShareName[];
extern const char kFeedShareDescription[];

extern const char kInterestFeedV1ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV1ClickAndViewActionsConditionalUploadDescription[];

extern const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[];

extern const char kMessagesForAndroidInfrastructureName[];
extern const char kMessagesForAndroidInfrastructureDescription[];

extern const char kMessagesForAndroidPasswordsName[];
extern const char kMessagesForAndroidPasswordsDescription[];

extern const char kMessagesForAndroidPopupBlockedName[];
extern const char kMessagesForAndroidPopupBlockedDescription[];

extern const char kOfflineIndicatorAlwaysHttpProbeName[];
extern const char kOfflineIndicatorAlwaysHttpProbeDescription[];

extern const char kOfflineIndicatorChoiceName[];
extern const char kOfflineIndicatorChoiceDescription[];

extern const char kOfflineIndicatorV2Name[];
extern const char kOfflineIndicatorV2Description[];

extern const char kOfflinePagesCtName[];
extern const char kOfflinePagesCtDescription[];

extern const char kOfflinePagesCtV2Name[];
extern const char kOfflinePagesCtV2Description[];

extern const char kOfflinePagesCTSuppressNotificationsName[];
extern const char kOfflinePagesCTSuppressNotificationsDescription[];

extern const char kOfflinePagesDescriptiveFailStatusName[];
extern const char kOfflinePagesDescriptiveFailStatusDescription[];

extern const char kOfflinePagesDescriptivePendingStatusName[];
extern const char kOfflinePagesDescriptivePendingStatusDescription[];

extern const char kOfflinePagesInDownloadHomeOpenInCctName[];
extern const char kOfflinePagesInDownloadHomeOpenInCctDescription[];

extern const char kOfflinePagesPrefetchingName[];
extern const char kOfflinePagesPrefetchingDescription[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kOfflinePagesShowAlternateDinoPageName[];
extern const char kOfflinePagesShowAlternateDinoPageDescription[];

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kPageInfoDiscoverabilityName[];
extern const char kPageInfoDiscoverabilityDescription[];

extern const char kPageInfoHistoryName[];
extern const char kPageInfoHistoryDescription[];

extern const char kPageInfoPerformanceHintsName[];
extern const char kPageInfoPerformanceHintsDescription[];

extern const char kPageInfoV2Name[];
extern const char kPageInfoV2Description[];

extern const char kPhotoPickerVideoSupportName[];
extern const char kPhotoPickerVideoSupportDescription[];

extern const char kProcessSharingWithDefaultSiteInstancesName[];
extern const char kProcessSharingWithDefaultSiteInstancesDescription[];

extern const char kProcessSharingWithStrictSiteInstancesName[];
extern const char kProcessSharingWithStrictSiteInstancesDescription[];

extern const char kQueryTilesName[];
extern const char kQueryTilesDescription[];
extern const char kQueryTilesNTPName[];
extern const char kQueryTilesNTPDescription[];
extern const char kQueryTilesOmniboxName[];
extern const char kQueryTilesOmniboxDescription[];
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
extern const char kQueryTilesLocalOrderingName[];
extern const char kQueryTilesLocalOrderingDescription[];
extern const char kQueryTilesInstantFetchName[];
extern const char kQueryTilesInstantFetchDescription[];
extern const char kQueryTilesMoreTrendingName[];
extern const char kQueryTilesMoreTrendingDescription[];
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

extern const char kReadLaterReminderNotificationName[];
extern const char kReadLaterReminderNotificationDescription[];

extern const char kRecoverFromNeverSaveAndroidName[];
extern const char kRecoverFromNeverSaveAndroidDescription[];

extern const char kReengagementNotificationName[];
extern const char kReengagementNotificationDescription[];

extern const char kRelatedSearchesName[];
extern const char kRelatedSearchesDescription[];

extern const char kRelatedSearchesUiName[];
extern const char kRelatedSearchesUiDescription[];

extern const char kRequestDesktopSiteForTabletsName[];
extern const char kRequestDesktopSiteForTabletsDescription[];

extern const char kSafeBrowsingClientSideDetectionAndroidName[];
extern const char kSafeBrowsingClientSideDetectionAndroidDescription[];

extern const char kEnhancedProtectionPromoAndroidName[];
extern const char kEnhancedProtectionPromoAndroidDescription[];

extern const char kSafeBrowsingUseLocalBlacklistsV2Name[];
extern const char kSafeBrowsingUseLocalBlacklistsV2Description[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kSmartSuggestionForLargeDownloadsName[];
extern const char kSmartSuggestionForLargeDownloadsDescription[];

extern const char kStartSurfaceAndroidName[];
extern const char kStartSurfaceAndroidDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kActionableContentSettingsName[];
extern const char kActionableContentSettingsDescription[];

extern const char kThemeRefactorAndroidName[];
extern const char kThemeRefactorAndroidDescription[];

extern const char kToolbarIphAndroidName[];
extern const char kToolbarIphAndroidDescription[];

extern const char kToolbarMicIphAndroidName[];
extern const char kToolbarMicIphAndroidDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuTypeName[];
extern const char kUpdateMenuTypeDescription[];
extern const char kUpdateMenuTypeNone[];
extern const char kUpdateMenuTypeUpdateAvailable[];
extern const char kUpdateMenuTypeUnsupportedOSVersion[];
extern const char kUpdateMenuTypeInlineUpdateSuccess[];
extern const char kUpdateMenuTypeInlineUpdateDialogCanceled[];
extern const char kUpdateMenuTypeInlineUpdateDialogFailed[];
extern const char kUpdateMenuTypeInlineUpdateDownloadFailed[];
extern const char kUpdateMenuTypeInlineUpdateDownloadCanceled[];
extern const char kUpdateMenuTypeInlineUpdateInstallFailed[];

extern const char kUseNotificationCompatBuilderName[];
extern const char kUseNotificationCompatBuilderDescription[];

extern const char kUserMediaScreenCapturingName[];
extern const char kUserMediaScreenCapturingDescription[];

extern const char kVideoTutorialsName[];
extern const char kVideoTutorialsDescription[];
extern const char kVideoTutorialsInstantFetchName[];
extern const char kVideoTutorialsInstantFetchDescription[];

extern const char kAdaptiveButtonInTopToolbarName[];
extern const char kAdaptiveButtonInTopToolbarDescription[];
extern const char kShareButtonInTopToolbarName[];
extern const char kShareButtonInTopToolbarDescription[];
extern const char kVoiceButtonInTopToolbarName[];
extern const char kVoiceButtonInTopToolbarDescription[];

extern const char kPrefetchNotificationSchedulingIntegrationName[];
extern const char kPrefetchNotificationSchedulingIntegrationDescription[];

extern const char kInlineUpdateFlowName[];
extern const char kInlineUpdateFlowDescription[];

extern const char kAndroidDarkSearchName[];
extern const char kAndroidDarkSearchDescription[];

extern const char kSwipeToMoveCursorName[];
extern const char kSwipeToMoveCursorDescription[];

extern const char kWalletRequiresFirstSyncSetupCompleteName[];
extern const char kWalletRequiresFirstSyncSetupCompleteDescription[];

extern const char kWebFeedName[];
extern const char kWebFeedDescription[];

extern const char kXsurfaceMetricsReportingName[];
extern const char kXsurfaceMetricsReportingDescription[];

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAllowAllSitesToInitiateMirroringName[];
extern const char kAllowAllSitesToInitiateMirroringDescription[];

extern const char kEnableAccessibilityLiveCaptionName[];
extern const char kEnableAccessibilityLiveCaptionDescription[];

extern const char kEnableAccessibilityLiveCaptionSodaName[];
extern const char kEnableAccessibilityLiveCaptionSodaDescription[];

extern const char kCastMediaRouteProviderName[];
extern const char kCastMediaRouteProviderDescription[];

extern const char kCopyLinkToTextName[];
extern const char kCopyLinkToTextDescription[];

extern const char kEnterpriseRealtimeExtensionRequestName[];
extern const char kEnterpriseRealtimeExtensionRequestDescription[];

extern const char kGlobalMediaControlsCastStartStopName[];
extern const char kGlobalMediaControlsCastStartStopDescription[];

extern const char kNtpCacheOneGoogleBarName[];
extern const char kNtpCacheOneGoogleBarDescription[];

extern const char kNtpIframeOneGoogleBarName[];
extern const char kNtpIframeOneGoogleBarDescription[];

extern const char kNtpOneGoogleBarModalOverlaysName[];
extern const char kNtpOneGoogleBarModalOverlaysDescription[];

extern const char kNtpRepeatableQueriesName[];
extern const char kNtpRepeatableQueriesDescription[];

extern const char kNtpModulesName[];
extern const char kNtpModulesDescription[];

extern const char kNtpDriveModuleName[];
extern const char kNtpDriveModuleDescription[];

extern const char kNtpRecipeTasksModuleName[];
extern const char kNtpRecipeTasksModuleDescription[];

extern const char kNtpShoppingTasksModuleName[];
extern const char kNtpShoppingTasksModuleDescription[];

extern const char kNtpChromeCartModuleName[];
extern const char kNtpChromeCartModuleDescription[];

extern const char kEnableReaderModeName[];
extern const char kEnableReaderModeDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kHappinessTrackingSurveysForDesktopPrivacySandboxName[];
extern const char
    kHappinessTrackingSurveysForDesktopPrivacySandboxDescription[];

extern const char kHappinessTrackingSurveysForDesktopSettingsName[];
extern const char kHappinessTrackingSurveysForDesktopSettingsDescription[];

extern const char kHappinessTrackingSurveysForDesktopSettingsPrivacyName[];
extern const char
    kHappinessTrackingSurveysForDesktopSettingsPrivacyDescription[];

extern const char kKernelnextVMsName[];
extern const char kKernelnextVMsDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescriptions[];

extern const char kOmniboxExperimentalKeywordModeName[];
extern const char kOmniboxExperimentalKeywordModeDescription[];

extern const char kOmniboxSuggestionButtonRowName[];
extern const char kOmniboxSuggestionButtonRowDescription[];

extern const char kOmniboxPedalSuggestionsName[];
extern const char kOmniboxPedalSuggestionsDescription[];

extern const char kOmniboxPedalsBatch2Name[];
extern const char kOmniboxPedalsBatch2Description[];

extern const char kOmniboxPedalsDefaultIconColoredName[];
extern const char kOmniboxPedalsDefaultIconColoredDescription[];

extern const char kOmniboxKeywordSearchButtonName[];
extern const char kOmniboxKeywordSearchButtonDescription[];

extern const char kOmniboxRefinedFocusStateName[];
extern const char kOmniboxRefinedFocusStateDescription[];

extern const char kSCTAuditingName[];
extern const char kSCTAuditingDescription[];

extern const char kShutdownSupportForKeepaliveName[];
extern const char kShutdownSupportForKeepaliveDescription[];

extern const char kTabFreezeName[];
extern const char kTabFreezeDescription[];

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kChromeCleanupScanCompletedNotificationName[];
extern const char kChromeCleanupScanCompletedNotificationDescription[];

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

extern const char kD3D11VideoDecoderName[];
extern const char kD3D11VideoDecoderDescription[];

extern const char kElasticOverscrollWinName[];
extern const char kElasticOverscrollWinDescription[];

extern const char kEnableIncognitoShortcutOnDesktopName[];
extern const char kEnableIncognitoShortcutOnDesktopDescription[];

extern const char kEnableMediaFoundationVideoCaptureName[];
extern const char kEnableMediaFoundationVideoCaptureDescription[];

extern const char kRawAudioCaptureName[];
extern const char kRawAudioCaptureDescription[];

extern const char kRunVideoCaptureServiceInBrowserProcessName[];
extern const char kRunVideoCaptureServiceInBrowserProcessDescription[];

extern const char kSafetyCheckChromeCleanerChildName[];
extern const char kSafetyCheckChromeCleanerChildDescription[];

extern const char kUseAngleName[];
extern const char kUseAngleDescription[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];
extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];
extern const char kUseAngleD3D11on12[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

extern const char kPrintWithReducedRasterizationName[];
extern const char kPrintWithReducedRasterizationDescription[];

extern const char kUseXpsForPrintingName[];
extern const char kUseXpsForPrintingDescription[];

extern const char kUseXpsForPrintingFromPdfName[];
extern const char kUseXpsForPrintingFromPdfDescription[];
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_SPELLCHECK)
extern const char kWinUseBrowserSpellCheckerName[];
extern const char kWinUseBrowserSpellCheckerDescription[];

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#endif  // defined(OS_WIN)

// Mac ------------------------------------------------------------------------

#if defined(OS_MAC)

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kCupsIppPrintingBackendName[];
extern const char kCupsIppPrintingBackendDescription[];
#endif  // BUILDFLAG(ENABLE_PRINTING)

extern const char kEnterpriseReportingApiKeychainRecreationName[];
extern const char kEnterpriseReportingApiKeychainRecreationDescription[];

extern const char kImmersiveFullscreenName[];
extern const char kImmersiveFullscreenDescription[];

extern const char kMacSyscallSandboxName[];
extern const char kMacSyscallSandboxDescription[];

extern const char kMetalName[];
extern const char kMetalDescription[];

extern const char kScreenTimeName[];
extern const char kScreenTimeDescription[];

#endif  // defined(OS_MAC)

// Chrome OS ------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kAccountManagementFlowsV2Name[];
extern const char kAccountManagementFlowsV2Description[];

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAllowDisableMouseAccelerationName[];
extern const char kAllowDisableMouseAccelerationDescription[];

extern const char kAllowRepeatedUpdatesName[];
extern const char kAllowRepeatedUpdatesDescription[];

extern const char kAllowScrollSettingsName[];
extern const char kAllowScrollSettingsDescription[];

extern const char kAppServiceAdaptiveIconName[];
extern const char kAppServiceAdaptiveIconDescription[];

extern const char kAppServiceExternalProtocolName[];
extern const char kAppServiceExternalProtocolDescription[];

extern const char kArcCustomTabsExperimentName[];
extern const char kArcCustomTabsExperimentDescription[];

extern const char kArcDocumentsProviderUnknownSizeName[];
extern const char kArcDocumentsProviderUnknownSizeDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcNativeBridge64BitSupportExperimentName[];
extern const char kArcNativeBridge64BitSupportExperimentDescription[];

extern const char kArcRtVcpuDualCoreName[];
extern const char kArcRtVcpuDualCoreDesc[];

extern const char kArcRtVcpuQuadCoreName[];
extern const char kArcRtVcpuQuadCoreDesc[];

extern const char kArcUseHighMemoryDalvikProfileName[];
extern const char kArcUseHighMemoryDalvikProfileDesc[];

extern const char kArcEnableUsapName[];
extern const char kArcEnableUsapDesc[];

extern const char kArcUsbHostName[];
extern const char kArcUsbHostDescription[];

extern const char kAshEnablePipRoundedCornersName[];
extern const char kAshEnablePipRoundedCornersDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAshSwapSideVolumeButtonsForOrientationName[];
extern const char kAshSwapSideVolumeButtonsForOrientationDescription[];

extern const char kAshSwipingFromLeftEdgeToGoBackName[];
extern const char kAshSwipingFromLeftEdgeToGoBackDescription[];

extern const char kBluetoothAggressiveAppearanceFilterName[];
extern const char kBluetoothAggressiveAppearanceFilterDescription[];

extern const char kBluetoothFixA2dpPacketSizeName[];
extern const char kBluetoothFixA2dpPacketSizeDescription[];

extern const char kBluetoothWbsDogfoodName[];
extern const char kBluetoothWbsDogfoodDescription[];

extern const char kPreferConstantFrameRateName[];
extern const char kPreferConstantFrameRateDescription[];

extern const char kCdmFactoryDaemonName[];
extern const char kCdmFactoryDaemonDescription[];

extern const char kCellularUseAttachApnName[];
extern const char kCellularUseAttachApnDescription[];

extern const char kCellularUseExternalEuiccName[];
extern const char kCellularUseExternalEuiccDescription[];

extern const char kContextualNudgesName[];
extern const char kContextualNudgesDescription[];

extern const char kCroshSWAName[];
extern const char kCroshSWADescription[];

extern const char kCrosLanguageSettingsUpdate2Name[];
extern const char kCrosLanguageSettingsUpdate2Description[];

extern const char kCrosOnDeviceGrammarCheckName[];
extern const char kCrosOnDeviceGrammarCheckDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kCrostiniDiskResizingName[];
extern const char kCrostiniDiskResizingDescription[];

extern const char kCrostiniUseBusterImageName[];
extern const char kCrostiniUseBusterImageDescription[];

extern const char kCrostiniGpuSupportName[];
extern const char kCrostiniGpuSupportDescription[];

extern const char kCrostiniUseDlcName[];
extern const char kCrostiniUseDlcDescription[];

extern const char kCrostiniEnableDlcName[];
extern const char kCrostiniEnableDlcDescription[];

extern const char kCrostiniResetLxdDbName[];
extern const char kCrostiniResetLxdDbDescription[];

extern const char kCryptAuthV2DeviceActivityStatusName[];
extern const char kCryptAuthV2DeviceActivityStatusDescription[];

extern const char kCryptAuthV2DeviceActivityStatusUseConnectivityName[];
extern const char kCryptAuthV2DeviceActivityStatusUseConnectivityDescription[];

extern const char kCryptAuthV2DeviceSyncName[];
extern const char kCryptAuthV2DeviceSyncDescription[];

extern const char kCryptAuthV2EnrollmentName[];
extern const char kCryptAuthV2EnrollmentDescription[];

extern const char kDisableBufferBWCompressionName[];
extern const char kDisableBufferBWCompressionDescription[];

extern const char kDisableCameraFrameRotationAtSourceName[];
extern const char kDisableCameraFrameRotationAtSourceDescription[];

extern const char kForceSpectreVariant2MitigationName[];
extern const char kForceSpectreVariant2MitigationDescription[];

extern const char kDisableCancelAllTouchesName[];
extern const char kDisableCancelAllTouchesDescription[];

extern const char kDisableCryptAuthV1DeviceSyncName[];
extern const char kDisableCryptAuthV1DeviceSyncDescription[];

extern const char kDisableIdleSocketsCloseOnMemoryPressureName[];
extern const char kDisableIdleSocketsCloseOnMemoryPressureDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisplayAlignmentAssistanceName[];
extern const char kDisplayAlignmentAssistanceDescription[];

extern const char kDisplayIdentificationName[];
extern const char kDisplayIdentificationDescription[];

extern const char kUseHDRTransferFunctionName[];
extern const char kUseHDRTransferFunctionDescription[];

extern const char kDisableOfficeEditingComponentAppName[];
extern const char kDisableOfficeEditingComponentAppDescription[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kDriveFsBidirectionalNativeMessagingName[];
extern const char kDriveFsBidirectionalNativeMessagingDescription[];

extern const char kEnableAppDataSearchName[];
extern const char kEnableAppDataSearchDescription[];

extern const char kEnableAppGridGhostName[];
extern const char kEnableAppGridGhostDescription[];

extern const char kEnableAppListSearchAutocompleteName[];
extern const char kEnableAppListSearchAutocompleteDescription[];

extern const char kEnableAppReinstallZeroStateName[];
extern const char kEnableAppReinstallZeroStateDescription[];

extern const char kEnableArcUnifiedAudioFocusName[];
extern const char kEnableArcUnifiedAudioFocusDescription[];

extern const char kEnableAssistantAppSupportName[];
extern const char kEnableAssistantAppSupportDescription[];

extern const char kEnableAssistantBetterOnboardingName[];
extern const char kEnableAssistantBetterOnboardingDescription[];

extern const char kEnableAssistantLauncherIntegrationName[];
extern const char kEnableAssistantLauncherIntegrationDescription[];

extern const char kEnableAssistantLauncherUIName[];
extern const char kEnableAssistantLauncherUIDescription[];

extern const char kEnableAssistantMediaSessionIntegrationName[];
extern const char kEnableAssistantMediaSessionIntegrationDescription[];

extern const char kEnableAssistantRoutinesName[];
extern const char kEnableAssistantRoutinesDescription[];

extern const char kEnableAutoSelectName[];
extern const char kEnableAutoSelectDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnhancedClipboardName[];
extern const char kEnhancedClipboardDescription[];

extern const char kEnhancedClipboardNudgeSessionResetName[];
extern const char kEnhancedClipboardNudgeSessionResetDescription[];

extern const char kEnableCrOSActionRecorderName[];
extern const char kEnableCrOSActionRecorderDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

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

extern const char kEnableHeuristicStylusPalmRejectionName[];
extern const char kEnableHeuristicStylusPalmRejectionDescription[];

extern const char kEnableLauncherSearchNormalizationName[];
extern const char kEnableLauncherSearchNormalizationDescription[];

extern const char kNewDragSpecInLauncherName[];
extern const char kNewDragSpecInLauncherDescription[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableNewShortcutMappingName[];
extern const char kEnableNewShortcutMappingDescription[];

extern const char kEnablePalmOnMaxTouchMajorName[];
extern const char kEnablePalmOnMaxTouchMajorDescription[];

extern const char kEnablePalmOnToolTypePalmName[];
extern const char kEnablePalmOnToolTypePalmDescription[];

extern const char kEnablePalmSuppressionName[];
extern const char kEnablePalmSuppressionDescription[];

extern const char kEnablePlayStoreSearchName[];
extern const char kEnablePlayStoreSearchDescription[];

extern const char kEnableQuickAnswersName[];
extern const char kEnableQuickAnswersDescription[];

extern const char kEnableQuickAnswersOnEditableTextName[];
extern const char kEnableQuickAnswersOnEditableTextDescription[];

extern const char kEnableQuickAnswersRichUiName[];
extern const char kEnableQuickAnswersRichUiDescription[];

extern const char kEnableQuickAnswersTextAnnotatorName[];
extern const char kEnableQuickAnswersTextAnnotatorDescription[];

extern const char kEnableQuickAnswersTranslationName[];
extern const char kEnableQuickAnswersTranslationDescription[];

extern const char kEnableQuickAnswersTranslationCloudAPIName[];
extern const char kEnableQuickAnswersTranslationCloudAPIDescription[];

extern const char kPluginVmFullscreenName[];
extern const char kPluginVmFullscreenDescription[];

extern const char kPluginVmShowCameraPermissionsName[];
extern const char kPluginVmShowCameraPermissionsDescription[];

extern const char kPluginVmShowMicrophonePermissionsName[];
extern const char kPluginVmShowMicrophonePermissionsDescription[];

extern const char kTrimOnFreezeName[];
extern const char kTrimOnFreezeDescription[];

extern const char kTrimOnMemoryPressureName[];
extern const char kTrimOnMemoryPressureDescription[];

extern const char kFilesJsModulesName[];
extern const char kFilesJsModulesDescription[];

extern const char kAudioPlayerJsModulesName[];
extern const char kAudioPlayerJsModulesDescription[];

extern const char kVideoPlayerJsModulesName[];
extern const char kVideoPlayerJsModulesDescription[];

extern const char kEcheSWAName[];
extern const char kEcheSWADescription[];

extern const char kEnableNetworkingInDiagnosticsAppName[];
extern const char kEnableNetworkingInDiagnosticsAppDescription[];

extern const char kEnableSuggestedFilesName[];
extern const char kEnableSuggestedFilesDescription[];

extern const char kEnhancedDeskAnimationsName[];
extern const char kEnhancedDeskAnimationsDescription[];

extern const char kEnterpriseReportingInChromeOSName[];
extern const char kEnterpriseReportingInChromeOSDescription[];

extern const char kExoGamepadVibrationName[];
extern const char kExoGamepadVibrationDescription[];

extern const char kExoOrdinalMotionName[];
extern const char kExoOrdinalMotionDescription[];

extern const char kExoPointerLockName[];
extern const char kExoPointerLockDescription[];

extern const char kExoLockNotificationName[];
extern const char kExoLockNotificationDescription[];

extern const char kExperimentalAccessibilityDictationExtensionName[];
extern const char kExperimentalAccessibilityDictationExtensionDescription[];

extern const char kExperimentalAccessibilityDictationListeningName[];
extern const char kExperimentalAccessibilityDictationListeningDescription[];

extern const char kExperimentalAccessibilityDictationOfflineName[];
extern const char kExperimentalAccessibilityDictationOfflineDescription[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char kSwitchAccessPointScanningName[];
extern const char kSwitchAccessPointScanningDescription[];

extern const char kExperimentalAccessibilitySwitchAccessSetupGuideName[];
extern const char kExperimentalAccessibilitySwitchAccessSetupGuideDescription[];

extern const char kMagnifierNewFocusFollowingName[];
extern const char kMagnifierNewFocusFollowingDescription[];

extern const char kMagnifierPanningImprovementsName[];
extern const char kMagnifierPanningImprovementsDescription[];

extern const char kMagnifierContinuousMouseFollowingModeSettingName[];
extern const char kMagnifierContinuousMouseFollowingModeSettingDescription[];

extern const char kFilesAppCopyImageName[];
extern const char kFilesAppCopyImageDescription[];

extern const char kFilesNGName[];
extern const char kFilesNGDescription[];

extern const char kFilesSinglePartitionFormatName[];
extern const char kFilesSinglePartitionFormatDescription[];

extern const char kFilesSWAName[];
extern const char kFilesSWADescription[];

extern const char kFilesTrashName[];
extern const char kFilesTrashDescription[];

extern const char kFilesZipMountName[];
extern const char kFilesZipMountDescription[];

extern const char kFilesZipPackName[];
extern const char kFilesZipPackDescription[];

extern const char kFilesZipUnpackName[];
extern const char kFilesZipUnpackDescription[];

extern const char kFiltersInRecentsName[];
extern const char kFiltersInRecentsDescription[];

extern const char kFrameThrottleFpsName[];
extern const char kFrameThrottleFpsDescription[];
extern const char kFrameThrottleFpsDefault[];
extern const char kFrameThrottleFps5[];
extern const char kFrameThrottleFps10[];
extern const char kFrameThrottleFps15[];
extern const char kFrameThrottleFps20[];
extern const char kFrameThrottleFps25[];
extern const char kFrameThrottleFps30[];

extern const char kFsNosymfollowName[];
extern const char kFsNosymfollowDescription[];

extern const char kFullRestoreName[];
extern const char kFullRestoreDescription[];

extern const char kHelpAppLauncherSearchName[];
extern const char kHelpAppLauncherSearchDescription[];

extern const char kHelpAppSearchServiceIntegrationName[];
extern const char kHelpAppSearchServiceIntegrationDescription[];

extern const char kHideArcMediaNotificationsName[];
extern const char kHideArcMediaNotificationsDescription[];

extern const char kHoldingSpaceName[];
extern const char kHoldingSpaceDescription[];

extern const char kHoldingSpacePreviewsName[];
extern const char kHoldingSpacePreviewsDescription[];

extern const char kImeAssistAutocorrectName[];
extern const char kImeAssistAutocorrectDescription[];

extern const char kImeAssistMultiWordName[];
extern const char kImeAssistMultiWordDescription[];

extern const char kImeAssistPersonalInfoName[];
extern const char kImeAssistPersonalInfoDescription[];

extern const char kImeEmojiSuggestAdditionName[];
extern const char kImeEmojiSuggestAdditionDescription[];

extern const char kImeMozcProtoName[];
extern const char kImeMozcProtoDescription[];

extern const char kImeServiceDecoderName[];
extern const char kImeServiceDecoderDescription[];

extern const char kImeSystemEmojiPickerName[];
extern const char kImeSystemEmojiPickerDescription[];

extern const char kIntentHandlingSharingName[];
extern const char kIntentHandlingSharingDescription[];

extern const char kIntentPickerPWAPersistenceName[];
extern const char kIntentPickerPWAPersistenceDescription[];

extern const char kInteractiveWindowCycleList[];
extern const char kInteractiveWindowCycleListDescription[];

extern const char kKeyboardBasedDisplayArrangementInSettingsName[];
extern const char kKeyboardBasedDisplayArrangementInSettingsDescription[];

extern const char kLacrosPrimaryName[];
extern const char kLacrosPrimaryDescription[];

extern const char kLacrosStabilityName[];
extern const char kLacrosStabilityDescription[];
extern const char kLacrosStabilityLessStableDescription[];
extern const char kLacrosStabilityMoreStableDescription[];

extern const char kLacrosSupportName[];
extern const char kLacrosSupportDescription[];

extern const char kLacrosWebAppsName[];
extern const char kLacrosWebAppsDescription[];

extern const char kLimitAltTabToActiveDeskName[];
extern const char kLimitAltTabToActiveDeskDescription[];

extern const char kLimitShelfItemsToActiveDeskName[];
extern const char kLimitShelfItemsToActiveDeskDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kEnableHardwareMirrorModeName[];
extern const char kEnableHardwareMirrorModeDescription[];

extern const char kLockScreenMediaControlsName[];
extern const char kLockScreenMediaControlsDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMediaAppName[];
extern const char kMediaAppDescription[];

extern const char kMediaAppAnnotationName[];
extern const char kMediaAppAnnotationDescription[];

extern const char kMediaAppDisplayExifName[];
extern const char kMediaAppDisplayExifDescription[];

extern const char kMediaAppPdfInInkName[];
extern const char kMediaAppPdfInInkDescription[];

extern const char kMediaAppVideoName[];
extern const char kMediaAppVideoDescription[];

extern const char kMediaSessionNotificationsName[];
extern const char kMediaSessionNotificationsDescription[];

extern const char kMeteredShowToggleName[];
extern const char kMeteredShowToggleDescription[];

extern const char kMultilingualTypingName[];
extern const char kMultilingualTypingDescription[];

extern const char kNearbySharingName[];
extern const char kNearbySharingDescription[];

extern const char kNearbySharingDeviceContactsName[];
extern const char kNearbySharingDeviceContactsDescription[];

extern const char kNearbySharingWebRtcName[];
extern const char kNearbySharingWebRtcDescription[];

extern const char kPhoneHubName[];
extern const char kPhoneHubDescription[];

extern const char kReduceDisplayNotificationsName[];
extern const char kReduceDisplayNotificationsDescription[];

extern const char kReleaseNotesNotificationName[];
extern const char kReleaseNotesNotificationDescription[];

extern const char kReleaseNotesNotificationAllChannelsName[];
extern const char kReleaseNotesNotificationAllChannelsDescription[];

extern const char kArcGhostWindowName[];
extern const char kArcGhostWindowDescription[];

extern const char kArcResizeLockName[];
extern const char kArcResizeLockDescription[];

extern const char kScalableStatusAreaName[];
extern const char kScalableStatusAreaDescription[];

extern const char kScanAppMediaLinkName[];
extern const char kScanAppMediaLinkDescription[];

extern const char kScanAppStickySettingsName[];
extern const char kScanAppStickySettingsDescription[];

extern const char kShimlessRMAFlowName[];
extern const char kShimlessRMAFlowDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kSelectToSpeakNavigationControlName[];
extern const char kSelectToSpeakNavigationControlDescription[];

extern const char kSharesheetContentPreviewsName[];
extern const char kSharesheetContentPreviewsDescription[];

extern const char kSharesheetName[];
extern const char kSharesheetDescription[];

extern const char kChromeOSSharingHubName[];
extern const char kChromeOSSharingHubDescription[];

extern const char kShowBluetoothDebugLogToggleName[];
extern const char kShowBluetoothDebugLogToggleDescription[];

extern const char kBluetoothSessionizedMetricsName[];
extern const char kBluetoothSessionizedMetricsDescription[];

extern const char kShowDateInTrayName[];
extern const char kShowDateInTrayDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSmbfsFileSharesName[];
extern const char kSmbfsFileSharesDescription[];

extern const char kSpectreVariant2MitigationName[];
extern const char kSpectreVariant2MitigationDescription[];

extern const char kSplitSettingsSyncName[];
extern const char kSplitSettingsSyncDescription[];

extern const char kSystemLatinPhysicalTypingName[];
extern const char kSystemLatinPhysicalTypingDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiShowCompositedLayerBordersName[];
extern const char kUiShowCompositedLayerBordersDescription[];
extern const char kUiShowCompositedLayerBordersRenderPass[];
extern const char kUiShowCompositedLayerBordersSurface[];
extern const char kUiShowCompositedLayerBordersLayer[];
extern const char kUiShowCompositedLayerBordersAll[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUnifiedMediaViewName[];
extern const char kUnifiedMediaViewDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVaapiWebPImageDecodeAccelerationName[];
extern const char kVaapiWebPImageDecodeAccelerationDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardBorderedKeyName[];
extern const char kVirtualKeyboardBorderedKeyDescription[];

extern const char kVirtualKeyboardDisabledName[];
extern const char kVirtualKeyboardDisabledDescription[];

extern const char kVirtualKeyboardMultipasteName[];
extern const char kVirtualKeyboardMultipasteDescription[];

extern const char kVmCameraMicIndicatorsAndNotificationsName[];
extern const char kVmCameraMicIndicatorsAndNotificationsDescription[];

extern const char kVmStatusPageName[];
extern const char kVmStatusPageDescription[];

extern const char kWakeOnWifiAllowedName[];
extern const char kWakeOnWifiAllowedDescription[];

extern const char kWebuiDarkModeName[];
extern const char kWebuiDarkModeDescription[];

extern const char kWifiSyncAllowDeletesName[];
extern const char kWifiSyncAllowDeletesDescription[];

extern const char kWifiSyncAndroidName[];
extern const char kWifiSyncAndroidDescription[];

// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
extern const char kDeprecateLowUsageCodecsName[];
extern const char kDeprecateLowUsageCodecsDescription[];

extern const char kVaapiAV1DecoderName[];
extern const char kVaapiAV1DecoderDescription[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
extern const char kChromeOSDirectVideoDecoderName[];
extern const char kChromeOSDirectVideoDecoderDescription[];
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_WIN)
extern const char kZeroCopyVideoCaptureName[];
extern const char kZeroCopyVideoCaptureDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_WIN)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)

extern const char kDesktopInProductHelpSnoozeName[];
extern const char kDesktopInProductHelpSnoozeDescription[];

extern const char kEnableMDRoundedCornersOnDialogsName[];
extern const char kEnableMDRoundedCornersOnDialogsDescription[];

extern const char kInstallableInkDropName[];
extern const char kInstallableInkDropDescription[];

extern const char kTextfieldFocusOnTapUpName[];
extern const char kTextfieldFocusOnTapUpDescription[];

extern const char kEnableNewBadgeOnMenuItemsName[];
extern const char kEnableNewBadgeOnMenuItemsDescription[];

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

extern const char kEnableOopPrintDriversName[];
extern const char kEnableOopPrintDriversDescription[];

extern const char kRemoteCopyReceiverName[];
extern const char kRemoteCopyReceiverDescription[];

extern const char kRemoteCopyImageNotificationName[];
extern const char kRemoteCopyImageNotificationDescription[];

extern const char kRemoteCopyPersistentNotificationName[];
extern const char kRemoteCopyPersistentNotificationDescription[];

extern const char kRemoteCopyProgressNotificationName[];
extern const char kRemoteCopyProgressNotificationDescription[];

extern const char kDirectManipulationStylusName[];
extern const char kDirectManipulationStylusDescription[];

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)

extern const char kCommanderName[];
extern const char kCommanderDescription[];

extern const char kDesktopRestructuredLanguageSettingsName[];
extern const char kDesktopRestructuredLanguageSettingsDescription[];

extern const char kDesktopDetailedLanguageSettingsName[];
extern const char kDesktopDetailedLanguageSettingsDescription[];

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
extern const char kDynamicTcmallocName[];
extern const char kDynamicTcmallocDescription[];
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kUserDataSnapshotName[];
extern const char kUserDataSnapshotDescription[];
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC)
extern const char kWebShareName[];
extern const char kWebShareDescription[];
#endif  // defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
extern const char kEnableEphemeralGuestProfilesOnDesktopName[];
extern const char kEnableEphemeralGuestProfilesOnDesktopDescription[];
#endif  // defined(OS_WIN) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_MAC)

#if defined(OS_LINUX) && defined(USE_OZONE)
extern const char kUseOzonePlatformName[];
extern const char kUseOzonePlatformDescription[];
#endif  // defined(OS_LINUX) && defined(USE_OZONE)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_JXL_DECODER)
extern const char kEnableJXLName[];
extern const char kEnableJXLDescription[];
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const char kDiceWebSigninInterceptionName[];
extern const char kDiceWebSigninInterceptionDescription[];
#endif  // ENABLE_DICE_SUPPORT

#if BUILDFLAG(ENABLE_NACL)
extern const char kNaclName[];
extern const char kNaclDescription[];
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
extern const char kPaintPreviewDemoName[];
extern const char kPaintPreviewDemoDescription[];
extern const char kPaintPreviewStartupName[];
extern const char kPaintPreviewStartupDescription[];
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPdfViewerDocumentPropertiesName[];
extern const char kPdfViewerDocumentPropertiesDescription[];

extern const char kPdfViewerPresentationModeName[];
extern const char kPdfViewerPresentationModeDescription[];
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kWebUITabStripName[];
extern const char kWebUITabStripDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kWebUITabStripTabDragIntegrationName[];
extern const char kWebUITabStripTabDragIntegrationDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if !defined(OS_WIN) && !defined(OS_FUCHSIA)
extern const char kSendWebUIJavaScriptErrorReportsName[];
extern const char kSendWebUIJavaScriptErrorReportsDescription[];
#endif

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
extern const char kUIDebugToolsName[];
extern const char kUIDebugToolsDescription[];
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
extern const char kWebrtcPipeWireCapturerName[];
extern const char kWebrtcPipeWireCapturerDescription[];
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
