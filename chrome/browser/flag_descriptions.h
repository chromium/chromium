// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/logging.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "ui/android/buildflags.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#endif  // defined(OS_LINUX)

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

extern const char kAccessibilityExposeARIAAnnotationsName[];
extern const char kAccessibilityExposeARIAAnnotationsDescription[];

extern const char kAccessibilityExposeDisplayNoneName[];
extern const char kAccessibilityExposeDisplayNoneDescription[];

extern const char kAccessibilityInternalsPageImprovementsName[];
extern const char kAccessibilityInternalsPageImprovementsDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAllowPopupsDuringPageUnloadName[];
extern const char kAllowPopupsDuringPageUnloadDescription[];

extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[];
extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[];

extern const char kAllowSyncXHRInPageDismissalName[];
extern const char kAllowSyncXHRInPageDismissalDescription[];

extern const char kEnableClipboardProviderTextSuggestionsName[];
extern const char kEnableClipboardProviderTextSuggestionsDescription[];

extern const char kEnableFtpName[];
extern const char kEnableFtpDescription[];

extern const char kEnableSignedExchangeSubresourcePrefetchName[];
extern const char kEnableSignedExchangeSubresourcePrefetchDescription[];

extern const char kEnableSignedExchangePrefetchCacheForNavigationsName[];
extern const char kEnableSignedExchangePrefetchCacheForNavigationsDescription[];

extern const char kAudioWorkletRealtimeThreadName[];
extern const char kAudioWorkletRealtimeThreadDescription[];

extern const char kUpdatedCellularActivationUiName[];
extern const char kUpdatedCellularActivationUiDescription[];

extern const char kUseMessagesGoogleComDomainName[];
extern const char kUseMessagesGoogleComDomainDescription[];

extern const char kUseMessagesStagingUrlName[];
extern const char kUseMessagesStagingUrlDescription[];

extern const char kEnableMessagesWebPushName[];
extern const char kEnableMessagesWebPushDescription[];

extern const char kAndroidPictureInPictureAPIName[];
extern const char kAndroidPictureInPictureAPIDescription[];

extern const char kAndroidSiteSettingsUIRefreshName[];
extern const char kAndroidSiteSettingsUIRefreshDescription[];

extern const char kAutomaticPasswordGenerationName[];
extern const char kAutomaticPasswordGenerationDescription[];

extern const char kAutofillAlwaysReturnCloudTokenizedCardName[];
extern const char kAutofillAlwaysReturnCloudTokenizedCardDescription[];

extern const char kAutofillAlwaysShowServerCardsInSyncTransportName[];
extern const char kAutofillAlwaysShowServerCardsInSyncTransportDescription[];

extern const char kAutofillAssistantChromeEntryName[];
extern const char kAutofillAssistantChromeEntryDescription[];

extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

extern const char kAutofillEnableLocalCardMigrationForNonSyncUserName[];
extern const char kAutofillEnableLocalCardMigrationForNonSyncUserDescription[];

extern const char kAutofillEnableToolbarStatusChipName[];
extern const char kAutofillEnableToolbarStatusChipDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

extern const char kAutofillNoLocalSaveOnUnmaskSuccessName[];
extern const char kAutofillNoLocalSaveOnUnmaskSuccessDescription[];

extern const char kAutofillNoLocalSaveOnUploadSuccessName[];
extern const char kAutofillNoLocalSaveOnUploadSuccessDescription[];

extern const char kAutofillOffNoServerDataName[];
extern const char kAutofillOffNoServerDataDescription[];

extern const char kAutofillProfileClientValidationName[];
extern const char kAutofillProfileClientValidationDescription[];

extern const char kAutofillProfileServerValidationName[];
extern const char kAutofillProfileServerValidationDescription[];

