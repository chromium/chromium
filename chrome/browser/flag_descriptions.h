// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/check_op.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

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

extern const char kAllowPopupsDuringPageUnloadName[];
extern const char kAllowPopupsDuringPageUnloadDescription[];

extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[];
extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[];

extern const char kAllowSyncXHRInPageDismissalName[];
extern const char kAllowSyncXHRInPageDismissalDescription[];

extern const char kConversionMeasurementApiName[];
extern const char kConversionMeasurementApiDescription[];

extern const char kConversionMeasurementDebugModeName[];
extern const char kConversionMeasurementDebugModeDescription[];

extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

extern const char kEnableBloomName[];
extern const char kEnableBloomDescription[];

extern const char kEnableBluetoothSerialPortProfileInSerialApiName[];
extern const char kEnableBluetoothSerialPortProfileInSerialApiDescription[];

extern const char kEnableClipboardProviderImageSuggestionsName[];
extern const char kEnableClipboardProviderImageSuggestionsDescription[];

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

extern const char kAutofillEnableCardNicknameManagementName[];
extern const char kAutofillEnableCardNicknameManagementDescription[];

extern const char kAutofillEnableCardNicknameUpstreamName[];
extern const char kAutofillEnableCardNicknameUpstreamDescription[];

extern const char kAutofillEnableGoogleIssuedCardName[];
extern const char kAutofillEnableGoogleIssuedCardDescription[];

extern const char kAutofillEnableOffersInDownstreamName[];
extern const char kAutofillEnableOffersInDownstreamDescription[];

extern const char kAutofillEnableStickyPaymentsBubbleName[];
extern const char kAutofillEnableStickyPaymentsBubbleDescription[];

extern const char kAutofillEnableToolbarStatusChipName[];
extern const char kAutofillEnableToolbarStatusChipDescription[];

extern const char kAutofillEnableVirtualCardName[];
extern const char kAutofillEnableVirtualCardDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

extern const char kAutofillOffNoServerDataName[];
extern const char kAutofillOffNoServerDataDescription[];

extern const char kAutofillProfileClientValidationName[];
extern const char kAutofillProfileClientValidationDescription[];

extern const char kAutofillProfileServerValidationName[];
extern const char kAutofillProfileServerValidationDescription[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

extern const char kShelfAppScalingName[];
extern const char kShelfAppScalingDescription[];

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

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kContextMenuGoogleLensChipName[];
extern const char kContextMenuGoogleLensChipDescription[];

extern const char kContextMenuSearchWithGoogleLensName[];
extern const char kContextMenuSearchWithGoogleLensDescription[];

extern const char kContextMenuShopWithGoogleLensName[];
extern const char kContextMenuShopWithGoogleLensDescription[];

extern const char kContextMenuSearchAndShopWithGoogleLensName[];
extern const char kContextMenuSearchAndShopWithGoogleLensDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kClientStorageAccessContextAuditingName[];
extern const char kClientStorageAccessContextAuditingDescription[];

extern const char kConditionalTabStripAndroidName[];
extern const char kConditionalTabStripAndroidDescription[];

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

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileSCRGBLinear[];
extern const char kForceColorProfileHDR10[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kForcePreferredIntervalForVideoName[];
extern const char kForcePreferredIntervalForVideoDescription[];

extern const char kDynamicColorGamutName[];
extern const char kDynamicColorGamutDescription[];

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

extern const char kForceEnableDevicesPageName[];
extern const char kForceEnableDevicesPageDescription[];

extern const char kDisableBestEffortTasksName[];
extern const char kDisableBestEffortTasksDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kEnableAccessibilityObjectModelName[];
extern const char kEnableAccessibilityObjectModelDescription[];

extern const char kEnableAmbientAuthenticationInIncognitoName[];
extern const char kEnableAmbientAuthenticationInIncognitoDescription[];

extern const char kEnableAmbientAuthenticationInGuestSessionName[];
extern const char kEnableAmbientAuthenticationInGuestSessionDescription[];

extern const char kEnableAudioFocusEnforcementName[];
extern const char kEnableAudioFocusEnforcementDescription[];

extern const char kEnableAutofillAccountWalletStorageName[];
extern const char kEnableAutofillAccountWalletStorageDescription[];

extern const char kEnableAutofillCacheServerCardInfoName[];
extern const char kEnableAutofillCacheServerCardInfoDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardAuthenticationName[];
extern const char kEnableAutofillCreditCardAuthenticationDescription[];

extern const char kEnableAutofillCreditCardCvcPromptGoogleLogoName[];
extern const char kEnableAutofillCreditCardCvcPromptGoogleLogoDescription[];

extern const char kEnableAutofillCreditCardUploadFeedbackName[];
extern const char kEnableAutofillCreditCardUploadFeedbackDescription[];

extern const char kEnableExperimentalCookieFeaturesName[];
extern const char kEnableExperimentalCookieFeaturesDescription[];

extern const char kEnableDeferAllScriptName[];
extern const char kEnableDeferAllScriptDescription[];

extern const char kEnableDeferAllScriptWithoutOptimizationHintsName[];
extern const char kEnableDeferAllScriptWithoutOptimizationHintsDescription[];

extern const char kEnableSaveDataName[];
extern const char kEnableSaveDataDescription[];

extern const char kEnableNavigationPredictorName[];
extern const char kEnableNavigationPredictorDescription[];

extern const char kEnablePreconnectToSearchName[];
extern const char kEnablePreconnectToSearchDescription[];

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
extern const char kEnableDataReductionProxyServerExperimentName[];
extern const char kEnableDataReductionProxyServerExperimentDescription[];

extern const char kColorProviderRedirectionName[];
extern const char kColorProviderRedirectionDescription[];

extern const char kDesktopPWAsLocalUpdatingName[];
extern const char kDesktopPWAsLocalUpdatingDescription[];

extern const char kDesktopPWAsLocalUpdatingThrottlePersistenceName[];
extern const char kDesktopPWAsLocalUpdatingThrottlePersistenceDescription[];

extern const char kDesktopPWAsMigrationUserDisplayModeCleanUpName[];
extern const char kDesktopPWAsMigrationUserDisplayModeCleanUpDescription[];

extern const char kDesktopPWAsAppIconShortcutsMenuName[];
extern const char kDesktopPWAsAppIconShortcutsMenuDescription[];

extern const char kDesktopPWAsTabStripName[];
extern const char kDesktopPWAsTabStripDescription[];

extern const char kDesktopPWAsTabStripLinkCapturingName[];
extern const char kDesktopPWAsTabStripLinkCapturingDescription[];

extern const char kDesktopPWAsWithoutExtensionsName[];
extern const char kDesktopPWAsWithoutExtensionsDescription[];

extern const char kDesktopPWAsRunOnOsLoginName[];
extern const char kDesktopPWAsRunOnOsLoginDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription[];

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

extern const char kMaintainShelfStateWhenEnteringOverviewName[];
extern const char kMaintainShelfStateWhenEnteringOverviewDescription[];

extern const char kWinrtGeolocationImplementationName[];
extern const char kWinrtGeolocationImplementationDescription[];

extern const char kWinrtSensorsImplementationName[];
extern const char kWinrtSensorsImplementationDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableImplicitRootScrollerName[];
extern const char kEnableImplicitRootScrollerDescription[];

extern const char kEnableCSSOMViewScrollCoordinatesName[];
extern const char kEnableCSSOMViewScrollCoordinatesDescription[];

extern const char kEnableLayoutNGName[];
extern const char kEnableLayoutNGDescription[];

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

extern const char kCorsForContentScriptsName[];
extern const char kCorsForContentScriptsDescription[];
extern const char kForceEmptyCorbAndCorsAllowlistName[];
extern const char kForceEmptyCorbAndCorsAllowlistDescription[];

extern const char kCrossOriginOpenerPolicyReportingName[];
extern const char kCrossOriginOpenerPolicyReportingDescription[];

extern const char kCrossOriginOpenerPolicyAccessReportingName[];
extern const char kCrossOriginOpenerPolicyAccessReportingDescription[];

extern const char kCrossOriginIsolatedName[];
extern const char kCrossOriginIsolatedDescription[];

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

extern const char kDuetTabStripIntegrationAndroidName[];
extern const char kDuetTabStripIntegrationAndroidDescription[];

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePortalsName[];
extern const char kEnablePortalsDescription[];

extern const char kEnablePortalsCrossOriginName[];
extern const char kEnablePortalsCrossOriginDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnablePreviewsCoinFlipName[];
extern const char kEnablePreviewsCoinFlipDescription[];

extern const char kEnableSRPIsolatedPrerendersName[];
extern const char kEnableSRPIsolatedPrerendersDescription[];

extern const char kEnableSRPIsolatedPrerenderProbingName[];
extern const char kEnableSRPIsolatedPrerenderProbingDescription[];

extern const char kEnableSRPIsolatedPrerendersNSPName[];
extern const char kEnableSRPIsolatedPrerendersNSPDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];

extern const char kEnableResourceLoadingHintsName[];
extern const char kEnableResourceLoadingHintsDescription[];

extern const char kEnableSubresourceRedirectName[];
extern const char kEnableSubresourceRedirectDescription[];

#if BUILDFLAG(ENABLE_TAB_SEARCH)
extern const char kEnableTabSearchName[];
extern const char kEnableTabSearchDescription[];
extern const char kEnableTabSearchFixedEntrypointName[];
extern const char kEnableTabSearchFixedEntrypointDescription[];
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)

extern const char kEnableTextFragmentAnchorName[];
extern const char kEnableTextFragmentAnchorDescription[];

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

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalFlingAnimationName[];
extern const char kExperimentalFlingAnimationDescription[];

extern const char kExperimentalProductivityFeaturesName[];
extern const char kExperimentalProductivityFeaturesDescription[];

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

extern const char kFirstScrollLatencyMeasurementName[];
extern const char kFirstScrollLatencyMeasurementDescription[];

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

extern const char kColorPickerEyeDropperName[];
extern const char kColorPickerEyeDropperDescription[];

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

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];

extern const char kGuestOsExternalProtocolName[];
extern const char kGuestOsExternalProtocolDescription[];

extern const char kH264DecoderBufferIsCompleteFrameName[];
extern const char kH264DecoderBufferIsCompleteFrameDescription[];

extern const char kHandwritingGestureName[];
extern const char kHandwritingGestureDescription[];

extern const char kHandwritingGestureEditingName[];
extern const char kHandwritingGestureEditingDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHeavyAdPrivacyMitigationsName[];
extern const char kHeavyAdPrivacyMitigationsDescription[];

extern const char kHeavyAdInterventionName[];
extern const char kHeavyAdInterventionDescription[];

extern const char kHorizontalTabSwitcherAndroidName[];
extern const char kHorizontalTabSwitcherAndroidDescription[];

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

extern const char kIgnorePreviewsBlocklistName[];
extern const char kIgnorePreviewsBlocklistDescription[];

extern const char kImprovedCookieControlsName[];
extern const char kImprovedCookieControlsDescription[];

extern const char kImprovedCookieControlsForThirdPartyCookieBlockingName[];
extern const char
    kImprovedCookieControlsForThirdPartyCookieBlockingDescription[];

extern const char kCompositorThreadedScrollbarScrollingName[];
extern const char kCompositorThreadedScrollbarScrollingDescription[];