extern const char kAutofillRejectCompanyBirthyearName[];
extern const char kAutofillRejectCompanyBirthyearDescription[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

extern const char kAutofillSaveAndFillVPAName[];
extern const char kAutofillSaveAndFillVPADescription[];

extern const char kAutofillSettingsSplitByCardTypeName[];
extern const char kAutofillSettingsSplitByCardTypeDescription[];

extern const char kAutofillUseImprovedLabelDisambiguationName[];
extern const char kAutofillUseImprovedLabelDisambiguationDescription[];

extern const char kAutoScreenBrightnessName[];
extern const char kAutoScreenBrightnessDescription[];

extern const char kBackForwardCacheName[];
extern const char kBackForwardCacheDescription[];

extern const char kBrowserTaskSchedulerName[];
extern const char kBrowserTaskSchedulerDescription[];

extern const char kBundledConnectionHelpName[];
extern const char kBundledConnectionHelpDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kCaptionSettingsName[];
extern const char kCaptionSettingsDescription[];

extern const char kContextMenuSearchWithGoogleLensName[];
extern const char kContextMenuSearchWithGoogleLensDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kCloudImportName[];
extern const char kCloudImportDescription[];

extern const char kCloudPrinterHandlerName[];
extern const char kCloudPrinterHandlerDescription[];

extern const char kDecodeJpeg420ImagesToYUVName[];
extern const char kDecodeJpeg420ImagesToYUVDescription[];

extern const char kDecodeLossyWebPImagesToYUVName[];
extern const char kDecodeLossyWebPImagesToYUVDescription[];

extern const char kDnsOverHttpsName[];
extern const char kDnsOverHttpsDescription[];

extern const char kDrawVerticallyEdgeToEdgeName[];
extern const char kDrawVerticallyEdgeToEdgeDescription[];

extern const char kEnablePasswordsAccountStorageName[];
extern const char kEnablePasswordsAccountStorageDescription[];

extern const char kExperimentalAccessibilityFeaturesName[];
extern const char kExperimentalAccessibilityFeaturesDescription[];

extern const char kExperimentalAccessibilityAutoclickName[];
extern const char kExperimentalAccessibilityAutoclickDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionName[];
extern const char kExperimentalAccessibilityLanguageDetectionDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionDynamicName[];
extern const char
    kExperimentalAccessibilityLanguageDetectionDynamicDescription[];

extern const char kFontSrcLocalMatchingName[];
extern const char kFontSrcLocalMatchingDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileSCRGBLinear[];
extern const char kForceColorProfileHDR10[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kCookieDeprecationMessagesName[];
extern const char kCookieDeprecationMessagesDescription[];

extern const char kCookiesWithoutSameSiteMustBeSecureName[];
extern const char kCookiesWithoutSameSiteMustBeSecureDescription[];

extern const char kCooperativeSchedulingName[];
extern const char kCooperativeSchedulingDescription[];

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kDarkenWebsitesCheckboxInThemesSettingName[];
extern const char kDarkenWebsitesCheckboxInThemesSettingDescription[];

extern const char kDataSaverServerPreviewsName[];
extern const char kDataSaverServerPreviewsDescription[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

extern const char kDevtoolsExperimentsName[];
extern const char kDevtoolsExperimentsDescription[];

extern const char kDisableAudioForDesktopShareName[];
extern const char kDisableAudioForDesktopShareDescription[];

extern const char kDisableBestEffortTasksName[];
extern const char kDisableBestEffortTasksDescription[];

extern const char kDisableIpcFloodingProtectionName[];
extern const char kDisableIpcFloodingProtectionDescription[];

extern const char kDisablePushStateThrottleName[];
extern const char kDisablePushStateThrottleDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kDisallowUnsafeHttpDownloadsName[];
extern const char kDisallowUnsafeHttpDownloadsNameDescription[];

extern const char kDisplayList2dCanvasName[];
extern const char kDisplayList2dCanvasDescription[];

extern const char kDownloadResumptionWithoutStrongValidatorsName[];
extern const char kDownloadResumptionWithoutStrongValidatorsDescription[];

extern const char kEnableAccessibilityImageDescriptionsName[];
extern const char kEnableAccessibilityImageDescriptionsDescription[];

extern const char kEnableAccessibilityObjectModelName[];
extern const char kEnableAccessibilityObjectModelDescription[];

extern const char kEnableAmbientAuthenticationInIncognitoName[];
extern const char kEnableAmbientAuthenticationInIncognitoDescription[];

extern const char kEnableAmbientAuthenticationInGuestSessionName[];
extern const char kEnableAmbientAuthenticationInGuestSessionDescription[];

extern const char kEnableAudioFocusEnforcementName[];
extern const char kEnableAudioFocusEnforcementDescription[];

extern const char kEnableAutocompleteDataRetentionPolicyName[];
extern const char kEnableAutocompleteDataRetentionPolicyDescription[];

extern const char kEnableAutofillAccountWalletStorageName[];
extern const char kEnableAutofillAccountWalletStorageDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardAuthenticationName[];
extern const char kEnableAutofillCreditCardAuthenticationDescription[];

extern const char kEnableAutofillCreditCardLastUsedDateDisplayName[];
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

extern const char kEnableAutofillCreditCardUploadEditableCardholderNameName[];
extern const char
    kEnableAutofillCreditCardUploadEditableCardholderNameDescription[];

extern const char kEnableAutofillCreditCardUploadEditableExpirationDateName[];
extern const char
    kEnableAutofillCreditCardUploadEditableExpirationDateDescription[];

extern const char kEnableAutofillCreditCardUploadFeedbackName[];
extern const char kEnableAutofillCreditCardUploadFeedbackDescription[];

extern const char kEnableAutofillDoNotMigrateUnsupportedLocalCardsName[];
extern const char kEnableAutofillDoNotMigrateUnsupportedLocalCardsDescription[];

extern const char kEnableAutofillToolkitViewsCreditCardDialogsMac[];
extern const char kEnableAutofillToolkitViewsCreditCardDialogsMacDescription[];

extern const char kEnableAutofillNativeDropdownViewsName[];
extern const char kEnableAutofillNativeDropdownViewsDescription[];

extern const char kEnableAutofillUpdatedCardUnmaskPromptUiName[];
extern const char kEnableAutofillUpdatedCardUnmaskPromptUiDescription[];

extern const char kEnableDeferAllScriptName[];
extern const char kEnableDeferAllScriptDescription[];

extern const char kEnableDeferAllScriptWithoutOptimizationHintsName[];
extern const char kEnableDeferAllScriptWithoutOptimizationHintsDescription[];

extern const char kEnableEduCoexistenceName[];
extern const char kEnableEduCoexistenceDescription[];

extern const char kEnableSaveDataName[];
extern const char kEnableSaveDataDescription[];

extern const char kEnableNoScriptPreviewsName[];
extern const char kEnableNoScriptPreviewsDescription[];

extern const char kEnableRemovingAllThirdPartyCookiesName[];
extern const char kEnableRemovingAllThirdPartyCookiesDescription[];

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
extern const char kEnableDataReductionProxyNetworkServiceName[];
extern const char kEnableDataReductionProxyNetworkServiceDescription[];
extern const char kEnableDataReductionProxyServerExperimentName[];
extern const char kEnableDataReductionProxyServerExperimentDescription[];

extern const char kDesktopMinimalUIName[];
extern const char kDesktopMinimalUIDescription[];

extern const char kEnableDesktopPWAsName[];
extern const char kEnableDesktopPWAsDescription[];

extern const char kDesktopPWAsLocalUpdatingName[];
extern const char kDesktopPWAsLocalUpdatingDescription[];

extern const char kDesktopPWAsOmniboxInstallName[];
extern const char kDesktopPWAsOmniboxInstallDescription[];

extern const char kEnableSystemWebAppsName[];
extern const char kEnableSystemWebAppsDescription[];

extern const char kEnableTLS13EarlyDataName[];
extern const char kEnableTLS13EarlyDataDescription[];

extern const char kWinrtGeolocationImplementationName[];
extern const char kWinrtGeolocationImplementationDescription[];

extern const char kWinrtSensorsImplementationName[];
extern const char kWinrtSensorsImplementationDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableHistoryFaviconsGoogleServerQueryName[];
extern const char kEnableHistoryFaviconsGoogleServerQueryDescription[];

extern const char kEnableImplicitRootScrollerName[];
extern const char kEnableImplicitRootScrollerDescription[];

extern const char kEnableCSSOMViewScrollCoordinatesName[];
extern const char kEnableCSSOMViewScrollCoordinatesDescription[];

extern const char kEnableLitePageServerPreviewsName[];
extern const char kEnableLitePageServerPreviewsDescription[];

extern const char kBuiltInModuleAllName[];
extern const char kBuiltInModuleAllDescription[];

extern const char kBuiltInModuleInfraName[];
extern const char kBuiltInModuleInfraDescription[];

extern const char kBuiltInModuleKvStorageName[];
extern const char kBuiltInModuleKvStorageDescription[];

extern const char kEnableDisplayLockingName[];
extern const char kEnableDisplayLockingDescription[];

extern const char kEnableLayoutNGName[];
extern const char kEnableLayoutNGDescription[];

extern const char kEnableLazyFrameLoadingName[];
extern const char kEnableLazyFrameLoadingDescription[];

extern const char kEnableLazyImageLoadingName[];
extern const char kEnableLazyImageLoadingDescription[];

extern const char kEnableMacMaterialDesignDownloadShelfName[];
extern const char kEnableMacMaterialDesignDownloadShelfDescription[];

extern const char kEnableMaterialDesignBookmarksName[];
extern const char kEnableMaterialDesignBookmarksDescription[];

extern const char kEnableMediaSessionServiceName[];
extern const char kEnableMediaSessionServiceDescription[];

extern const char kEnableNavigationTracingName[];
extern const char kEnableNavigationTracingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableNetworkServiceInProcessName[];
extern const char kEnableNetworkServiceInProcessDescription[];

extern const char kEnableNotificationScrollBarName[];
extern const char kEnableNotificationScrollBarDescription[];

extern const char kEnableNotificationExpansionAnimationName[];
extern const char kEnableNotificationExpansionAnimationDescription[];

extern const char kEnableOutOfBlinkCorsName[];
extern const char kEnableOutOfBlinkCorsDescription[];

extern const char kCrossOriginIsolationName[];
extern const char kCrossOriginIsolationDescription[];

extern const char kDisableKeepaliveFetchName[];
extern const char kDisableKeepaliveFetchDescription[];

extern const char kVizDisplayCompositorName[];
extern const char kVizDisplayCompositorDescription[];

extern const char kVizHitTestName[];
extern const char kVizHitTestDescription[];

extern const char kMemlogName[];
extern const char kMemlogDescription[];
extern const char kMemlogModeMinimal[];
extern const char kMemlogModeAll[];
extern const char kMemlogModeAllRenderers[];
extern const char kMemlogModeBrowser[];
extern const char kMemlogModeGpu[];
extern const char kMemlogModeManual[];
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

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePortalsName[];
extern const char kEnablePortalsDescription[];

extern const char kEnablePictureInPictureName[];
extern const char kEnablePictureInPictureDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnablePreviewsCoinFlipName[];
extern const char kEnablePreviewsCoinFlipDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];

extern const char kEnableResourceLoadingHintsName[];
extern const char kEnableResourceLoadingHintsDescription[];

extern const char kEnableSubresourceRedirectName[];
extern const char kEnableSubresourceRedirectDescription[];

extern const char kEnableSyncUSSNigoriName[];
extern const char kEnableSyncUSSNigoriDescription[];

extern const char kEnableSyncUSSPasswordsName[];
extern const char kEnableSyncUSSPasswordsDescription[];

extern const char kEnableSyncUSSSessionsName[];
extern const char kEnableSyncUSSSessionsDescription[];

extern const char kEnableTextFragmentAnchorName[];
extern const char kEnableTextFragmentAnchorDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kEnableScrollAnchorSerializationName[];
extern const char kEnableScrollAnchorSerializationDescription[];

extern const char kEnableSharedArrayBufferName[];
extern const char kEnableSharedArrayBufferDescription[];

extern const char kEnableWasmName[];
extern const char kEnableWasmDescription[];

extern const char kEnableWebAuthenticationCableV2SupportName[];
extern const char kEnableWebAuthenticationCableV2SupportDescription[];

extern const char kEnableWebUsbName[];
extern const char kEnableWebUsbDescription[];

extern const char kExperimentalWebAssemblyFeaturesName[];
extern const char kExperimentalWebAssemblyFeaturesDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmCodeGCName[];
extern const char kEnableWasmCodeGCDescription[];

extern const char kEnableWasmCodeCacheName[];
extern const char kEnableWasmCodeCacheDescription[];

extern const char kEnableWasmSimdName[];
extern const char kEnableWasmSimdDescription[];

extern const char kEnableWasmThreadsName[];
extern const char kEnableWasmThreadsDescription[];

extern const char kEvDetailsInPageInfoName[];
extern const char kEvDetailsInPageInfoDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

extern const char kExperimentalCanvasFeaturesName[];
extern const char kExperimentalCanvasFeaturesDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalProductivityFeaturesName[];
extern const char kExperimentalProductivityFeaturesDescription[];

extern const char kExperimentalSecurityFeaturesName[];
extern const char kExperimentalSecurityFeaturesDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

extern const char kExtensionsToolbarMenuName[];
extern const char kExtensionsToolbarMenuDescription[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFeaturePolicyName[];
extern const char kFeaturePolicyDescription[];

extern const char kFilteringScrollPredictionName[];
extern const char kFilteringScrollPredictionDescription[];

extern const char kFractionalScrollOffsetsName[];
extern const char kFractionalScrollOffsetsDescription[];

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

extern const char kFormControlsRefreshName[];
extern const char kFormControlsRefreshDescription[];

extern const char kGlobalMediaControlsName[];
extern const char kGlobalMediaControlsDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];
extern const char kForceGpuRasterization[];

extern const char kGooglePasswordManagerName[];
extern const char kGooglePasswordManagerDescription[];

extern const char kGoogleProfileInfoName[];
extern const char kGoogleProfileInfoDescription[];

extern const char kHandwritingGestureName[];
extern const char kHandwritingGestureDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHarfBuzzPDFSubsetterName[];
extern const char kHarfBuzzPDFSubsetterDescription[];

extern const char kHarfbuzzRendertextName[];
extern const char kHarfbuzzRendertextDescription[];

extern const char kHeavyAdPrivacyMitigationsOptOutName[];
extern const char kHeavyAdPrivacyMitigationsOptOutDescription[];

extern const char kHeavyAdInterventionName[];
extern const char kHeavyAdInterventionDescription[];

extern const char kHorizontalTabSwitcherAndroidName[];
extern const char kHorizontalTabSwitcherAndroidDescription[];

extern const char kTabSwitcherOnReturnName[];
extern const char kTabSwitcherOnReturnDescription[];

extern const char kHideActiveAppsFromShelfName[];
extern const char kHideActiveAppsFromShelfDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kHTTPAuthCommittedInterstitialsName[];
extern const char kHTTPAuthCommittedInterstitialsDescription[];

extern const char kIconNtpName[];
extern const char kIconNtpDescription[];

extern const char kIgnoreGpuBlacklistName[];
extern const char kIgnoreGpuBlacklistDescription[];

extern const char kIgnorePreviewsBlacklistName[];
extern const char kIgnorePreviewsBlacklistDescription[];

extern const char kIgnoreLitePageRedirectHintsBlacklistName[];
extern const char kIgnoreLitePageRedirectHintsBlacklistDescription[];

extern const char kCompositorThreadedScrollbarScrollingName[];
extern const char kCompositorThreadedScrollbarScrollingDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kKeepAliveRendererForKeepaliveRequestsName[];
extern const char kKeepAliveRendererForKeepaliveRequestsDescription[];

extern const char kLegacyTLSWarningsName[];
extern const char kLegacyTLSWarningsDescription[];

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kLogJsConsoleMessagesName[];
extern const char kLogJsConsoleMessagesDescription[];

extern const char kLookalikeUrlNavigationSuggestionsName[];
extern const char kLookalikeUrlNavigationSuggestionsDescription[];

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];
extern const char kMarkHttpAsWarning[];
extern const char kMarkHttpAsWarningAndDangerousOnFormEdits[];
extern const char kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards[];

extern const char kMediaInspectorLoggingName[];
extern const char kMediaInspectorLoggingDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMimeHandlerViewInCrossProcessFrameName[];
extern const char kMimeHandlerViewInCrossProcessFrameDescription[];

extern const char kMixBrowserTypeTabsName[];
extern const char kMixBrowserTypeTabsDescription[];

extern const char kMixedContentSiteSettingName[];
extern const char kMixedContentSiteSettingDescription[];

extern const char kMobileIdentityConsistencyName[];
extern const char kMobileIdentityConsistencyDescription[];

extern const char kMouseSubframeNoImplicitCaptureName[];
extern const char kMouseSubframeNoImplicitCaptureDescription[];

extern const char kNativeFileSystemAPIName[];
extern const char kNativeFileSystemAPIDescription[];

extern const char kNewAudioRenderingMixingStrategyName[];
extern const char kNewAudioRenderingMixingStrategyDescription[];

extern const char kNewBookmarkAppsName[];
extern const char kNewBookmarkAppsDescription[];

extern const char kNewPasswordFormParsingName[];
extern const char kNewPasswordFormParsingDescription[];

extern const char kNewPasswordFormParsingForSavingName[];
extern const char kNewPasswordFormParsingForSavingDescription[];

extern const char kOnlyNewPasswordFormParsingName[];
extern const char kOnlyNewPasswordFormParsingDescription[];

extern const char kUsernameFirstFlowName[];
extern const char kUsernameFirstFlowDescription[];

extern const char kUseSurfaceLayerForVideoName[];
extern const char kUseSurfaceLayerForVideoDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNewblueName[];
extern const char kNewblueDescription[];

extern const char kNewTabstripAnimationName[];
extern const char kNewTabstripAnimationDescription[];

extern const char kNotificationIndicatorName[];
extern const char kNotificationIndicatorDescription[];

extern const char kNotificationSchedulerDebugOptionName[];
extern const char kNotificationSchedulerDebugOptionDescription[];
extern const char kNotificationSchedulerImmediateBackgroundTaskDescription[];

extern const char kNotificationsNativeFlagName[];
extern const char kNotificationsNativeFlagDescription[];

extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

extern const char kOfferStoreUnmaskedWalletCardsName[];
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

extern const char kOmniboxAutocompleteTitlesName[];
extern const char kOmniboxAutocompleteTitlesDescription[];

extern const char kOmniboxDisableInstantExtendedLimitName[];
extern const char kOmniboxDisableInstantExtendedLimitDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxExperimentalSuggestScoringName[];
extern const char kOmniboxExperimentalSuggestScoringDescription[];

extern const char kOmniboxGroupSuggestionsBySearchVsUrlName[];
extern const char kOmniboxGroupSuggestionsBySearchVsUrlDescription[];

extern const char kOmniboxLocalEntitySuggestionsName[];
extern const char kOmniboxLocalEntitySuggestionsDescription[];

extern const char kOmniboxMaterialDesignWeatherIconsName[];
extern const char kOmniboxMaterialDesignWeatherIconsDescription[];

extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

extern const char kOmniboxRemoveSuggestionsFromClipboardName[];
extern const char kOmniboxRemoveSuggestionsFromClipboardDescription[];

extern const char kOmniboxRichEntitySuggestionsName[];
extern const char kOmniboxRichEntitySuggestionsDescription[];

extern const char kOmniboxShortBookmarkSuggestionsName[];
extern const char kOmniboxShortBookmarkSuggestionsDescription[];

extern const char kOmniboxSearchEngineLogoName[];
extern const char kOmniboxSearchEngineLogoDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxUIHideSteadyStateUrlSchemeName[];
extern const char kOmniboxUIHideSteadyStateUrlSchemeDescription[];

extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[];

extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefName[];
extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription[];

extern const char kOmniboxUIOneClickUnelideName[];
extern const char kOmniboxUIOneClickUnelideDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsName[];
extern const char kOmniboxOnDeviceHeadSuggestionsDescription[];

extern const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName[];
extern const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription[];

extern const char kOmniboxUIShowSuggestionFaviconsName[];
extern const char kOmniboxUIShowSuggestionFaviconsDescription[];

extern const char kOmniboxUISwapTitleAndUrlName[];
extern const char kOmniboxUISwapTitleAndUrlDescription[];

extern const char kOmniboxZeroSuggestionsOnNTPName[];
extern const char kOmniboxZeroSuggestionsOnNTPDescription[];

extern const char kOmniboxZeroSuggestionsOnNTPRealboxName[];
extern const char kOmniboxZeroSuggestionsOnNTPRealboxDescription[];

extern const char kOmniboxZeroSuggestionsOnSERPName[];
extern const char kOmniboxZeroSuggestionsOnSERPDescription[];

extern const char kOnTheFlyMhtmlHashComputationName[];
extern const char kOnTheFlyMhtmlHashComputationDescription[];

extern const char kOopRasterizationName[];
extern const char kOopRasterizationDescription[];

extern const char kEnableDeJellyName[];
extern const char kEnableDeJellyDescription[];

extern const char kOverlayNewLayoutName[];
extern const char kOverlayNewLayoutDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateName[];
extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription[];

extern const char kOverlayScrollbarsFlashWhenMouseEnterName[];
extern const char kOverlayScrollbarsFlashWhenMouseEnterDescription[];

extern const char kOverlayStrategiesName[];
extern const char kOverlayStrategiesDescription[];
extern const char kOverlayStrategiesDefault[];
extern const char kOverlayStrategiesNone[];
extern const char kOverlayStrategiesUnoccludedFullscreen[];
extern const char kOverlayStrategiesUnoccluded[];
extern const char kOverlayStrategiesOccludedAndUnoccluded[];

extern const char kUpdateHoverAtBeginFrameName[];
extern const char kUpdateHoverAtBeginFrameDescription[];

extern const char kUseNewAcceptLanguageHeaderName[];
extern const char kUseNewAcceptLanguageHeaderDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPasswordEditingAndroidName[];
extern const char kPasswordEditingAndroidDescription[];

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

extern const char kPasswordLeakDetectionName[];
extern const char kPasswordLeakDetectionDescription[];

extern const char kForceWebContentsDarkModeName[];
extern const char kForceWebContentsDarkModeDescription[];

extern const char kForcedColorsName[];
extern const char kForcedColorsDescription[];

extern const char kPercentBasedScrollingName[];
extern const char kPercentBasedScrollingDescription[];

extern const char kPeriodicBackgroundSyncName[];
extern const char kPeriodicBackgroundSyncDescription[];

extern const char kPerMethodCanMakePaymentQuotaName[];
extern const char kPerMethodCanMakePaymentQuotaDescription[];

extern const char kPointerLockOptionsName[];
extern const char kPointerLockOptionsDescription[];

extern const char kPolicyAtomicGroupsEnabledName[];
extern const char kPolicyAtomicGroupsEnabledDescription[];

extern const char kPreviewsAllowedName[];
extern const char kPreviewsAllowedDescription[];

extern const char kPrintPdfAsImageName[];
extern const char kPrintPdfAsImageDescription[];

extern const char kPrintPreviewRegisterPromosName[];
extern const char kPrintPreviewRegisterPromosDescription[];

extern const char kPrivacySettingsRedesignName[];
extern const char kPrivacySettingsRedesignDescription[];

extern const char kProminentDarkModeActiveTabTitleName[];
extern const char kProminentDarkModeActiveTabTitleDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kQueryInOmniboxName[];
extern const char kQueryInOmniboxDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kQuietNotificationPromptsName[];
extern const char kQuietNotificationPromptsDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

extern const char kRewriteLevelDBOnDeletionName[];
extern const char kRewriteLevelDBOnDeletionDescription[];

extern const char kRendererSideResourceSchedulerName[];
extern const char kRendererSideResourceSchedulerDescription[];

extern const char kReorderBookmarksName[];
extern const char kReorderBookmarksDescription[];

extern const char kRequestUnbufferedDispatchName[];
extern const char kRequestUnbufferedDispatchDescription[];

extern const char kRequestTabletSiteName[];
extern const char kRequestTabletSiteDescription[];

extern const char kPrefetchPrivacyChangesName[];
extern const char kPrefetchPrivacyChangesDescription[];

extern const char kPrefetchMainResourceNetworkIsolationKeyName[];
extern const char kPrefetchMainResourceNetworkIsolationKeyDescription[];

extern const char kSafeBrowsingUseAPDownloadVerdictsName[];
extern const char kSafeBrowsingUseAPDownloadVerdictsDescription[];

extern const char kSafetyTipName[];
extern const char kSafetyTipDescription[];

extern const char kSameSiteByDefaultCookiesName[];
extern const char kSameSiteByDefaultCookiesDescription[];

extern const char kSaveasMenuLabelExperimentName[];
extern const char kSaveasMenuLabelExperimentDescription[];

extern const char kScalableAppListName[];
extern const char kScalableAppListDescription[];

extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kSecurityInterstitialsDarkModeName[];
extern const char kSecurityInterstitialsDarkModeDescription[];

extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

extern const char kSendTabToSelfBroadcastName[];
extern const char kSendTabToSelfBroadcastDescription[];

extern const char kSendTabToSelfWhenSignedInName[];
extern const char kSendTabToSelfWhenSignedInDescription[];

extern const char kServiceWorkerImportedScriptUpdateCheckName[];
extern const char kServiceWorkerImportedScriptUpdateCheckDescription[];

extern const char kServiceWorkerLongRunningMessageName[];
extern const char kServiceWorkerLongRunningMessageDescription[];

extern const char kServiceWorkerOnUIName[];
extern const char kServiceWorkerOnUIDescription[];

extern const char kSettingsWindowName[];
extern const char kSettingsWindowDescription[];

extern const char kSharedClipboardReceiverName[];
extern const char kSharedClipboardReceiverDescription[];

extern const char kSharedClipboardUIName[];
extern const char kSharedClipboardUIDescription[];

extern const char kSharingDeviceRegistrationName[];
extern const char kSharingDeviceRegistrationDescription[];

extern const char kSharingPeerConnectionReceiverName[];
extern const char kSharingPeerConnectionReceiverDescription[];

extern const char kSharingPeerConnectionSenderName[];
extern const char kSharingPeerConnectionSenderDescription[];

extern const char kSharingRenameDevicesName[];
extern const char kSharingRenameDevicesDescription[];

extern const char kSharingUseDeviceInfoName[];
extern const char kSharingUseDeviceInfoDescription[];

extern const char kSharingDeriveVapidKeyName[];
extern const char kSharingDeriveVapidKeyDescription[];

extern const char kShelfHotseatName[];
extern const char kShelfHotseatDescription[];

extern const char kShelfHoverPreviewsName[];
extern const char kShelfHoverPreviewsDescription[];

extern const char kShelfScrollableName[];
extern const char kShelfScrollableDescription[];

extern const char kShowAndroidFilesInFilesAppName[];
extern const char kShowAndroidFilesInFilesAppDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kSkiaRendererName[];
extern const char kSkiaRendererDescription[];

extern const char kHistoryManipulationIntervention[];
extern const char kHistoryManipulationInterventionDescription[];

extern const char kEnableDrawOcclusionName[];
extern const char kEnableDrawOcclusionDescription[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

extern const char kSimplifyHttpsIndicatorName[];
extern const char kSimplifyHttpsIndicatorDescription[];

extern const char kStorageAccessAPIName[];
extern const char kStorageAccessAPIDescription[];

extern const char kIsolateOriginsName[];
extern const char kIsolateOriginsDescription[];

extern const char kKidsManagementUrlClassificationName[];
extern const char kKidsManagementUrlClassificationDescription[];

extern const char kSiteIsolationOptOutName[];
extern const char kSiteIsolationOptOutDescription[];
extern const char kSiteIsolationOptOutChoiceDefault[];
extern const char kSiteIsolationOptOutChoiceOptOut[];

extern const char kSiteSettings[];
extern const char kSiteSettingsDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kStopInBackgroundName[];
extern const char kStopInBackgroundDescription[];

extern const char kStopNonTimersInBackgroundName[];
extern const char kStopNonTimersInBackgroundDescription[];

extern const char kStoragePressureUIName[];
extern const char kStoragePressureUIDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncDeviceInfoInTransportModeName[];
extern const char kSyncDeviceInfoInTransportModeDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kTabEngagementReportingName[];
extern const char kTabEngagementReportingDescription[];

extern const char kTabGridLayoutAndroidName[];
extern const char kTabGridLayoutAndroidDescription[];

extern const char kTabGroupsAndroidName[];
extern const char kTabGroupsAndroidDescription[];

extern const char kTabGroupsContinuationAndroidName[];
extern const char kTabGroupsContinuationAndroidDescription[];

extern const char kTabGroupsUiImprovementsAndroidName[];
extern const char kTabGroupsUiImprovementsAndroidDescription[];

extern const char kTabToGTSAnimationAndroidName[];
extern const char kTabToGTSAnimationAndroidDescription[];

extern const char kTabGroupsName[];
extern const char kTabGroupsDescription[];

extern const char kTabHoverCardsName[];
extern const char kTabHoverCardsDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabOutlinesInLowContrastThemesName[];
extern const char kTabOutlinesInLowContrastThemesDescription[];

extern const char kTabsInCbdName[];
extern const char kTabsInCbdDescription[];

extern const char kTintGlCompositedContentName[];
extern const char kTintGlCompositedContentDescription[];

extern const char kTLS13HardeningForLocalAnchorsName[];
extern const char kTLS13HardeningForLocalAnchorsDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchEventsName[];
extern const char kTouchEventsDescription[];

extern const char kTouchpadOverscrollHistoryNavigationName[];
extern const char kTouchpadOverscrollHistoryNavigationDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

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

extern const char kTrySupportedChannelLayoutsName[];
extern const char kTrySupportedChannelLayoutsDescription[];

extern const char kTurnOffStreamingMediaCachingName[];
extern const char kTurnOffStreamingMediaCachingDescription[];

extern const char kUnexpireFlagsM78Name[];
extern const char kUnexpireFlagsM78Description[];
extern const char kUnexpireFlagsM80Name[];
extern const char kUnexpireFlagsM80Description[];

extern const char kUnsafeWebGPUName[];
extern const char kUnsafeWebGPUDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUseSearchClickForRightClickName[];
extern const char kUseSearchClickForRightClickDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebBundlesName[];
extern const char kWebBundlesDescription[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kWebPaymentsExperimentalFeaturesName[];
extern const char kWebPaymentsExperimentalFeaturesDescription[];

extern const char kWebrtcHideLocalIpsWithMdnsName[];
extern const char kWebrtcHideLocalIpsWithMdnsDecription[];

extern const char kWebrtcHybridAgcName[];
extern const char kWebrtcHybridAgcDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebrtcMultiChannelApmName[];
extern const char kWebrtcMultiChannelApmDescription[];

extern const char kWebrtcNewEncodeCpuLoadEstimatorName[];
extern const char kWebrtcNewEncodeCpuLoadEstimatorDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kWebUITabStripName[];
extern const char kWebUITabStripDescription[];

extern const char kWebUITabStripDemoOptionsName[];
extern const char kWebUITabStripDemoOptionsDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

extern const char kWebXrName[];
extern const char kWebXrDescription[];

extern const char kWebXrArModuleName[];
extern const char kWebXrArModuleDescription[];

extern const char kWebXrArDOMOverlayName[];
extern const char kWebXrArDOMOverlayDescription[];

extern const char kWebXrHitTestName[];
extern const char kWebXrHitTestDescription[];

extern const char kWebXrAnchorsName[];
extern const char kWebXrAnchorsDescription[];

extern const char kWebXrPlaneDetectionName[];
extern const char kWebXrPlaneDetectionDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAiaFetchingName[];
extern const char kAiaFetchingDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidSetupSearchEngineName[];
extern const char kAndroidSetupSearchEngineDescription[];

extern const char kAndroidSurfaceControl[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAppNotificationStatusMessagingName[];
extern const char kAppNotificationStatusMessagingDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kAutofillAssistantDirectActionsName[];
extern const char kAutofillAssistantDirectActionsDescription[];

extern const char kAutofillTouchToFillName[];
extern const char kAutofillTouchToFillDescription[];

extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

extern const char kBackgroundTaskComponentUpdateName[];
extern const char kBackgroundTaskComponentUpdateDescription[];

extern const char kCCTIncognitoName[];
extern const char kCCTIncognitoDescription[];

extern const char kCCTModuleName[];
extern const char kCCTModuleDescription[];

extern const char kCCTModuleCacheName[];
extern const char kCCTModuleCacheDescription[];

extern const char kCCTModuleCustomHeaderName[];
extern const char kCCTModuleCustomHeaderDescription[];

extern const char kCCTModuleCustomRequestHeaderName[];
extern const char kCCTModuleCustomRequestHeaderDescription[];

extern const char kCCTModuleDexLoadingName[];
extern const char kCCTModuleDexLoadingDescription[];

extern const char kCCTModulePostMessageName[];
extern const char kCCTModulePostMessageDescription[];

extern const char kCCTModuleUseIntentExtrasName[];
extern const char kCCTModuleUseIntentExtrasDescription[];

extern const char kCCTTargetTranslateLanguageName[];
extern const char kCCTTargetTranslateLanguageDescription[];

extern const char kChromeDuetName[];
extern const char kChromeDuetDescription[];

extern const char kChromeDuetLabelsName[];
extern const char kChromeDuetLabelsDescription[];

extern const char kChromeSharingHubName[];
extern const char kChromeSharingHubDescription[];

extern const char kClearOldBrowsingDataName[];
extern const char kClearOldBrowsingDataDescription[];

extern const char kClickToCallOpenDialerDirectlyName[];
extern const char kClickToCallOpenDialerDirectlyDescription[];

extern const char kCloseTabSuggestionsName[];
extern const char kCloseTabSuggestionsDescription[];

extern const char kClickToCallReceiverName[];
extern const char kClickToCallReceiverDescription[];

extern const char kContextualSearchDefinitionsName[];
extern const char kContextualSearchDefinitionsDescription[];

extern const char kContextualSearchLongpressResolveName[];
extern const char kContextualSearchLongpressResolveDescription[];

extern const char kContextualSearchMlTapSuppressionName[];
extern const char kContextualSearchMlTapSuppressionDescription[];

extern const char kContextualSearchRankerQueryName[];
extern const char kContextualSearchRankerQueryDescription[];

extern const char kContextualSearchSecondTapName[];
extern const char kContextualSearchSecondTapDescription[];

extern const char kContextualSearchSimplifiedServerName[];
extern const char kContextualSearchSimplifiedServerDescription[];

extern const char kContextualSearchTranslationModelName[];
extern const char kContextualSearchTranslationModelDescription[];

extern const char kDirectActionsName[];
extern const char kDirectActionsDescription[];

extern const char kDontPrefetchLibrariesName[];
extern const char kDontPrefetchLibrariesDescription[];

extern const char kDownloadRenameName[];
extern const char kDownloadRenameDescription[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAutofillRefreshStyleName[];
extern const char kEnableAutofillRefreshStyleDescription[];

extern const char kEnableAndroidSpellcheckerName[];
extern const char kEnableAndroidSpellcheckerDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnableRevampedContextMenuName[];
extern const char kEnableRevampedContextMenuDescription[];

extern const char kEnableNtpRemoteSuggestionsName[];
extern const char kEnableNtpRemoteSuggestionsDescription[];

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableWebNfcName[];
extern const char kEnableWebNfcDescription[];

extern const char kEphemeralTabName[];
extern const char kEphemeralTabDescription[];

extern const char kEphemeralTabUsingBottomSheetName[];
extern const char kEphemeralTabUsingBottomSheetDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kGamesHubName[];
extern const char kGamesHubDescription[];

extern const char kHomepageLocationName[];
extern const char kHomepageLocationDescription[];

extern const char kInterestFeedNotificationsName[];
extern const char kInterestFeedNotificationsDescription[];

extern const char kInterestFeedContentSuggestionsName[];
extern const char kInterestFeedContentSuggestionsDescription[];

extern const char kManualPasswordGenerationAndroidName[];
extern const char kManualPasswordGenerationAndroidDescription[];

extern const char kNoCreditCardAbort[];
extern const char kNoCreditCardAbortDescription[];

extern const char kOfflineIndicatorAlwaysHttpProbeName[];
extern const char kOfflineIndicatorAlwaysHttpProbeDescription[];

extern const char kOfflineIndicatorChoiceName[];
extern const char kOfflineIndicatorChoiceDescription[];

extern const char kOfflineHomeName[];
extern const char kOfflineHomeDescription[];

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

extern const char kOfflinePagesLimitlessPrefetchingName[];
extern const char kOfflinePagesLimitlessPrefetchingDescription[];

extern const char kOfflinePagesLoadSignalCollectingName[];
extern const char kOfflinePagesLoadSignalCollectingDescription[];

extern const char kOfflinePagesPrefetchingName[];
extern const char kOfflinePagesPrefetchingDescription[];

extern const char kOfflinePagesResourceBasedSnapshotName[];
extern const char kOfflinePagesResourceBasedSnapshotDescription[];

extern const char kOfflinePagesRenovationsName[];
extern const char kOfflinePagesRenovationsDescription[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kOfflinePagesShowAlternateDinoPageName[];
extern const char kOfflinePagesShowAlternateDinoPageDescription[];

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kPasswordManagerOnboardingAndroidName[];
extern const char kPasswordManagerOnboardingAndroidDescription[];

extern const char kProcessSharingWithDefaultSiteInstancesName[];
extern const char kProcessSharingWithDefaultSiteInstancesDescription[];

extern const char kProcessSharingWithStrictSiteInstancesName[];
extern const char kProcessSharingWithStrictSiteInstancesDescription[];

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAllArticles[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

extern const char kReaderModeInCCTName[];
extern const char kReaderModeInCCTDescription[];

extern const char kSafeBrowsingUseLocalBlacklistsV2Name[];
extern const char kSafeBrowsingUseLocalBlacklistsV2Description[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kShoppingAssistName[];
extern const char kShoppingAssistDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kTabSwitcherLongpressMenuName[];
extern const char kTabSwitcherLongpressMenuDescription[];

extern const char kStartSurfaceAndroidName[];
extern const char kStartSurfaceAndroidDescription[];

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

extern const char kUpdateNotificationSchedulingIntegrationName[];
extern const char kUpdateNotificationSchedulingIntegrationDescription[];

extern const char kUsageStatsDescription[];
extern const char kUsageStatsName[];

extern const char kInlineUpdateFlowName[];
extern const char kInlineUpdateFlowDescription[];

#if BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

extern const char kAndroidNightModeName[];
extern const char kAndroidNightModeDescription[];

#endif  // BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kForceDiceMigrationName[];
extern const char kForceDiceMigrationDescription[];

extern const char kShowSyncPausedReasonCookiesClearedOnExitName[];
extern const char kShowSyncPausedReasonCookiesClearedOnExitDescription[];

extern const char kAppManagementName[];
extern const char kAppManagementDescription[];

extern const char kAutofillDropdownLayoutName[];
extern const char kAutofillDropdownLayoutDescription[];

extern const char kCastMediaRouteProviderName[];
extern const char kCastMediaRouteProviderDescription[];

extern const char kChromeColorsName[];
extern const char kChromeColorsDescription[];

extern const char kChromeColorsCustomColorPickerName[];
extern const char kChromeColorsCustomColorPickerDescription[];

extern const char kDialMediaRouteProviderName[];
extern const char kDialMediaRouteProviderDescription[];

extern const char kNtpConfirmSuggestionRemovalsName[];
extern const char kNtpConfirmSuggestionRemovalsDescription[];

extern const char kNtpCustomizationMenuV2Name[];
extern const char kNtpCustomizationMenuV2Description[];

extern const char kNtpDismissPromosName[];
extern const char kNtpDismissPromosDescription[];

extern const char kNtpRealboxName[];
extern const char kNtpRealboxDescription[];

extern const char kNtpRealboxMatchOmniboxThemeName[];
extern const char kNtpRealboxMatchOmniboxThemeDescription[];

extern const char kImprovedCookieControlsName[];
extern const char kImprovedCookieControlsDescription[];

extern const char kImprovedCookieControlsForThirdPartyCookieBlockingName[];
extern const char
    kImprovedCookieControlsForThirdPartyCookieBlockingDescription[];

extern const char kEnableReaderModeName[];
extern const char kEnableReaderModeDescription[];

extern const char kEnableWebAuthenticationBleSupportName[];
extern const char kEnableWebAuthenticationBleSupportDescription[];

extern const char kEnableWebAuthenticationTestingAPIName[];
extern const char kEnableWebAuthenticationTestingAPIDescription[];

extern const char kEnterpriseReportingInBrowserName[];
extern const char kEnterpriseReportingInBrowserDescription[];

extern const char kHappinessTrackingSurveysForDesktopName[];
extern const char kHappinessTrackingSurveysForDesktopDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kIntentPickerName[];
extern const char kIntentPickerDescription[];

extern const char kKernelnextVMsName[];
extern const char kKernelnextVMsDescription[];

extern const char kMirroringServiceName[];
extern const char kMirroringServiceDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescriptions[];

extern const char kOmniboxExperimentalKeywordModeName[];
extern const char kOmniboxExperimentalKeywordModeDescription[];

extern const char kOmniboxLooseMaxLimitOnDedicatedRowsName[];
extern const char kOmniboxLooseMaxLimitOnDedicatedRowsDescription[];

extern const char kOmniboxPedalSuggestionsName[];
extern const char kOmniboxPedalSuggestionsDescription[];

extern const char kOmniboxReverseAnswersName[];
extern const char kOmniboxReverseAnswersDescription[];

extern const char kOmniboxSuggestionTransparencyOptionsName[];
extern const char kOmniboxSuggestionTransparencyOptionsDescription[];

extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

extern const char kOmniboxTabSwitchSuggestionsDedicatedRowName[];
extern const char kOmniboxTabSwitchSuggestionsDedicatedRowDescription[];

extern const char kTabFreezeName[];
extern const char kTabFreezeDescription[];

extern const char kWebUIA11yEnhancementsName[];
extern const char kWebUIA11yEnhancementsDescription[];

// Only used in branded builds.
extern const char kGoogleBrandedContextMenuName[];
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

extern const char kD3D11VideoDecoderName[];
extern const char kD3D11VideoDecoderDescription[];

extern const char kEnableAuraTooltipsOnWindowsName[];
extern const char kEnableAuraTooltipsOnWindowsDescription[];

extern const char kEnableGpuAppcontainerName[];
extern const char kEnableGpuAppcontainerDescription[];

extern const char kExperimentalFlingAnimationName[];
extern const char kExperimentalFlingAnimationDescription[];

extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

extern const char kUseAngleName[];
extern const char kUseAngleDescription[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];
extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];
extern const char kUseAngleD3D11on12[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

#if BUILDFLAG(ENABLE_SPELLCHECK)
extern const char kWinUseBrowserSpellCheckerName[];
extern const char kWinUseBrowserSpellCheckerDescription[];
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#endif  // defined(OS_WIN)

// Mac ------------------------------------------------------------------------

#if defined(OS_MACOSX)

extern const char kImmersiveFullscreenName[];
extern const char kImmersiveFullscreenDescription[];

extern const char kEnableCustomMacPaperSizesName[];
extern const char kEnableCustomMacPaperSizesDescription[];

extern const char kMacTouchBarName[];
extern const char kMacTouchBarDescription[];

extern const char kMacSyscallSandboxName[];
extern const char kMacSyscallSandboxDescription[];

extern const char kMacV2GPUSandboxName[];
extern const char kMacV2GPUSandboxDescription[];

extern const char kMacViewsTaskManagerName[];
extern const char kMacViewsTaskManagerDescription[];

extern const char kMacSystemMediaPermissionsInfoUiName[];
extern const char kMacSystemMediaPermissionsInfoUiDescription[];

extern const char kMetalName[];
extern const char kMetalDescription[];

// Non-Mac --------------------------------------------------------------------

#else  // !defined(OS_MACOSX)

extern const char kPermissionPromptPersistenceToggleName[];
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // defined(OS_MACOSX)

// Chrome OS ------------------------------------------------------------------

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAggregatedMlAppRankingName[];
extern const char kAggregatedMlAppRankingDescription[];

extern const char kAggregatedMlSearchRankingName[];
extern const char kAggregatedMlSearchRankingDescription[];

extern const char kAllowAmbientEQName[];
extern const char kAllowAmbientEQDescription[];

extern const char kAllowDisableMouseAccelerationName[];
extern const char kAllowDisableMouseAccelerationDescription[];

extern const char kAppServiceAshName[];
extern const char kAppServiceAshDescription[];

extern const char kAppServiceInstanceRegistryName[];
extern const char kAppServiceInstanceRegistryDescription[];

extern const char kAppServiceIntentHandlingName[];
extern const char kAppServiceIntentHandlingDescription[];

extern const char kAppServiceShelfName[];
extern const char kAppServiceShelfDescription[];

extern const char kArcApplicationZoomName[];
extern const char kArcApplicationZoomDescription[];

extern const char kArcBootCompleted[];
extern const char kArcBootCompletedDescription[];

extern const char kArcCupsApiName[];
extern const char kArcCupsApiDescription[];

extern const char kArcCustomTabsExperimentName[];
extern const char kArcCustomTabsExperimentDescription[];

extern const char kArcDocumentsProviderName[];
extern const char kArcDocumentsProviderDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcPrintSpoolerExperimentName[];
extern const char kArcPrintSpoolerExperimentDescription[];

extern const char kArcUsbHostName[];
extern const char kArcUsbHostDescription[];

extern const char kArcUsbStorageUIName[];
extern const char kArcUsbStorageUIDescription[];

extern const char kArcVpnName[];
extern const char kArcVpnDescription[];

extern const char kAshDragWindowFromShelfName[];
extern const char kAshDragWindowFromShelfDescription[];

extern const char kAshEnableDisplayMoveWindowAccelsName[];
extern const char kAshEnableDisplayMoveWindowAccelsDescription[];

extern const char kAshEnableOverviewRoundedCornersName[];
extern const char kAshEnableOverviewRoundedCornersDescription[];

extern const char kAshEnablePersistentWindowBoundsName[];
extern const char kAshEnablePersistentWindowBoundsDescription[];

extern const char kAshEnablePipRoundedCornersName[];
extern const char kAshEnablePipRoundedCornersDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAshNotificationStackingBarRedesignName[];
extern const char kAshNotificationStackingBarRedesignDescription[];

extern const char kAshSwapSideVolumeButtonsForOrientationName[];
extern const char kAshSwapSideVolumeButtonsForOrientationDescription[];

extern const char kAshSwipingFromLeftEdgeToGoBackName[];
extern const char kAshSwipingFromLeftEdgeToGoBackDescription[];

extern const char kBluetoothAggressiveAppearanceFilterName[];
extern const char kBluetoothAggressiveAppearanceFilterDescription[];

extern const char kCameraSystemWebAppName[];
extern const char kCameraSystemWebAppDescription[];

extern const char kCrOSContainerName[];
extern const char kCrOSContainerDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kCrostiniAppSearchName[];
extern const char kCrostiniAppSearchDescription[];

extern const char kCrostiniBackupName[];
extern const char kCrostiniBackupDescription[];

extern const char kCrostiniUseBusterImageName[];
extern const char kCrostiniUseBusterImageDescription[];

extern const char kCrostiniGpuSupportName[];
extern const char kCrostiniGpuSupportDescription[];

extern const char kCrostiniUsbAllowUnsupportedName[];
extern const char kCrostiniUsbAllowUnsupportedDescription[];

extern const char kCrostiniWebUIInstallerName[];
extern const char kCrostiniWebUIInstallerDescription[];

extern const char kCrostiniWebUIUpgraderName[];
extern const char kCrostiniWebUIUpgraderDescription[];

extern const char kCryptAuthV1DeviceSyncDeprecateName[];
extern const char kCryptAuthV1DeviceSyncDeprecateDescription[];

extern const char kCryptAuthV2DeviceActivityStatusName[];
extern const char kCryptAuthV2DeviceActivityStatusDescription[];

extern const char kCryptAuthV2DeviceSyncName[];
extern const char kCryptAuthV2DeviceSyncDescription[];

extern const char kCryptAuthV2EnrollmentName[];
extern const char kCryptAuthV2EnrollmentDescription[];

extern const char kCupsPrintersUiOverhaulName[];
extern const char kCupsPrintersUiOverhaulDescription[];

extern const char kDisableCancelAllTouchesName[];
extern const char kDisableCancelAllTouchesDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kEnableUseHDRTransferFunctionName[];
extern const char kEnableUseHDRTransferFunctionDescription[];

extern const char kDisableOfficeEditingComponentAppName[];
extern const char kDisableOfficeEditingComponentAppDescription[];

extern const char kDisableTabletAutohideTitlebarsName[];
extern const char kDisableTabletAutohideTitlebarsDescription[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kEnableAdvancedPpdAttributesName[];
extern const char kEnableAdvancedPpdAttributesDescription[];

extern const char kEnableAppDataSearchName[];
extern const char kEnableAppDataSearchDescription[];

extern const char kEnableAppGridGhostName[];
extern const char kEnableAppGridGhostDescription[];

extern const char kEnableSearchBoxSelectionName[];
extern const char kEnableSearchBoxSelectionDescription[];

extern const char kEnableAppListSearchAutocompleteName[];
extern const char kEnableAppListSearchAutocompleteDescription[];

extern const char kEnableAppReinstallZeroStateName[];
extern const char kEnableAppReinstallZeroStateDescription[];

extern const char kEnableArcUnifiedAudioFocusName[];
extern const char kEnableArcUnifiedAudioFocusDescription[];

extern const char kEnableAssistantAppSupportName[];
extern const char kEnableAssistantAppSupportDescription[];

extern const char kEnableAssistantLauncherIntegrationName[];
extern const char kEnableAssistantLauncherIntegrationDescription[];

extern const char kEnableAssistantLauncherUIName[];
extern const char kEnableAssistantLauncherUIDescription[];

extern const char kEnableAssistantMediaSessionIntegrationName[];
extern const char kEnableAssistantMediaSessionIntegrationDescription[];

extern const char kEnableAssistantRoutinesName[];
extern const char kEnableAssistantRoutinesDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnableCrOSActionRecorderName[];
extern const char kEnableCrOSActionRecorderDescription[];

extern const char kEnableDiscoverAppName[];
extern const char kEnableDiscoverAppDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

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

extern const char kEnableHomeLauncherName[];
extern const char kEnableHomeLauncherDescription[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableParentalControlsSettingsName[];
extern const char kEnableParentalControlsSettingsDescription[];

extern const char kEnablePlayStoreSearchName[];
extern const char kEnablePlayStoreSearchDescription[];

extern const char kEnableVideoPlayerNativeControlsName[];
extern const char kEnableVideoPlayerNativeControlsDescription[];

extern const char kEnableVirtualDesksName[];
extern const char kEnableVirtualDesksDescription[];

extern const char kTerminalSystemAppName[];
extern const char kTerminalSystemAppDescription[];

extern const char kTerminalSystemAppSplitsName[];
extern const char kTerminalSystemAppSplitsDescription[];

extern const char kTrimOnFreezeName[];
extern const char kTrimOnFreezeDescription[];

extern const char kTrimOnMemoryPressureName[];
extern const char kTrimOnMemoryPressureDescription[];

extern const char kEnableZeroStateSuggestionsName[];
extern const char kEnableZeroStateSuggestionsDescription[];

extern const char kExoPointerLockName[];
extern const char kExoPointerLockDescription[];

extern const char kExperimentalAccessibilityChromeVoxLanguageSwitchingName[];
extern const char
    kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription[];

extern const char
    kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingName[];
extern const char
    kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingDescription[];

extern const char kExperimentalAccessibilitySwitchAccessName[];
extern const char kExperimentalAccessibilitySwitchAccessDescription[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char kFileManagerFeedbackPanelDescription[];
extern const char kFileManagerFeedbackPanelName[];

extern const char kFileManagerPiexWasmName[];
extern const char kFileManagerPiexWasmDescription[];

extern const char kFileManagerTouchModeName[];
extern const char kFileManagerTouchModeDescription[];

extern const char kFilesNGName[];
extern const char kFilesNGDescription[];

extern const char kFirstRunUiTransitionsName[];
extern const char kFirstRunUiTransitionsDescription[];

extern const char kFsNosymfollowName[];
extern const char kFsNosymfollowDescription[];

extern const char kFuzzyAppSearchName[];
extern const char kFuzzyAppSearchDescription[];

extern const char kGaiaActionButtonsName[];
extern const char kGaiaActionButtonsDescription[];

extern const char kHideArcMediaNotificationsName[];
extern const char kHideArcMediaNotificationsDescription[];

extern const char kImeInputLogicFstName[];
extern const char kImeInputLogicFstDescription[];

extern const char kImeNativeDecoderName[];
extern const char kImeNativeDecoderDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kLockScreenMediaControlsName[];
extern const char kLockScreenMediaControlsDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMediaAppName[];
extern const char kMediaAppDescription[];

extern const char kMediaSessionNotificationsName[];
extern const char kMediaSessionNotificationsDescription[];

extern const char kNetworkPortalNotificationName[];
extern const char kNetworkPortalNotificationDescription[];

extern const char kNewOverviewTabletLayoutName[];
extern const char kNewOverviewTabletLayoutDescription[];

extern const char kNewZipUnpackerName[];
extern const char kNewZipUnpackerDescription[];

extern const char kPrinterProviderSearchAppName[];
extern const char kPrinterProviderSearchAppDescription[];

extern const char kPrintServerUiName[];
extern const char kPrintServerUiDescription[];

extern const char kReduceDisplayNotificationsName[];
extern const char kReduceDisplayNotificationsDescription[];

extern const char kReleaseNotesName[];
extern const char kReleaseNotesDescription[];

extern const char kReleaseNotesNotificationName[];
extern const char kReleaseNotesNotificationDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kShowBluetoothDebugLogToggleName[];
extern const char kShowBluetoothDebugLogToggleDescription[];

extern const char kShowBluetoothDeviceBatteryName[];
extern const char kShowBluetoothDeviceBatteryDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSmartDimModelV3Name[];
extern const char kSmartDimModelV3Description[];

extern const char kSmartTextSelectionName[];
extern const char kSmartTextSelectionDescription[];

extern const char kSplitSettingsName[];
extern const char kSplitSettingsDescription[];

extern const char kSplitSettingsSyncName[];
extern const char kSplitSettingsSyncDescription[];

extern const char kStreamlinedUsbPrinterSetupName[];
extern const char kStreamlinedUsbPrinterSetupDescription[];

extern const char kSyncWifiConfigurationsName[];
extern const char kSyncWifiConfigurationsDescription[];

extern const char kMessageCenterRedesignName[];
extern const char kMessageCenterRedesignDescription[];

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

extern const char kUsbguardName[];
extern const char kUsbguardDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kUseMonitorColorSpaceName[];
extern const char kUseMonitorColorSpaceDescription[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVaapiWebPImageDecodeAccelerationName[];
extern const char kVaapiWebPImageDecodeAccelerationDescription[];

extern const char kVirtualKeyboardBorderedKeyName[];
extern const char kVirtualKeyboardBorderedKeyDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kWakeOnPacketsName[];
extern const char kWakeOnPacketsDescription[];

extern const char kZeroStateFilesName[];
extern const char kZeroStateFilesDescription[];

// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if BUILDFLAG(USE_TCMALLOC)
extern const char kDynamicTcmallocName[];
extern const char kDynamicTcmallocDescription[];
#endif  // BUILDFLAG(USE_TCMALLOC)

#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)

extern const char kEnableMDRoundedCornersOnDialogsName[];
extern const char kEnableMDRoundedCornersOnDialogsDescription[];

extern const char kInstallableInkDropName[];
extern const char kInstallableInkDropDescription[];

extern const char kReopenTabInProductHelpName[];
extern const char kReopenTabInProductHelpDescription[];

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

extern const char kWebGL2ComputeContextName[];
extern const char kWebGL2ComputeContextDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)

extern const char kClickToCallContextMenuForSelectedTextName[];
extern const char kClickToCallContextMenuForSelectedTextDescription[];

extern const char kClickToCallUIName[];
extern const char kClickToCallUIDescription[];

extern const char kClickToCallDetectionV2Name[];
extern const char kClickToCallDetectionV2Description[];

#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

extern const char kRemoteCopyReceiverName[];
extern const char kRemoteCopyReceiverDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

extern const char kDirectManipulationStylusName[];
extern const char kDirectManipulationStylusDescription[];

extern const char kAnimatedAvatarButtonName[];
extern const char kAnimatedAvatarButtonDescription[];

extern const char kProfileMenuRevampName[];
extern const char kProfileMenuRevampDescription[];

extern const char kSyncClipboardServiceName[];
extern const char kSyncClipboardServiceDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)

extern const char kForceEnableSystemAecName[];
extern const char kForceEnableSystemAecDescription[];

#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

extern const char kWebContentsOcclusionName[];
extern const char kWebContentsOcclusionDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_VR)

extern const char kWebXrOrientationSensorDeviceName[];
extern const char kWebXrOrientationSensorDeviceDescription[];

#if BUILDFLAG(ENABLE_OCULUS_VR)
extern const char kOculusVRName[];
extern const char kOculusVRDescription[];
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
extern const char kOpenVRName[];
extern const char kOpenVRDescription[];
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
extern const char kWindowsMixedRealityName[];
extern const char kWindowsMixedRealityDescription[];
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_OPENXR)
extern const char kOpenXRName[];
extern const char kOpenXRDescription[];
#endif  // ENABLE_OPENXR

#if !defined(OS_ANDROID)
extern const char kXRSandboxName[];
extern const char kXRSandboxDescription[];
#endif  // !defined(OS_ANDROID)

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_NACL)
extern const char kNaclName[];
extern const char kNaclDescription[];
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)

extern const char kPdfAnnotations[];
extern const char kPdfAnnotationsDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kPdfFormSaveName[];
extern const char kPdfFormSaveDescription[];

#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kPaintHoldingName[];
extern const char kPaintHoldingDescription[];

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