extern const char kImpulseScrollAnimationsName[];
extern const char kImpulseScrollAnimationsDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kInstalledAppsInCbdName[];
extern const char kInstalledAppsInCbdDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kKaleidoscopeModuleName[];
extern const char kKaleidoscopeModuleDescription[];

extern const char kLauncherSettingsSearchName[];
extern const char kLauncherSettingsSearchDescription[];

extern const char kLegacyTLSEnforcedName[];
extern const char kLegacyTLSEnforcedDescription[];

extern const char kLegacyTLSWarningsName[];
extern const char kLegacyTLSWarningsDescription[];

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

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];
extern const char kMarkHttpAsWarning[];
extern const char kMarkHttpAsWarningAndDangerousOnFormEdits[];

extern const char kMediaHistoryName[];
extern const char kMediaHistoryDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMetricsSettingsAndroidName[];
extern const char kMetricsSettingsAndroidDescription[];

extern const char kMixedFormsDisableAutofillName[];
extern const char kMixedFormsDisableAutofillDescription[];

extern const char kMixedFormsInterstitialName[];
extern const char kMixedFormsInterstitialDescription[];

extern const char kMobileIdentityConsistencyName[];
extern const char kMobileIdentityConsistencyDescription[];

extern const char kMouseSubframeNoImplicitCaptureName[];
extern const char kMouseSubframeNoImplicitCaptureDescription[];

extern const char kNearbySharingName[];
extern const char kNearbySharingDescription[];

extern const char kUsernameFirstFlowName[];
extern const char kUsernameFirstFlowDescription[];

extern const char kNewCanvas2DAPIName[];
extern const char kNewCanvas2DAPIDescription[];

extern const char kNewProfilePickerName[];
extern const char kNewProfilePickerDescription[];

extern const char kProfilesUIRevampName[];
extern const char kProfilesUIRevampDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

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

extern const char kOmniboxDeferredKeyboardPopupName[];
extern const char kOmniboxDeferredKeyboardPopupDescription[];

extern const char kOmniboxDisableInstantExtendedLimitName[];
extern const char kOmniboxDisableInstantExtendedLimitDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxExperimentalSuggestScoringName[];
extern const char kOmniboxExperimentalSuggestScoringDescription[];

extern const char
    kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsName[];
extern const char
    kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsDescription[];
extern const char kOmniboxHistoryQuickProviderAllowMidwordContinuationsName[];
extern const char
    kOmniboxHistoryQuickProviderAllowMidwordContinuationsDescription[];

extern const char kOmniboxLocalZeroSuggestFrecencyRankingName[];
extern const char kOmniboxLocalZeroSuggestFrecencyRankingDescription[];

extern const char kOmniboxMostVisitedTilesName[];
extern const char kOmniboxMostVisitedTilesDescription[];

extern const char kOmniboxRichAutocompletionName[];
extern const char kOmniboxRichAutocompletionDescription[];
extern const char kOmniboxRichAutocompletionMinCharName[];
extern const char kOmniboxRichAutocompletionMinCharDescription[];
extern const char kOmniboxRichAutocompletionShowAdditionalTextName[];
extern const char kOmniboxRichAutocompletionShowAdditionalTextDescription[];
extern const char kOmniboxRichAutocompletionSplitName[];
extern const char kOmniboxRichAutocompletionSplitDescription[];
extern const char kOmniboxRichAutocompletionPromisingName[];
extern const char kOmniboxRichAutocompletionPromisingDescription[];

extern const char kOmniboxRichEntitiesInLauncherName[];
extern const char kOmniboxRichEntitiesInLauncherDescription[];

extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

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

extern const char kOmniboxSuggestionsRecyclerViewName[];
extern const char kOmniboxSuggestionsRecyclerViewDescription[];

extern const char kOmniboxSuggestionsWrapAroundName[];
extern const char kOmniboxSuggestionsWrapAroundDescription[];

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

extern const char kOmniboxBubbleUrlSuggestionsName[];
extern const char kOmniboxBubbleUrlSuggestionsDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

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

extern const char kOopRasterizationDDLName[];
extern const char kOopRasterizationDDLDescription[];

extern const char kOsSettingsDeepLinkingName[];
extern const char kOsSettingsDeepLinkingDescription[];

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

extern const char kOverrideLanguagePrefsForHrefTranslateName[];
extern const char kOverrideLanguagePrefsForHrefTranslateDescription[];
extern const char kOverrideSitePrefsForHrefTranslateName[];
extern const char kOverrideSitePrefsForHrefTranslateDescription[];

extern const char kUpdateHoverAtBeginFrameName[];
extern const char kUpdateHoverAtBeginFrameDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

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

extern const char kForceWebContentsDarkModeName[];
extern const char kForceWebContentsDarkModeDescription[];

extern const char kForcedColorsName[];
extern const char kForcedColorsDescription[];

extern const char kPercentBasedScrollingName[];
extern const char kPercentBasedScrollingDescription[];

extern const char kPermissionChipName[];
extern const char kPermissionChipDescription[];

extern const char kPointerLockOptionsName[];
extern const char kPointerLockOptionsDescription[];

extern const char kPreviewsAllowedName[];
extern const char kPreviewsAllowedDescription[];

extern const char kPrintJobManagementAppName[];
extern const char kPrintJobManagementAppDescription[];

extern const char kPrintSaveToDriveName[];
extern const char kPrintSaveToDriveDescription[];

extern const char kPrivacyElevatedAndroidName[];
extern const char kPrivacyElevatedAndroidDescription[];

extern const char kPrivacyReorderedAndroidName[];
extern const char kPrivacyReorderedAndroidDescription[];

extern const char kPrivacySettingsRedesignName[];
extern const char kPrivacySettingsRedesignDescription[];

extern const char kSafetyCheckAndroidName[];
extern const char kSafetyCheckAndroidDescription[];

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

extern const char kReadLaterName[];
extern const char kReadLaterDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

extern const char kRewriteLevelDBOnDeletionName[];
extern const char kRewriteLevelDBOnDeletionDescription[];

extern const char kRequestUnbufferedDispatchName[];
extern const char kRequestUnbufferedDispatchDescription[];

extern const char kRestrictGamepadAccessName[];
extern const char kRestrictGamepadAccessDescription[];

extern const char kDelayAsyncScriptExecutionName[];
extern const char kDelayAsyncScriptExecutionDescription[];

extern const char kDelayCompetingLowPriorityRequestsName[];
extern const char kDelayCompetingLowPriorityRequestsDescription[];

extern const char kIntensiveWakeUpThrottlingName[];
extern const char kIntensiveWakeUpThrottlingDescription[];

extern const char kPrinterStatusName[];
extern const char kPrinterStatusDescription[];

extern const char kPrinterStatusDialogName[];
extern const char kPrinterStatusDialogDescription[];

extern const char kSafeBrowsingEnhancedProtectionMessageInInterstitialsName[];
extern const char
    kSafeBrowsingEnhancedProtectionMessageInInterstitialsDescription[];

extern const char kSafetyTipName[];
extern const char kSafetyTipDescription[];

extern const char kSameSiteByDefaultCookiesName[];
extern const char kSameSiteByDefaultCookiesDescription[];

extern const char kSchemefulSameSiteName[];
extern const char kSchemefulSameSiteDescription[];

extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kScrollUnificationName[];
extern const char kScrollUnificationDescription[];

extern const char kSecurePaymentConfirmationDebugName[];
extern const char kSecurePaymentConfirmationDebugDescription[];

extern const char kSendTabToSelfOmniboxSendingAnimationName[];
extern const char kSendTabToSelfOmniboxSendingAnimationDescription[];

extern const char kServiceWorkerOnUIName[];
extern const char kServiceWorkerOnUIDescription[];

extern const char kSharedClipboardUIName[];
extern const char kSharedClipboardUIDescription[];

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

extern const char kShelfHotseatName[];
extern const char kShelfHotseatDescription[];

extern const char kShelfHoverPreviewsName[];
extern const char kShelfHoverPreviewsDescription[];

extern const char kShowAndroidFilesInFilesAppName[];
extern const char kShowAndroidFilesInFilesAppDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

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

extern const char kSiteIsolationOptOutName[];
extern const char kSiteIsolationOptOutDescription[];
extern const char kSiteIsolationOptOutChoiceDefault[];
extern const char kSiteIsolationOptOutChoiceOptOut[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSmsReceiverCrossDeviceName[];
extern const char kSmsReceiverCrossDeviceDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kStopInBackgroundName[];
extern const char kStopInBackgroundDescription[];

extern const char kStopNonTimersInBackgroundName[];
extern const char kStopNonTimersInBackgroundDescription[];

extern const char kStoragePressureEventName[];
extern const char kStoragePressureEventDescription[];

extern const char kStoragePressureUIName[];
extern const char kStoragePressureUIDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSuggestedContentToggleName[];
extern const char kSuggestedContentToggleDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncAutofillWalletOfferDataName[];
extern const char kSyncAutofillWalletOfferDataDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSystemTrayMicGainName[];
extern const char kSystemTrayMicGainDescription[];

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

extern const char kTabGroupsAutoCreateName[];
extern const char kTabGroupsAutoCreateDescription[];

extern const char kTabGroupsCollapseName[];
extern const char kTabGroupsCollapseDescription[];

extern const char kTabGroupsCollapseFreezingName[];
extern const char kTabGroupsCollapseFreezingDescription[];

extern const char kTabGroupsFeedbackName[];
extern const char kTabGroupsFeedbackDescription[];

extern const char kTabHoverCardsName[];
extern const char kTabHoverCardsDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabOutlinesInLowContrastThemesName[];
extern const char kTabOutlinesInLowContrastThemesDescription[];

extern const char kTintCompositedContentName[];
extern const char kTintCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

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

extern const char kUsePreferredIntervalForVideoName[];
extern const char kUsePreferredIntervalForVideoDescription[];

extern const char kUseSearchClickForRightClickName[];
extern const char kUseSearchClickForRightClickDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebBluetoothNewPermissionsBackendName[];
extern const char kWebBluetoothNewPermissionsBackendDescription[];

extern const char kWebBundlesName[];
extern const char kWebBundlesDescription[];

extern const char kWebOtpBackendName[];
extern const char kWebOtpBackendDescription[];
extern const char kWebOtpBackendSmsVerification[];
extern const char kWebOtpBackendUserConsent[];

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
extern const char kWebXrRuntimeChoiceWindowsMixedReality[];

extern const char kWebXrIncubationsName[];
extern const char kWebXrIncubationsDescription[];

extern const char kWebXrMultiGpuName[];
extern const char kWebXrMultiGpuDescription[];

extern const char kWellKnownChangePasswordName[];
extern const char kWellKnownChangePasswordDescription[];

extern const char kWindowNamingName[];
extern const char kWindowNamingDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAImageReaderName[];
extern const char kAImageReaderDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidDefaultBrowserPromoName[];
extern const char kAndroidDefaultBrowserPromoDescription[];

extern const char kAndroidDetailedLanguageSettingsName[];
extern const char kAndroidDetailedLanguageSettingsDescription[];

extern const char kAndroidManagedByMenuItemName[];
extern const char kAndroidManagedByMenuItemDescription[];

extern const char kAndroidMultipleDisplayName[];
extern const char kAndroidMultipleDisplayDescription[];

extern const char kAndroidPartnerCustomizationPhenotypeName[];
extern const char kAndroidPartnerCustomizationPhenotypeDescription[];

extern const char kAndroidSurfaceControlName[];
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

extern const char kBentoOfflineName[];
extern const char kBentoOfflineDescription[];

extern const char kCCTIncognitoName[];
extern const char kCCTIncognitoDescription[];

extern const char kCCTTargetTranslateLanguageName[];
extern const char kCCTTargetTranslateLanguageDescription[];

extern const char kShareButtonInTopToolbarName[];
extern const char kShareButtonInTopToolbarDescription[];

extern const char kChromeShareHighlightsAndroidName[];
extern const char kChromeShareHighlightsAndroidDescription[];

extern const char kChromeShareQRCodeName[];
extern const char kChromeShareQRCodeDescription[];

extern const char kChromeShareScreenshotName[];
extern const char kChromeShareScreenshotDescription[];

extern const char kChromeSharingHubName[];
extern const char kChromeSharingHubDescription[];

extern const char kChromeSharingHubV15Name[];
extern const char kChromeSharingHubV15Description[];

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

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableUseAaudioDriverName[];
extern const char kEnableUseAaudioDriverDescription[];

extern const char kEphemeralTabUsingBottomSheetName[];
extern const char kEphemeralTabUsingBottomSheetDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kFillingPasswordsFromAnyOriginName[];
extern const char kFillingPasswordsFromAnyOriginDescription[];

extern const char kGamesHubName[];
extern const char kGamesHubDescription[];

extern const char kHomepageLocationName[];
extern const char kHomepageLocationDescription[];

extern const char kHomepagePromoCardName[];
extern const char kHomepagePromoCardDescription[];

extern const char kHomepageSettingsUIConversionName[];
extern const char kHomepageSettingsUIConversionDescription[];

extern const char kInstantStartName[];
extern const char kInstantStartDescription[];

extern const char kIntentBlockExternalFormRedirectsNoGestureName[];
extern const char kIntentBlockExternalFormRedirectsNoGestureDescription[];

extern const char kInterestFeedContentSuggestionsName[];
extern const char kInterestFeedContentSuggestionsDescription[];

extern const char kInterestFeedV2Name[];
extern const char kInterestFeedV2Description[];

extern const char kInterestFeedFeedbackName[];
extern const char kInterestFeedFeedbackDescription[];

extern const char InterestFeedV1ClickAndViewActionsConditionalUploadName[];
extern const char
    InterestFeedV1ClickAndViewActionsConditionalUploadDescription[];

extern const char InterestFeedV2ClickAndViewActionsConditionalUploadName[];
extern const char
    InterestFeedV2ClickAndViewActionsConditionalUploadDescription[];

extern const char kMessagesForAndroidInfrastructureName[];
extern const char kMessagesForAndroidInfrastructureDescription[];

extern const char kMessagesForAndroidPasswordsName[];
extern const char kMessagesForAndroidPasswordsDescription[];

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

extern const char kPageInfoPerformanceHintsName[];
extern const char kPageInfoPerformanceHintsDescription[];

extern const char kPageInfoV2Name[];
extern const char kPageInfoV2Description[];

extern const char kPasswordCheckName[];
extern const char kPasswordCheckDescription[];

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

extern const char kReengagementNotificationName[];
extern const char kReengagementNotificationDescription[];

extern const char kRelatedSearchesName[];
extern const char kRelatedSearchesDescription[];

extern const char kReportFeedUserActionsName[];
extern const char kReportFeedUserActionsDescription[];

extern const char kSafeBrowsingClientSideDetectionAndroidName[];
extern const char kSafeBrowsingClientSideDetectionAndroidDescription[];

extern const char kSafeBrowsingEnhancedProtectionAndroidName[];
extern const char kSafeBrowsingEnhancedProtectionAndroidDescription[];

extern const char kEnhancedProtectionPromoAndroidName[];
extern const char kEnhancedProtectionPromoAndroidDescription[];

extern const char kSafeBrowsingSecuritySectionUiAndroidName[];
extern const char kSafeBrowsingSecuritySectionUiAndroidDescription[];

extern const char kSafeBrowsingUseLocalBlacklistsV2Name[];
extern const char kSafeBrowsingUseLocalBlacklistsV2Description[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kShoppingAssistName[];
extern const char kShoppingAssistDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kSmartSuggestionForLargeDownloadsName[];
extern const char kSmartSuggestionForLargeDownloadsDescription[];

extern const char kStartSurfaceAndroidName[];
extern const char kStartSurfaceAndroidDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

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
extern const char kUpdateNotificationServiceImmediateShowOptionName[];
extern const char kUpdateNotificationServiceImmediateShowOptionDescription[];

extern const char kVideoTutorialsName[];
extern const char kVideoTutorialsDescription[];

extern const char kPrefetchNotificationSchedulingIntegrationName[];
extern const char kPrefetchNotificationSchedulingIntegrationDescription[];

extern const char kUsageStatsDescription[];
extern const char kUsageStatsName[];

extern const char kUseChimeAndroidSdkDescription[];
extern const char kUseChimeAndroidSdkName[];

extern const char kInlineUpdateFlowName[];
extern const char kInlineUpdateFlowDescription[];

extern const char kAndroidDarkSearchName[];
extern const char kAndroidDarkSearchDescription[];

extern const char kAndroidNightModeTabReparentingName[];
extern const char kAndroidNightModeTabReparentingDescription[];

extern const char kTabbedAppOverflowMenuIconsName[];
extern const char kTabbedAppOverflowMenuIconsDescription[];

extern const char kTabbedAppOverflowMenuRegroupName[];
extern const char kTabbedAppOverflowMenuRegroupDescription[];

extern const char kTabbedAppOverflowMenuThreeButtonActionbarName[];
extern const char kTabbedAppOverflowMenuThreeButtonActionbarDescription[];

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAllowAllSitesToInitiateMirroringName[];
extern const char kAllowAllSitesToInitiateMirroringDescription[];

extern const char kEnableAccessibilityLiveCaptionsName[];
extern const char kEnableAccessibilityLiveCaptionsDescription[];

extern const char kCastMediaRouteProviderName[];
extern const char kCastMediaRouteProviderDescription[];

extern const char kCopyLinkToTextName[];
extern const char kCopyLinkToTextDescription[];

extern const char kGlobalMediaControlsCastStartStopName[];
extern const char kGlobalMediaControlsCastStartStopDescription[];

extern const char kMuteNotificationsDuringScreenShareName[];
extern const char kMuteNotificationsDuringScreenShareDescription[];

extern const char kNtpDismissPromosName[];
extern const char kNtpDismissPromosDescription[];

extern const char kNtpIframeOneGoogleBarName[];
extern const char kNtpIframeOneGoogleBarDescription[];

extern const char kNtpOneGoogleBarModalOverlaysName[];
extern const char kNtpOneGoogleBarModalOverlaysDescription[];

extern const char kNtpRealboxName[];
extern const char kNtpRealboxDescription[];

extern const char kNtpRealboxMatchOmniboxThemeName[];
extern const char kNtpRealboxMatchOmniboxThemeDescription[];

extern const char kNtpRepeatableQueriesName[];
extern const char kNtpRepeatableQueriesDescription[];

extern const char kNtpWebUIName[];
extern const char kNtpWebUIDescription[];

extern const char kNtpModulesName[];
extern const char kNtpModulesDescription[];

extern const char kNtpShoppingTasksModuleName[];
extern const char kNtpShoppingTasksModuleDescription[];

extern const char kEnableReaderModeName[];
extern const char kEnableReaderModeDescription[];

extern const char kHappinessTrackingSurveysForDesktopName[];
extern const char kHappinessTrackingSurveysForDesktopDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kHappinessTrackingSurveysForDesktopSettingsName[];
extern const char kHappinessTrackingSurveysForDesktopSettingsDescription[];

extern const char kHappinessTrackingSurveysForDesktopSettingsPrivacyName[];
extern const char
    kHappinessTrackingSurveysForDesktopSettingsPrivacyDescription[];

extern const char kHappinessTrackingSurveysForDesktopMigrationName[];
extern const char kHappinessTrackingSurveysForDesktopMigrationDescription[];

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

extern const char kOmniboxKeywordSearchButtonName[];
extern const char kOmniboxKeywordSearchButtonDescription[];

extern const char kOmniboxRefinedFocusStateName[];
extern const char kOmniboxRefinedFocusStateDescription[];

extern const char kPasswordsWeaknessCheckName[];
extern const char kPasswordsWeaknessCheckDescription[];

extern const char kSCTAuditingName[];
extern const char kSCTAuditingDescription[];

extern const char kTabFreezeName[];
extern const char kTabFreezeDescription[];

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

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

extern const char kUseSerialBusEnumeratorName[];
extern const char kUseSerialBusEnumeratorDescription[];

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

extern const char kImmersiveFullscreenName[];
extern const char kImmersiveFullscreenDescription[];

extern const char kMacSyscallSandboxName[];
extern const char kMacSyscallSandboxDescription[];

extern const char kMacV2GPUSandboxName[];
extern const char kMacV2GPUSandboxDescription[];

extern const char kMacSystemMediaPermissionsInfoUiName[];
extern const char kMacSystemMediaPermissionsInfoUiDescription[];

extern const char kMetalName[];
extern const char kMetalDescription[];

extern const char kVideoToolboxVp9DecodingName[];
extern const char kVideoToolboxVp9DecodingDescription[];

#endif  // defined(OS_MAC)

// Chrome OS ------------------------------------------------------------------

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAggregatedMlAppRankingName[];
extern const char kAggregatedMlAppRankingDescription[];

extern const char kAllowDisableMouseAccelerationName[];
extern const char kAllowDisableMouseAccelerationDescription[];

extern const char kAllowScrollSettingsName[];
extern const char kAllowScrollSettingsDescription[];

extern const char kAppServiceAdaptiveIconName[];
extern const char kAppServiceAdaptiveIconDescription[];

extern const char kAppServiceExternalProtocolName[];
extern const char kAppServiceExternalProtocolDescription[];

extern const char kAppServiceIntentHandlingName[];
extern const char kAppServiceIntentHandlingDescription[];

extern const char kArcApplicationZoomName[];
extern const char kArcApplicationZoomDescription[];

extern const char kArcCustomTabsExperimentName[];
extern const char kArcCustomTabsExperimentDescription[];

extern const char kArcDocumentsProviderName[];
extern const char kArcDocumentsProviderDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcNativeBridge64BitSupportExperimentName[];
extern const char kArcNativeBridge64BitSupportExperimentDescription[];

extern const char kArcUsbHostName[];
extern const char kArcUsbHostDescription[];

extern const char kArcUsbStorageUIName[];
extern const char kArcUsbStorageUIDescription[];

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

extern const char kBluetoothNextHandsfreeProfileName[];
extern const char kBluetoothNextHandsfreeProfileDescription[];

extern const char kCameraSystemWebAppName[];
extern const char kCameraSystemWebAppDescription[];

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
extern const char kChromeOSDirectVideoDecoderName[];
extern const char kChromeOSDirectVideoDecoderDescription[];
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

extern const char kCdmFactoryDaemonName[];
extern const char kCdmFactoryDaemonDescription[];

extern const char kContextualNudgesName[];
extern const char kContextualNudgesDescription[];

extern const char kCrosLanguageSettingsUpdateName[];
extern const char kCrosLanguageSettingsUpdateDescription[];

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

extern const char kCrostiniWebUIUpgraderName[];
extern const char kCrostiniWebUIUpgraderDescription[];

extern const char kCrostiniUseDlcName[];
extern const char kCrostiniUseDlcDescription[];

extern const char kCrostiniEnableDlcName[];
extern const char kCrostiniEnableDlcDescription[];

extern const char kCryptAuthV2DeviceActivityStatusName[];
extern const char kCryptAuthV2DeviceActivityStatusDescription[];

extern const char kCryptAuthV2DeviceSyncName[];
extern const char kCryptAuthV2DeviceSyncDescription[];

extern const char kCryptAuthV2EnrollmentName[];
extern const char kCryptAuthV2EnrollmentDescription[];

extern const char kDisableCameraFrameRotationAtSourceName[];
extern const char kDisableCameraFrameRotationAtSourceDescription[];

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

extern const char kMovablePartialScreenshotName[];
extern const char kMovablePartialScreenshotDescription[];

extern const char kEnableAdvancedPpdAttributesName[];
extern const char kEnableAdvancedPpdAttributesDescription[];

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

extern const char kEnableAmbientModeName[];
extern const char kEnableAmbientModeDescription[];

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

extern const char kEnableAssistantTimersV2Name[];
extern const char kEnableAssistantTimersV2Description[];

extern const char kEnableAutoSelectName[];
extern const char kEnableAutoSelectDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kDragToSnapInClamshellModeName[];
extern const char kDragToSnapInClamshellModeDescription[];

extern const char kEnhancedClipboardName[];
extern const char kEnhancedClipboardDescription[];

extern const char kEnhancedClipboardSimpleRenderName[];
extern const char kEnhancedClipboardSimpleRenderDescription[];

extern const char kMultiDisplayOverviewAndSplitViewName[];
extern const char kMultiDisplayOverviewAndSplitViewDescription[];

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

extern const char kNewDragSpecInLauncherName[];
extern const char kNewDragSpecInLauncherDescription[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableNewShortcutMappingName[];
extern const char kEnableNewShortcutMappingDescription[];

extern const char kEnableOnDeviceAssistantName[];
extern const char kEnableOnDeviceAssistantDescription[];

extern const char kEnablePalmOnMaxTouchMajorName[];
extern const char kEnablePalmOnMaxTouchMajorDescription[];

extern const char kEnablePalmOnToolTypePalmName[];
extern const char kEnablePalmOnToolTypePalmDescription[];

extern const char kEnablePalmSuppressionName[];
extern const char kEnablePalmSuppressionDescription[];

extern const char kEnableParentalControlsSettingsName[];
extern const char kEnableParentalControlsSettingsDescription[];

extern const char kEnablePlayStoreSearchName[];
extern const char kEnablePlayStoreSearchDescription[];

extern const char kEnableQuickAnswersName[];
extern const char kEnableQuickAnswersDescription[];

extern const char kEnableQuickAnswersRichUiName[];
extern const char kEnableQuickAnswersRichUiDescription[];

extern const char kEnableQuickAnswersTextAnnotatorName[];
extern const char kEnableQuickAnswersTextAnnotatorDescription[];

extern const char kEnableQuickAnswersTranslationName[];
extern const char kEnableQuickAnswersTranslationDescription[];

extern const char kEnableQuickAnswersTranslationCloudAPIName[];
extern const char kEnableQuickAnswersTranslationCloudAPIDescription[];

extern const char kPluginVmShowCameraPermissionsName[];
extern const char kPluginVmShowCameraPermissionsDescription[];

extern const char kPluginVmShowMicrophonePermissionsName[];
extern const char kPluginVmShowMicrophonePermissionsDescription[];

extern const char kTrimOnFreezeName[];
extern const char kTrimOnFreezeDescription[];

extern const char kTrimOnMemoryPressureName[];
extern const char kTrimOnMemoryPressureDescription[];

extern const char kEnableZeroStateSuggestionsName[];
extern const char kEnableZeroStateSuggestionsDescription[];

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

extern const char kExperimentalAccessibilityChromeVoxAnnotationsName[];
extern const char kExperimentalAccessibilityChromeVoxAnnotationsDescription[];

extern const char kExperimentalAccessibilityChromeVoxTutorialName[];
extern const char kExperimentalAccessibilityChromeVoxTutorialDescription[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char kSwitchAccessPointScanningName[];
extern const char kSwitchAccessPointScanningDescription[];

extern const char kExperimentalAccessibilityCursorColorsName[];
extern const char kExperimentalAccessibilityCursorColorsDescription[];

extern const char kExperimentalAccessibilityMagnifierNewFocusFollowingName[];
extern const char
    kExperimentalAccessibilityMagnifierNewFocusFollowingDescription[];

extern const char kFilesAppCopyImageName[];
extern const char kFilesAppCopyImageDescription[];

extern const char kFilesCameraFolderName[];
extern const char kFilesCameraFolderDescription[];

extern const char kFilesNGName[];
extern const char kFilesNGDescription[];

extern const char kFilesSinglePartitionFormatName[];
extern const char kFilesSinglePartitionFormatDescription[];

extern const char kFilesSWAName[];
extern const char kFilesSWADescription[];

extern const char kFilesTransferDetailsName[];
extern const char kFilesTransferDetailsDescription[];

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

extern const char kHelpAppReleaseNotesName[];
extern const char kHelpAppReleaseNotesDescription[];

extern const char kHelpAppSearchServiceIntegrationName[];
extern const char kHelpAppSearchServiceIntegrationDescription[];

extern const char kHideArcMediaNotificationsName[];
extern const char kHideArcMediaNotificationsDescription[];

extern const char kHoldingSpaceName[];
extern const char kHoldingSpaceDescription[];

extern const char kImeAssistAutocorrectName[];
extern const char kImeAssistAutocorrectDescription[];

extern const char kImeAssistPersonalInfoName[];
extern const char kImeAssistPersonalInfoDescription[];

extern const char kImeEmojiSuggestAdditionName[];
extern const char kImeEmojiSuggestAdditionDescription[];

extern const char kImeInputLogicFstName[];
extern const char kImeInputLogicFstDescription[];

extern const char kImeInputLogicHmmName[];
extern const char kImeInputLogicHmmDescription[];

extern const char kImeInputLogicMozcName[];
extern const char kImeInputLogicMozcDescription[];

extern const char kImeMozcProtoName[];
extern const char kImeMozcProtoDescription[];

extern const char kImeServiceSandboxName[];
extern const char kImeServiceSandboxDescription[];

extern const char kIntentHandlingSharingName[];
extern const char kIntentHandlingSharingDescription[];

extern const char kIntentPickerPWAPersistenceName[];
extern const char kIntentPickerPWAPersistenceDescription[];

extern const char kInteractiveWindowCycleList[];
extern const char kInteractiveWindowCycleListDescription[];

extern const char kLacrosSupportName[];
extern const char kLacrosSupportDescription[];

extern const char kLimitAltTabToActiveDeskName[];
extern const char kLimitAltTabToActiveDeskDescription[];

extern const char kLimitShelfItemsToActiveDeskName[];
extern const char kLimitShelfItemsToActiveDeskDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kLockScreenMediaControlsName[];
extern const char kLockScreenMediaControlsDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMediaAppName[];
extern const char kMediaAppDescription[];

extern const char kMediaNotificationsCounterName[];
extern const char kMediaNotificationsCounterDescription[];

extern const char kMediaSessionNotificationsName[];
extern const char kMediaSessionNotificationsDescription[];

extern const char kMeteredShowToggleName[];
extern const char kMeteredShowToggleDescription[];

extern const char kOsSettingsPolymer3Name[];
extern const char kOsSettingsPolymer3Description[];

extern const char kPhoneHubName[];
extern const char kPhoneHubDescription[];

extern const char kReduceDisplayNotificationsName[];
extern const char kReduceDisplayNotificationsDescription[];

extern const char kReleaseNotesNotificationName[];
extern const char kReleaseNotesNotificationDescription[];

extern const char kReleaseNotesNotificationAllChannelsName[];
extern const char kReleaseNotesNotificationAllChannelsDescription[];

extern const char kScanningUIName[];
extern const char kScanningUIDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kSeparatePointingStickSettingsName[];
extern const char kSeparatePointingStickSettingsDescription[];

extern const char kSharesheetName[];
extern const char kSharesheetDescription[];

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

extern const char kSmartDimNewMlAgentName[];
extern const char kSmartDimNewMlAgentDescription[];

extern const char kSmbfsFileSharesName[];
extern const char kSmbfsFileSharesDescription[];

extern const char kSplitSettingsSyncName[];
extern const char kSplitSettingsSyncDescription[];

extern const char kSyncWifiConfigurationsName[];
extern const char kSyncWifiConfigurationsDescription[];

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

extern const char kUsbguardName[];
extern const char kUsbguardDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVaapiWebPImageDecodeAccelerationName[];
extern const char kVaapiWebPImageDecodeAccelerationDescription[];

extern const char kVirtualKeyboardBorderedKeyName[];
extern const char kVirtualKeyboardBorderedKeyDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVmCameraMicIndicatorsAndNotificationsName[];
extern const char kVmCameraMicIndicatorsAndNotificationsDescription[];

extern const char kWifiSyncAndroidName[];
extern const char kWifiSyncAndroidDescription[];

extern const char kZeroCopyVideoCaptureName[];
extern const char kZeroCopyVideoCaptureDescription[];

// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if defined(OS_CHROMEOS)

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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

extern const char kWebGL2ComputeContextName[];
extern const char kWebGL2ComputeContextDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

extern const char kEnableMediaFeedsName[];
extern const char kEnableMediaFeedsDescription[];

extern const char kEnableMediaFeedsBackgroundFetchName[];
extern const char kEnableMediaFeedsBackgroundFetchDescription[];

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

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)

extern const char kWebContentsOcclusionName[];
extern const char kWebContentsOcclusionDescription[];

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
extern const char kDynamicTcmallocName[];
extern const char kDynamicTcmallocDescription[];
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
extern const char kUserDataSnapshotName[];
extern const char kUserDataSnapshotDescription[];
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
extern const char kWebShareName[];
extern const char kWebShareDescription[];
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)
extern const char kEnableEphemeralGuestProfilesOnDesktopName[];
extern const char kEnableEphemeralGuestProfilesOnDesktopDescription[];
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_AV1_DECODER)
extern const char kEnableAVIFName[];
extern const char kEnableAVIFDescription[];
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
extern const char kClickToCallUIName[];
extern const char kClickToCallUIDescription[];
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

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
extern const char kPdfFormSaveName[];
extern const char kPdfFormSaveDescription[];

extern const char kPdfHonorJsContentSettingsName[];
extern const char kPdfHonorJsContentSettingsDescription[];

extern const char kPdfViewerUpdateName[];
extern const char kPdfViewerUpdateDescription[];
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kWebUITabStripName[];
extern const char kWebUITabStripDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && defined(OS_CHROMEOS)
extern const char kWebUITabStripTabDragIntegrationName[];
extern const char kWebUITabStripTabDragIntegrationDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && defined(OS_CHROMEOS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

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
