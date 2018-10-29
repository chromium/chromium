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
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"

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

extern const char kAffiliationBasedMatchingName[];
extern const char kAffiliationBasedMatchingDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAllowNaclSocketApiName[];
extern const char kAllowNaclSocketApiDescription[];

extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[];
extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[];

extern const char kAndroidMessagesIntegrationName[];
extern const char kAndroidMessagesIntegrationDescription[];

extern const char kAndroidMessagesProdEndpointName[];
extern const char kAndroidMessagesProdEndpointDescription[];

extern const char kAndroidSiteSettingsUIName[];
extern const char kAndroidSiteSettingsUIDescription[];

extern const char kAppBannersName[];
extern const char kAppBannersDescription[];

extern const char kAutomaticPasswordGenerationName[];
extern const char kAutomaticPasswordGenerationDescription[];

extern const char kEnableBlinkHeapUnifiedGarbageCollectionName[];
extern const char kEnableBlinkHeapUnifiedGarbageCollectionDescription[];

extern const char kEnableBloatedRendererDetectionName[];
extern const char kEnableBloatedRendererDetectionDescription[];

extern const char kAsyncImageDecodingName[];
extern const char kAsyncImageDecodingDescription[];

extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

extern const char kAutofillDynamicFormsName[];
extern const char kAutofillDynamicFormsDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

extern const char kAutofillNoLocalSaveOnUploadSuccessName[];
extern const char kAutofillNoLocalSaveOnUploadSuccessDescription[];

extern const char kAutofillPrefilledFieldsName[];
extern const char kAutofillPrefilledFieldsDescription[];

extern const char kAutofillPreviewStyleExperimentName[];
extern const char kAutofillPreviewStyleExperimentDescription[];

extern const char kAutofillRationalizeRepeatedServerPredictionsName[];
extern const char kAutofillRationalizeRepeatedServerPredictionsDescription[];

extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

extern const char kAutoplayPolicyName[];
extern const char kAutoplayPolicyDescription[];

extern const char kAutoplayPolicyUserGestureRequiredForCrossOrigin[];
extern const char kAutoplayPolicyNoUserGestureRequired[];
extern const char kAutoplayPolicyUserGestureRequired[];
extern const char kAutoplayPolicyDocumentUserActivation[];

extern const char kAv1DecoderName[];
extern const char kAv1DecoderDescription[];

extern const char kAwaitOptimizationName[];
extern const char kAwaitOptimizationDescription[];

extern const char kBleAdvertisingInExtensionsName[];
extern const char kBleAdvertisingInExtensionsDescription[];

extern const char kBlockTabUndersName[];
extern const char kBlockTabUndersDescription[];

extern const char kBrowserTaskSchedulerName[];
extern const char kBrowserTaskSchedulerDescription[];

extern const char kBundledConnectionHelpName[];
extern const char kBundledConnectionHelpDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kCanvas2DImageChromiumName[];
extern const char kCanvas2DImageChromiumDescription[];

extern const char kCastStreamingHwEncodingName[];
extern const char kCastStreamingHwEncodingDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kClipboardContentSettingName[];
extern const char kClipboardContentSettingDescription[];

extern const char kCloudImportName[];
extern const char kCloudImportDescription[];

extern const char kCloudPrinterHandlerName[];
extern const char kCloudPrinterHandlerDescription[];

extern const char kFCMInvalidationsName[];
extern const char kFCMInvalidationsDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileHdr[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kContextualSuggestionsAlternateCardLayoutName[];
extern const char kContextualSuggestionsAlternateCardLayoutDescription[];

extern const char kContextualSuggestionsButtonName[];
extern const char kContextualSuggestionsButtonDescription[];

extern const char kContextualSuggestionsIPHReverseScrollName[];
extern const char kContextualSuggestionsIPHReverseScrollDescription[];

extern const char kContextualSuggestionsOptOutName[];
extern const char kContextualSuggestionsOptOutDescription[];

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kCrossProcessGuestViewIsolationName[];
extern const char kCrossProcessGuestViewIsolationDescription[];

extern const char kDataSaverServerPreviewsName[];
extern const char kDataSaverServerPreviewsDescription[];

extern const char kDatasaverPromptName[];
extern const char kDatasaverPromptDescription[];
extern const char kDatasaverPromptDemoMode[];

#if DCHECK_IS_CONFIGURABLE
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // DCHECK_IS_CONFIGURABLE

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDefaultTileHeightName[];
extern const char kDefaultTileHeightDescription[];
extern const char kDefaultTileHeightShort[];
extern const char kDefaultTileHeightTall[];
extern const char kDefaultTileHeightGrande[];
extern const char kDefaultTileHeightVenti[];

extern const char kDefaultTileWidthName[];
extern const char kDefaultTileWidthDescription[];
extern const char kDefaultTileWidthShort[];
extern const char kDefaultTileWidthTall[];
extern const char kDefaultTileWidthGrande[];
extern const char kDefaultTileWidthVenti[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

extern const char kDevtoolsExperimentsName[];
extern const char kDevtoolsExperimentsDescription[];

extern const char kDisableAudioForDesktopShareName[];
extern const char kDisableAudioForDesktopShareDescription[];

extern const char kDisablePushStateThrottleName[];
extern const char kDisablePushStateThrottleDescription[];

extern const char kDisableTabForDesktopShareName[];
extern const char kDisableTabForDesktopShareDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kDisallowUnsafeHttpDownloadsName[];
extern const char kDisallowUnsafeHttpDownloadsNameDescription[];

extern const char kDisplayList2dCanvasName[];
extern const char kDisplayList2dCanvasDescription[];

extern const char kDriveSearchInChromeLauncherName[];
extern const char kDriveSearchInChromeLauncherDescription[];

extern const char kEmbeddedExtensionOptionsName[];
extern const char kEmbeddedExtensionOptionsDescription[];

extern const char kEnableAccessibilityObjectModelName[];
extern const char kEnableAccessibilityObjectModelDescription[];

extern const char kEnableAutofillAccountWalletStorageName[];
extern const char kEnableAutofillAccountWalletStorageDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardLastUsedDateDisplayName[];
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

extern const char kEnableAutofillCreditCardLocalCardMigrationName[];
extern const char kEnableAutofillCreditCardLocalCardMigrationDescription[];

extern const char kEnableAutofillCreditCardUploadEditableCardholderNameName[];
extern const char
    kEnableAutofillCreditCardUploadEditableCardholderNameDescription[];

extern const char
    kEnableAutofillCreditCardUploadGooglePayOnAndroidBrandingName[];
extern const char
    kEnableAutofillCreditCardUploadGooglePayOnAndroidBrandingDescription[];

extern const char kEnableAutofillLocalCardMigrationShowFeedbackName[];
extern const char kEnableAutofillLocalCardMigrationShowFeedbackDescription[];

extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[];
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[];

extern const char kEnableAutofillToolkitViewsCreditCardDialogsMac[];
extern const char kEnableAutofillToolkitViewsCreditCardDialogsMacDescription[];

extern const char kEnableAutofillNativeDropdownViewsName[];
extern const char kEnableAutofillNativeDropdownViewsDescription[];

extern const char kEnableAutofillSaveCardDialogUnlabeledExpirationDateName[];
extern const char
    kEnableAutofillSaveCardDialogUnlabeledExpirationDateDescription[];

extern const char kEnableAutofillSaveCardSignInAfterLocalSaveName[];
extern const char kEnableAutofillSaveCardSignInAfterLocalSaveDescription[];

extern const char kEnableAutofillSendExperimentIdsInPaymentsRPCsName[];
extern const char kEnableAutofillSendExperimentIdsInPaymentsRPCsDescription[];

extern const char kEnableAutoplayIgnoreWebAudioName[];
extern const char kEnableAutoplayIgnoreWebAudioDescription[];

extern const char kEnableAutoplayUnifiedSoundSettingsName[];
extern const char kEnableAutoplayUnifiedSoundSettingsDescription[];

extern const char kEnableBrotliName[];
extern const char kEnableBrotliDescription[];

extern const char kEnableCaptivePortalRandomUrl[];
extern const char kEnableCaptivePortalRandomUrlDescription[];

extern const char kEnableChromevoxDeveloperOptionName[];
extern const char kEnableChromevoxDeveloperOptionDescription[];

extern const char kEnableClientLoFiName[];
extern const char kEnableClientLoFiDescription[];

extern const char kEnableCSSFragmentIdentifiersName[];
extern const char kEnableCSSFragmentIdentifiersDescription[];

extern const char kEnableCursorMotionBlurName[];
extern const char kEnableCursorMotionBlurDescription[];

extern const char kEnableNoScriptPreviewsName[];
extern const char kEnableNoScriptPreviewsDescription[];

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

extern const char kEnableDataReductionProxySavingsPromoName[];
extern const char kEnableDataReductionProxySavingsPromoDescription[];

extern const char kEnableDesktopPWAsName[];
extern const char kEnableDesktopPWAsDescription[];

extern const char kEnableDesktopPWAsLinkCapturingName[];
extern const char kEnableDesktopPWAsLinkCapturingDescription[];

extern const char kDesktopPWAsStayInWindowName[];
extern const char kDesktopPWAsStayInWindowDescription[];

extern const char kEnableSystemWebAppsName[];
extern const char kEnableSystemWebAppsDescription[];

extern const char kEnableDockedMagnifierName[];
extern const char kEnableDockedMagnifierDescription[];

extern const char kEnableEmojiContextMenuName[];
extern const char kEnableEmojiContextMenuDescription[];

extern const char kEnforceTLS13DowngradeName[];
extern const char kEnforceTLS13DowngradeDescription[];

extern const char kEnableEnumeratingAudioDevicesName[];
extern const char kEnableEnumeratingAudioDevicesDescription[];

extern const char kEnableGenericSensorName[];
extern const char kEnableGenericSensorDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableHDRName[];
extern const char kEnableHDRDescription[];

extern const char kEnableHeavyPageCappingName[];
extern const char kEnableHeavyPageCappingDescription[];

extern const char kEnablePreviewsAndroidOmniboxUIName[];
extern const char kEnablePreviewsAndroidOmniboxUIDescription[];

extern const char kEnableLitePageServerPreviewsName[];
extern const char kEnableLitePageServerPreviewsDescription[];

extern const char kEnableHttpFormWarningName[];
extern const char kEnableHttpFormWarningDescription[];

extern const char kLayeredAPIName[];
extern const char kLayeredAPIDescription[];

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

extern const char kEnablePolicyToolName[];
extern const char kEnablePolicyToolDescription[];

extern const char kDisableMultiMirroringName[];
extern const char kDisableMultiMirroringDescription[];

extern const char kEnableNavigationTracingName[];
extern const char kEnableNavigationTracingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableNetworkServiceName[];
extern const char kEnableNetworkServiceDescription[];

extern const char kEnableNetworkServiceInProcessName[];
extern const char kEnableNetworkServiceInProcessDescription[];

extern const char kEnableNewPrintPreview[];
extern const char kEnableNewPrintPreviewDescription[];

extern const char kEnableNightLightName[];
extern const char kEnableNightLightDescription[];

extern const char kEnableNotificationScrollBarName[];
extern const char kEnableNotificationScrollBarDescription[];

extern const char kEnableNotificationExpansionAnimationName[];
extern const char kEnableNotificationExpansionAnimationDescription[];

extern const char kEnableNupPrintingName[];
extern const char kEnableNupPrintingDescription[];

extern const char kEnableOptimizationHintsName[];
extern const char kEnableOptimizationHintsDescription[];

extern const char kEnableOutOfBlinkCORSName[];
extern const char kEnableOutOfBlinkCORSDescription[];

extern const char kEnableOverviewSwipeToCloseName[];
extern const char kEnableOverviewSwipeToCloseDescription[];

extern const char kVizDisplayCompositorName[];
extern const char kVizDisplayCompositorDescription[];

extern const char kVizHitTestDrawQuadName[];
extern const char kVizHitTestDrawQuadDescription[];

extern const char kEnableOutOfProcessHeapProfilingName[];
extern const char kEnableOutOfProcessHeapProfilingDescription[];
extern const char kEnableOutOfProcessHeapProfilingModeMinimal[];
extern const char kEnableOutOfProcessHeapProfilingModeAll[];
extern const char kEnableOutOfProcessHeapProfilingModeAllRenderers[];
extern const char kEnableOutOfProcessHeapProfilingModeBrowser[];
extern const char kEnableOutOfProcessHeapProfilingModeGpu[];
extern const char kEnableOutOfProcessHeapProfilingModeManual[];
extern const char kEnableOutOfProcessHeapProfilingModeRendererSampling[];
extern const char kOutOfProcessHeapProfilingKeepSmallAllocations[];
extern const char kOutOfProcessHeapProfilingKeepSmallAllocationsDescription[];
extern const char kOutOfProcessHeapProfilingSampling[];
extern const char kOutOfProcessHeapProfilingSamplingDescription[];

extern const char kOOPHPStackModeName[];
extern const char kOOPHPStackModeDescription[];
extern const char kOOPHPStackModeMixed[];
extern const char kOOPHPStackModeNative[];
extern const char kOOPHPStackModeNativeWithThreadNames[];
extern const char kOOPHPStackModePseudo[];

extern const char kEnablePictureInPictureName[];
extern const char kEnablePictureInPictureDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];

extern const char kEnableResourceLoadingHintsName[];
extern const char kEnableResourceLoadingHintsDescription[];

extern const char kEnableSyncUserConsentSeparateTypeName[];
extern const char kEnableSyncUserConsentSeparateTypeDescription[];

extern const char kEnableSyncUSSBookmarksName[];
extern const char kEnableSyncUSSBookmarksDescription[];

extern const char kEnableSyncUSSSessionsName[];
extern const char kEnableSyncUSSSessionsDescription[];

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

extern const char kEnableWebAuthenticationAPIName[];
extern const char kEnableWebAuthenticationAPIDescription[];

extern const char kEnableWebAuthenticationCableSupportName[];
extern const char kEnableWebAuthenticationCableSupportDescription[];

extern const char kEnableWebPaymentsSingleAppUiSkipName[];
extern const char kEnableWebPaymentsSingleAppUiSkipDescription[];

extern const char kEnableWebUsbName[];
extern const char kEnableWebUsbDescription[];

extern const char kEnableImageCaptureAPIName[];
extern const char kEnableImageCaptureAPIDescription[];

extern const char kEnableIncognitoWindowCounterName[];
extern const char kEnableIncognitoWindowCounterDescription[];

extern const char kEnableZeroSuggestRedirectToChromeName[];
extern const char kEnableZeroSuggestRedirectToChromeDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmThreadsName[];
extern const char kEnableWasmThreadsDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

extern const char kExperimentalAppBannersName[];
extern const char kExperimentalAppBannersDescription[];

extern const char kExperimentalCanvasFeaturesName[];
extern const char kExperimentalCanvasFeaturesDescription[];

extern const char kExperimentalCrostiniUIName[];
extern const char kExperimentalCrostiniUIDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalFullscreenExitUIName[];
extern const char kExperimentalFullscreenExitUIDescription[];

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

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFastUnloadName[];
extern const char kFastUnloadDescription[];

extern const char kFeaturePolicyName[];
extern const char kFeaturePolicyDescription[];

extern const char kFontCacheScalingName[];
extern const char kFontCacheScalingDescription[];

extern const char kForceEffectiveConnectionTypeName[];
extern const char kForceEffectiveConnectionTypeDescription[];
extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionTypeSlow2GOnCellularDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFramebustingName[];
extern const char kFramebustingDescription[];

extern const char kGamepadExtensionsName[];
extern const char kGamepadExtensionsDescription[];
extern const char kGamepadPollingRateName[];
extern const char kGamepadPollingRateDescription[];
extern const char kGamepadVibrationName[];
extern const char kGamepadVibrationDescription[];

extern const char kGpuRasterizationMsaaSampleCountName[];
extern const char kGpuRasterizationMsaaSampleCountDescription[];
extern const char kGpuRasterizationMsaaSampleCountZero[];
extern const char kGpuRasterizationMsaaSampleCountTwo[];
extern const char kGpuRasterizationMsaaSampleCountFour[];
extern const char kGpuRasterizationMsaaSampleCountEight[];
extern const char kGpuRasterizationMsaaSampleCountSixteen[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];
extern const char kForceGpuRasterization[];

extern const char kGoogleProfileInfoName[];
extern const char kGoogleProfileInfoDescription[];

extern const char kHarfbuzzRendertextName[];
extern const char kHarfbuzzRendertextDescription[];

extern const char kHorizontalTabSwitcherAndroidName[];
extern const char kHorizontalTabSwitcherAndroidDescription[];

extern const char kViewsCastDialogName[];
extern const char kViewsCastDialogDescription[];

extern const char kHideActiveAppsFromShelfName[];
extern const char kHideActiveAppsFromShelfDescription[];

extern const char kHistoryRequiresUserGestureName[];
extern const char kHistoryRequiresUserGestureDescription[];
extern const char kHyperlinkAuditingName[];
extern const char kHyperlinkAuditingDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kHtmlBasedUsernameDetectorName[];
extern const char kHtmlBasedUsernameDetectorDescription[];

extern const char kIconNtpName[];
extern const char kIconNtpDescription[];

extern const char kIgnoreGpuBlacklistName[];
extern const char kIgnoreGpuBlacklistDescription[];

extern const char kIgnorePreviewsBlacklistName[];
extern const char kIgnorePreviewsBlacklistDescription[];

extern const char kImprovedLanguageSettingsName[];
extern const char kImprovedLanguageSettingsDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kJustInTimeServiceWorkerPaymentAppName[];
extern const char kJustInTimeServiceWorkerPaymentAppDescription[];

extern const char kKeepAliveRendererForKeepaliveRequestsName[];
extern const char kKeepAliveRendererForKeepaliveRequestsDescription[];

extern const char kKeyboardLockApiName[];
extern const char kKeyboardLockApiDescription[];

extern const char kLcdTextName[];
extern const char kLcdTextDescription[];

extern const char kLeftToRightUrlsName[];
extern const char kLeftToRightUrlsDescription[];

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kLookalikeUrlNavigationSuggestionsName[];
extern const char kLookalikeUrlNavigationSuggestionsDescription[];

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];
extern const char kMarkHttpAsWarning[];
extern const char kMarkHttpAsWarningAndDangerousOnFormEdits[];
extern const char kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards[];

extern const char kMaterialDesignIncognitoNTPName[];
extern const char kMaterialDesignIncognitoNTPDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMemoryCoordinatorName[];
extern const char kMemoryCoordinatorDescription[];

extern const char kMessageCenterNewStyleNotificationName[];
extern const char kMessageCenterNewStyleNotificationDescription[];

extern const char kMhtmlGeneratorOptionName[];
extern const char kMhtmlGeneratorOptionDescription[];
extern const char kMhtmlSkipNostoreMain[];
extern const char kMhtmlSkipNostoreAll[];

extern const char kNewAudioRenderingMixingStrategyName[];
extern const char kNewAudioRenderingMixingStrategyDescription[];

extern const char kNewBookmarkAppsName[];
extern const char kNewBookmarkAppsDescription[];

extern const char kNewPasswordFormParsingName[];
extern const char kNewPasswordFormParsingDescription[];

extern const char kNewPasswordFormParsingForSavingName[];
extern const char kNewPasswordFormParsingForSavingDescription[];

extern const char kNewRemotePlaybackPipelineName[];
extern const char kNewRemotePlaybackPipelineDescription[];

extern const char kUseSurfaceLayerForVideoName[];
extern const char kUseSurfaceLayerForVideoDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNewblueName[];
extern const char kNewblueDescription[];

extern const char kNewTabButtonPosition[];
extern const char kNewTabButtonPositionDescription[];
extern const char kNewTabButtonPositionOppositeCaption[];
extern const char kNewTabButtonPositionLeading[];
extern const char kNewTabButtonPositionAfterTabs[];
extern const char kNewTabButtonPositionTrailing[];

extern const char kNostatePrefetchName[];
extern const char kNostatePrefetchDescription[];

extern const char kNotificationIndicatorName[];
extern const char kNotificationIndicatorDescription[];

extern const char kNotificationsNativeFlagName[];
extern const char kNotificationsNativeFlagDescription[];

extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

#if defined(OS_POSIX)
extern const char kNtlmV2EnabledName[];
extern const char kNtlmV2EnabledDescription[];
#endif

extern const char kNtpBackgroundsName[];
extern const char kNtpBackgroundsDescription[];

extern const char kNtpCustomLinksName[];
extern const char kNtpCustomLinksDescription[];

extern const char kNtpIconsName[];
extern const char kNtpIconsDescription[];

extern const char kNtpUIMdName[];
extern const char kNtpUIMdDescription[];

extern const char kNumRasterThreadsName[];
extern const char kNumRasterThreadsDescription[];
extern const char kNumRasterThreadsOne[];
extern const char kNumRasterThreadsTwo[];
extern const char kNumRasterThreadsThree[];
extern const char kNumRasterThreadsFour[];

extern const char kOfferStoreUnmaskedWalletCardsName[];
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

extern const char kOfflineAutoReloadName[];
extern const char kOfflineAutoReloadDescription[];

extern const char kOfflineAutoReloadVisibleOnlyName[];
extern const char kOfflineAutoReloadVisibleOnlyDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

extern const char kOmniboxUIHideSteadyStateUrlSchemeName[];
extern const char kOmniboxUIHideSteadyStateUrlSchemeDescription[];

extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[];

extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefName[];
extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription[];

extern const char kOmniboxUIJogTextfieldOnPopupName[];
extern const char kOmniboxUIJogTextfieldOnPopupDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxUIShowSuggestionFaviconsName[];
extern const char kOmniboxUIShowSuggestionFaviconsDescription[];

extern const char kOmniboxUISwapTitleAndUrlName[];
extern const char kOmniboxUISwapTitleAndUrlDescription[];

extern const char kOmniboxVoiceSearchAlwaysVisibleName[];
extern const char kOmniboxVoiceSearchAlwaysVisibleDescription[];

extern const char kOopRasterizationName[];
extern const char kOopRasterizationDescription[];

extern const char kOverflowIconsForMediaControlsName[];
extern const char kOverflowIconsForMediaControlsDescription[];

extern const char kOriginTrialsName[];
extern const char kOriginTrialsDescription[];

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

extern const char kUseNewAcceptLanguageHeaderName[];
extern const char kUseNewAcceptLanguageHeaderDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kOverscrollStartThresholdName[];
extern const char kOverscrollStartThresholdDescription[];
extern const char kOverscrollStartThreshold133Percent[];
extern const char kOverscrollStartThreshold166Percent[];
extern const char kOverscrollStartThreshold200Percent[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

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

extern const char kPasswordSearchMobileName[];
extern const char kPasswordSearchMobileDescription[];

extern const char kPasswordsKeyboardAccessoryName[];
extern const char kPasswordsKeyboardAccessoryDescription[];

extern const char kPasswordsMigrateLinuxToLoginDBName[];
extern const char kPasswordsMigrateLinuxToLoginDBDescription[];

extern const char kPdfIsolationName[];
extern const char kPdfIsolationDescription[];

extern const char kPinchScaleName[];
extern const char kPinchScaleDescription[];

extern const char kPreviewsAllowedName[];
extern const char kPreviewsAllowedDescription[];

extern const char kPrintPdfAsImageName[];
extern const char kPrintPdfAsImageDescription[];

extern const char kPrintPreviewRegisterPromosName[];
extern const char kPrintPreviewRegisterPromosDescription[];

extern const char kProtectSyncCredentialName[];
extern const char kProtectSyncCredentialDescription[];

extern const char kProtectSyncCredentialOnReauthName[];
extern const char kProtectSyncCredentialOnReauthDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kQueryInOmniboxName[];
extern const char kQueryInOmniboxDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kRecurrentInterstitialName[];
extern const char kRecurrentInterstitialDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

extern const char kRegionalLocalesAsDisplayUIName[];
extern const char kRegionalLocalesAsDisplayUIDescription[];

extern const char kRemoveNavigationHistoryName[];
extern const char kRemoveNavigationHistoryDescription[];

extern const char kRewriteLevelDBOnDeletionName[];
extern const char kRewriteLevelDBOnDeletionDescription[];

extern const char kRendererSideResourceSchedulerName[];
extern const char kRendererSideResourceSchedulerDescription[];

extern const char kRequestTabletSiteName[];
extern const char kRequestTabletSiteDescription[];

extern const char kResetAppListInstallStateName[];
extern const char kResetAppListInstallStateDescription[];

extern const char kResourceLoadSchedulerName[];
extern const char kResourceLoadSchedulerDescription[];

extern const char kSafeSearchUrlReportingName[];
extern const char kSafeSearchUrlReportingDescription[];

extern const char kSamplingHeapProfilerName[];
extern const char kSamplingHeapProfilerDescription[];

extern const char kSaveasMenuLabelExperimentName[];
extern const char kSaveasMenuLabelExperimentDescription[];

extern const char kSavePageAsMhtmlName[];
extern const char kSavePageAsMhtmlDescription[];

extern const char kSavePreviousDocumentResourcesName[];
extern const char kSavePreviousDocumentResourcesDescription[];
extern const char kSavePreviousDocumentResourcesNever[];
extern const char kSavePreviousDocumentResourcesUntilOnDOMContentLoaded[];
extern const char kSavePreviousDocumentResourcesUntilOnLoad[];

extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

extern const char kServiceWorkerPaymentAppsName[];
extern const char kServiceWorkerPaymentAppsDescription[];

extern const char kServiceWorkerImportedScriptUpdateCheckName[];
extern const char kServiceWorkerImportedScriptUpdateCheckDescription[];

extern const char kServiceWorkerServicificationName[];
extern const char kServiceWorkerServicificationDescription[];

extern const char kServiceWorkerLongRunningMessageName[];
extern const char kServiceWorkerLongRunningMessageDescription[];

extern const char kSettingsWindowName[];
extern const char kSettingsWindowDescription[];

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

extern const char kSupervisedUserCommittedInterstitialsName[];
extern const char kSupervisedUserCommittedInterstitialsDescription[];

extern const char kEnableDrawOcclusionName[];
extern const char kEnableDrawOcclusionDescription[];

extern const char kShowSavedCopyName[];
extern const char kShowSavedCopyDescription[];
extern const char kEnableShowSavedCopyPrimary[];
extern const char kEnableShowSavedCopySecondary[];
extern const char kDisableShowSavedCopy[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

extern const char kSignedHTTPExchangeName[];
extern const char kSignedHTTPExchangeDescription[];

extern const char kSimpleCacheBackendName[];
extern const char kSimpleCacheBackendDescription[];

extern const char kSimplifyHttpsIndicatorName[];
extern const char kSimplifyHttpsIndicatorDescription[];

extern const char kSingleClickAutofillName[];
extern const char kSingleClickAutofillDescription[];

extern const char kSingleTabMode[];
extern const char kSingleTabModeDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kSiteIsolationTrialOptOutName[];
extern const char kSiteIsolationTrialOptOutDescription[];
extern const char kSiteIsolationTrialOptOutChoiceDefault[];
extern const char kSiteIsolationTrialOptOutChoiceOptOut[];

extern const char kSiteSettings[];
extern const char kSiteSettingsDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSoftwareRasterizerName[];
extern const char kSoftwareRasterizerDescription[];

extern const char kSoleIntegrationName[];
extern const char kSoleIntegrationDescription[];

extern const char kSoundContentSettingName[];
extern const char kSoundContentSettingDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kSpellingFeedbackFieldTrialName[];
extern const char kSpellingFeedbackFieldTrialDescription[];

extern const char kSSLCommittedInterstitialsName[];
extern const char kSSLCommittedInterstitialsDescription[];

extern const char kStopInBackgroundName[];
extern const char kStopInBackgroundDescription[];

extern const char kStopNonTimersInBackgroundName[];
extern const char kStopNonTimersInBackgroundDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kTLS13VariantName[];
extern const char kTLS13VariantDescription[];
extern const char kTLS13VariantDisabled[];
extern const char kTLS13VariantDeprecated[];
extern const char kTLS13VariantDraft23[];
extern const char kTLS13VariantFinal[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncStandaloneTransportName[];
extern const char kSyncStandaloneTransportDescription[];

extern const char kSyncSupportSecondaryAccountName[];
extern const char kSyncSupportSecondaryAccountDescription[];

extern const char kSyncUSSAutofillProfileName[];
extern const char kSyncUSSAutofillProfileDescription[];

extern const char kSyncUSSAutofillWalletDataName[];
extern const char kSyncUSSAutofillWalletDataDescription[];

extern const char kSysInternalsName[];
extern const char kSysInternalsDescription[];

extern const char kTabModalJsDialogName[];
extern const char kTabModalJsDialogDescription[];

extern const char kTabsInCbdName[];
extern const char kTabsInCbdDescription[];

extern const char kTcpFastOpenName[];
extern const char kTcpFastOpenDescription[];

extern const char kTintGlCompositedContentName[];
extern const char kTintGlCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTopSitesFromSiteEngagementName[];
extern const char kTopSitesFromSiteEngagementDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

extern const char kTouchableAppContextMenuName[];
extern const char kTouchableAppContextMenuDescription[];

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

extern const char kTranslateExplicitLanguageAskName[];
extern const char kTranslateExplicitLanguageAskDescription[];

extern const char kTranslateForceTriggerOnEnglishName[];
extern const char kTranslateForceTriggerOnEnglishDescription[];

extern const char kTranslateRankerEnforcementName[];
extern const char kTranslateRankerEnforcementDescription[];

extern const char kTranslateUIName[];
extern const char kTranslateUIDescription[];

extern const char kTreatInsecureOriginAsSecureName[];
extern const char kTreatInsecureOriginAsSecureDescription[];

extern const char kTrySupportedChannelLayoutsName[];
extern const char kTrySupportedChannelLayoutsDescription[];

extern const char kUnifiedConsentName[];
extern const char kUnifiedConsentDescription[];

extern const char kForceUnifiedConsentBumpName[];
extern const char kForceUnifiedConsentBumpDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

extern const char kUseModernMediaControlsName[];
extern const char kUseModernMediaControlsDescription[];

extern const char kUsePdfCompositorServiceName[];
extern const char kUsePdfCompositorServiceDescription[];

extern const char kUserActivationV2Name[];
extern const char kUserActivationV2Description[];

extern const char kUserConsentForExtensionScriptsName[];
extern const char kUserConsentForExtensionScriptsDescription[];

extern const char kUseSuggestionsEvenIfFewFeatureName[];
extern const char kUseSuggestionsEvenIfFewFeatureDescription[];

extern const char kV8CacheOptionsName[];
extern const char kV8CacheOptionsDescription[];
extern const char kV8CacheOptionsParse[];
extern const char kV8CacheOptionsCode[];

extern const char kV8ContextSnapshotName[];
extern const char kV8ContextSnapshotDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kV8OrinocoName[];
extern const char kV8OrinocoDescription[];

extern const char kVideoFullscreenOrientationLockName[];
extern const char kVideoFullscreenOrientationLockDescription[];

extern const char kVideoRotateToFullscreenName[];
extern const char kVideoRotateToFullscreenDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kWebPaymentsName[];
extern const char kWebPaymentsDescription[];

extern const char kWebPaymentsModifiersName[];
extern const char kWebPaymentsModifiersDescription[];

extern const char kWebrtcEchoCanceller3Name[];
extern const char kWebrtcEchoCanceller3Description[];

extern const char kWebrtcHybridAgcName[];
extern const char kWebrtcHybridAgcDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebrtcHwH264EncodingName[];
extern const char kWebrtcHwH264EncodingDescription[];

extern const char kWebrtcHwVP8EncodingName[];
extern const char kWebrtcHwVP8EncodingDescription[];

extern const char kWebrtcNewEncodeCpuLoadEstimatorName[];
extern const char kWebrtcNewEncodeCpuLoadEstimatorDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcSrtpEncryptedHeadersName[];
extern const char kWebrtcSrtpEncryptedHeadersDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];
extern const char kWebrtcH264WithOpenh264FfmpegName[];
extern const char kWebrtcH264WithOpenh264FfmpegDescription[];

extern const char kWebrtcUnifiedPlanByDefaultName[];
extern const char kWebrtcUnifiedPlanByDefaultDescription[];

extern const char kWebSocketHandshakeReuseConnectionName[];
extern const char kWebSocketHandshakeReuseConnectionDescription[];

extern const char kWebvrName[];
extern const char kWebvrDescription[];

extern const char kWebXrName[];
extern const char kWebXrDescription[];
extern const char kWebXrGamepadSupportName[];
extern const char kWebXrGamepadSupportDescription[];

extern const char kWebXrHitTestName[];
extern const char kWebXrHitTestDescription[];

extern const char kWebXrOrientationSensorDeviceName[];
extern const char kWebXrOrientationSensorDeviceDescription[];

extern const char kWifiCredentialSyncName[];
extern const char kWifiCredentialSyncDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAiaFetchingName[];
extern const char kAiaFetchingDescription[];

extern const char kAccessibilityTabSwitcherName[];
extern const char kAccessibilityTabSwitcherDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidPaymentAppsName[];
extern const char kAndroidPaymentAppsDescription[];

extern const char kAndroidSurfaceControl[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAppNotificationStatusMessagingName[];
extern const char kAppNotificationStatusMessagingDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutoFetchOnNetErrorPageName[];
extern const char kAutoFetchOnNetErrorPageDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kBackgroundLoaderForDownloadsName[];
extern const char kBackgroundLoaderForDownloadsDescription[];

extern const char kBackgroundTaskComponentUpdateName[];
extern const char kBackgroundTaskComponentUpdateDescription[];

extern const char kCCTModuleName[];
extern const char kCCTModuleDescription[];

extern const char kCCTModuleCacheName[];
extern const char kCCTModuleCacheDescription[];

extern const char kChromeDuetName[];
extern const char kChromeDuetDescription[];

extern const char kChromeHomeSwipeLogicName[];
extern const char kChromeHomeSwipeLogicDescription[];
extern const char kChromeHomeSwipeLogicRestrictArea[];
extern const char kChromeHomeSwipeLogicVelocity[];

extern const char kChromeMemexName[];
extern const char kChromeMemexDescription[];

extern const char kClearOldBrowsingDataName[];
extern const char kClearOldBrowsingDataDescription[];

extern const char kContentSuggestionsCategoryOrderName[];
extern const char kContentSuggestionsCategoryOrderDescription[];

extern const char kContentSuggestionsCategoryRankerName[];
extern const char kContentSuggestionsCategoryRankerDescription[];

extern const char kContentSuggestionsDebugLogName[];
extern const char kContentSuggestionsDebugLogDescription[];

extern const char kContextualSearchMlTapSuppressionName[];
extern const char kContextualSearchMlTapSuppressionDescription[];

extern const char kContextualSearchName[];
extern const char kContextualSearchDescription[];

extern const char kContextualSearchRankerQueryName[];
extern const char kContextualSearchRankerQueryDescription[];

extern const char kContextualSearchSecondTapName[];
extern const char kContextualSearchSecondTapDescription[];

extern const char kContextualSearchUnityIntegrationName[];
extern const char kContextualSearchUnityIntegrationDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kDontPrefetchLibrariesName[];
extern const char kDontPrefetchLibrariesDescription[];

extern const char kDownloadsLocationChangeName[];
extern const char kDownloadsLocationChangeDescription[];

extern const char kDownloadProgressInfoBarName[];
extern const char kDownloadProgressInfoBarDescription[];

extern const char kDownloadHomeV2Name[];
extern const char kDownloadHomeV2Description[];

extern const char kEnableAndroidPayIntegrationV1Name[];
extern const char kEnableAndroidPayIntegrationV1Description[];

extern const char kEnableAndroidPayIntegrationV2Name[];
extern const char kEnableAndroidPayIntegrationV2Description[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAutofillRefreshStyleName[];
extern const char kEnableAutofillRefreshStyleDescription[];

extern const char kEnableAndroidSpellcheckerName[];
extern const char kEnableAndroidSpellcheckerDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnableContentSuggestionsNewFaviconServerName[];
extern const char kEnableContentSuggestionsNewFaviconServerDescription[];

extern const char kEnableContentSuggestionsThumbnailDominantColorName[];
extern const char kEnableContentSuggestionsThumbnailDominantColorDescription[];

extern const char kEnableCustomContextMenuName[];
extern const char kEnableCustomContextMenuDescription[];

extern const char kEnableCustomFeedbackUiName[];
extern const char kEnableCustomFeedbackUiDescription[];

extern const char kEnableDataReductionProxyMainMenuName[];
extern const char kEnableDataReductionProxyMainMenuDescription[];

extern const char kEnableMediaControlsExpandGestureName[];
extern const char kEnableMediaControlsExpandGestureDescription[];

extern const char kEnableOmniboxClipboardProviderName[];
extern const char kEnableOmniboxClipboardProviderDescription[];

extern const char kEnableNtpAssetDownloadSuggestionsName[];
extern const char kEnableNtpAssetDownloadSuggestionsDescription[];

extern const char kEnableNtpBookmarkSuggestionsName[];
extern const char kEnableNtpBookmarkSuggestionsDescription[];

extern const char kEnableNtpOfflinePageDownloadSuggestionsName[];
extern const char kEnableNtpOfflinePageDownloadSuggestionsDescription[];

extern const char kEnableNtpRemoteSuggestionsName[];
extern const char kEnableNtpRemoteSuggestionsDescription[];

extern const char kEnableNtpSnippetsVisibilityName[];
extern const char kEnableNtpSnippetsVisibilityDescription[];

extern const char kEnableNtpSuggestionsNotificationsName[];
extern const char kEnableNtpSuggestionsNotificationsDescription[];

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableOskOverscrollName[];
extern const char kEnableOskOverscrollDescription[];

extern const char kEnableSpecialLocaleName[];
extern const char kEnableSpecialLocaleDescription[];

extern const char kEnableWebNfcName[];
extern const char kEnableWebNfcDescription[];

extern const char kEnableWebPaymentsMethodSectionOrderV2Name[];
extern const char kEnableWebPaymentsMethodSectionOrderV2Description[];

extern const char kEphemeralTabName[];
extern const char kEphemeralTabDescription[];

extern const char kGrantNotificationsToDSEName[];
extern const char kGrantNotificationsToDSENameDescription[];

extern const char kHomePageButtonName[];
extern const char kHomePageButtonDescription[];

extern const char kHomepageTileName[];
extern const char kHomepageTileDescription[];

extern const char kIncognitoStringsName[];
extern const char kIncognitoStringsDescription[];

extern const char kInterestFeedContentSuggestionsName[];
extern const char kInterestFeedContentSuggestionsDescription[];

extern const char kKeepPrefetchedContentSuggestionsName[];
extern const char kKeepPrefetchedContentSuggestionsDescription[];

extern const char kLanguagesPreferenceName[];
extern const char kLanguagesPreferenceDescription[];

extern const char kLsdPermissionPromptName[];
extern const char kLsdPermissionPromptDescription[];

extern const char kModalPermissionDialogViewName[];
extern const char kModalPermissionDialogViewDescription[];

extern const char kMediaScreenCaptureName[];
extern const char kMediaScreenCaptureDescription[];

extern const char kModalPermissionPromptsName[];
extern const char kModalPermissionPromptsDescription[];

extern const char kNewContactsPickerName[];
extern const char kNewContactsPickerDescription[];

extern const char kNewNetErrorPageUIName[];
extern const char kNewNetErrorPageUIDescription[];

extern const char kNewPhotoPickerName[];
extern const char kNewPhotoPickerDescription[];

extern const char kNoCreditCardAbort[];
extern const char kNoCreditCardAbortDescription[];

extern const char kNtpButtonName[];
extern const char kNtpButtonDescription[];

extern const char kOfflineBookmarksName[];
extern const char kOfflineBookmarksDescription[];

extern const char kOfflineIndicatorAlwaysHttpProbeName[];
extern const char kOfflineIndicatorAlwaysHttpProbeDescription[];

extern const char kOfflineIndicatorChoiceName[];
extern const char kOfflineIndicatorChoiceDescription[];

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

extern const char kOfflinePagesSharingName[];
extern const char kOfflinePagesSharingDescription[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kOfflinePagesShowAlternateDinoPageName[];
extern const char kOfflinePagesShowAlternateDinoPageDescription[];

extern const char kOfflinePagesSvelteConcurrentLoadingName[];
extern const char kOfflinePagesSvelteConcurrentLoadingDescription[];

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kPayWithGoogleV1Name[];
extern const char kPayWithGoogleV1Description[];

extern const char kProgressBarThrottleName[];
extern const char kProgressBarThrottleDescription[];

extern const char kPullToRefreshEffectName[];
extern const char kPullToRefreshEffectDescription[];

extern const char kPwaImprovedSplashScreenName[];
extern const char kPwaImprovedSplashScreenDescription[];
extern const char kPwaPersistentNotificationName[];
extern const char kPwaPersistentNotificationDescription[];

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

extern const char kSimplifiedNtpName[];
extern const char kSimplifiedNtpDescription[];

extern const char kSiteExplorationUiName[];
extern const char kSiteExplorationUiDescription[];

extern const char kSpannableInlineAutocompleteName[];
extern const char kSpannableInlineAutocompleteDescription[];

extern const char kTranslateAndroidManualTriggerName[];
extern const char kTranslateAndroidManualTriggerDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuTypeName[];
extern const char kUpdateMenuTypeDescription[];
extern const char kUpdateMenuTypeNone[];
extern const char kUpdateMenuTypeUpdateAvailable[];
extern const char kUpdateMenuTypeUnsupportedOSVersion[];

extern const char kVrBrowsingTabsViewName[];
extern const char kVrBrowsingTabsViewDescription[];

extern const char kThirdPartyDoodlesName[];
extern const char kThirdPartyDoodlesDescription[];

extern const char kWebXrRenderPathName[];
extern const char kWebXrRenderPathDescription[];
extern const char kWebXrRenderPathChoiceClientWaitDescription[];
extern const char kWebXrRenderPathChoiceGpuFenceDescription[];
extern const char kWebXrRenderPathChoiceSharedBufferDescription[];

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAccountConsistencyName[];
extern const char kAccountConsistencyDescription[];
extern const char kAccountConsistencyChoiceMirror[];
extern const char kAccountConsistencyChoiceDice[];

extern const char kAutofillDropdownLayoutName[];
extern const char kAutofillDropdownLayoutDescription[];

extern const char kAutofillPrimaryInfoStyleExperimentName[];
extern const char kAutofillPrimaryInfoStyleExperimentDescription[];

extern const char kDoodlesOnLocalNtpName[];
extern const char kDoodlesOnLocalNtpDescription[];

extern const char kEnableAudioFocusName[];
extern const char kEnableAudioFocusDescription[];
extern const char kEnableAudioFocusDisabled[];
extern const char kEnableAudioFocusEnabled[];
extern const char kEnableAudioFocusEnabledDuckFlash[];
extern const char kEnableAudioFocusEnabledNoEnforce[];

extern const char kEnableNewAppMenuIconName[];
extern const char kEnableNewAppMenuIconDescription[];

extern const char kEnableWebAuthenticationCtap2SupportName[];
extern const char kEnableWebAuthenticationCtap2SupportDescription[];

extern const char kEnableWebAuthenticationTestingAPIName[];
extern const char kEnableWebAuthenticationTestingAPIDescription[];

extern const char kHappinessTrackingSurveysForDesktopName[];
extern const char kHappinessTrackingSurveysForDesktopDescription[];

extern const char kInfiniteSessionRestoreName[];
extern const char kInfiniteSessionRestoreDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescriptions[];

extern const char kOmniboxRichEntitySuggestionsName[];
extern const char kOmniboxRichEntitySuggestionsDescription[];

extern const char kOmniboxNewAnswerLayoutName[];
extern const char kOmniboxNewAnswerLayoutDescription[];

extern const char kOmniboxReverseAnswersName[];
extern const char kOmniboxReverseAnswersDescription[];

extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

extern const char kOmniboxPedalSuggestionsName[];
extern const char kOmniboxPedalSuggestionsDescription[];

extern const char kOmniboxTailSuggestionsName[];
extern const char kOmniboxTailSuggestionsDescription[];

extern const char kPageAlmostIdleName[];
extern const char kPageAlmostIdleDescription[];

extern const char kProactiveTabFreezeAndDiscardName[];
extern const char kProactiveTabFreezeAndDiscardDescription[];

extern const char kSiteCharacteristicsDatabaseName[];
extern const char kSiteCharacteristicsDatabaseDescription[];

extern const char kUseGoogleLocalNtpName[];
extern const char kUseGoogleLocalNtpDescription[];

#if defined(GOOGLE_CHROME_BUILD)

extern const char kGoogleBrandedContextMenuName[];
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

extern const char kDisablePostscriptPrinting[];
extern const char kDisablePostscriptPrintingDescription[];

extern const char kEnableAppcontainerName[];
extern const char kEnableAppcontainerDescription[];

extern const char kEnableDesktopIosPromotionsName[];
extern const char kEnableDesktopIosPromotionsDescription[];

extern const char kEnableGpuAppcontainerName[];
extern const char kEnableGpuAppcontainerDescription[];

extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

extern const char kIncreaseInputAudioBufferSize[];
extern const char kIncreaseInputAudioBufferSizeDescription[];

extern const char kTraceExportEventsToEtwName[];
extern const char kTraceExportEventsToEtwDesription[];

extern const char kUseAngleName[];
extern const char kUseAngleDescription[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];
extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

extern const char kWindows10CustomTitlebarName[];
extern const char kWindows10CustomTitlebarDescription[];

#endif  // defined(OS_WIN)

// Mac ------------------------------------------------------------------------

#if defined(OS_MACOSX)

extern const char kFullscreenToolbarRevealName[];
extern const char kFullscreenToolbarRevealDescription[];

extern const char kContentFullscreenName[];
extern const char kContentFullscreenDescription[];

extern const char kEnableWebAuthenticationTouchIdName[];
extern const char kEnableWebAuthenticationTouchIdDescription[];

extern const char kHostedAppsInWindowsName[];
extern const char kHostedAppsInWindowsDescription[];

extern const char kCreateAppWindowsInAppShimProcessName[];
extern const char kCreateAppWindowsInAppShimProcessDescription[];

extern const char kMacRTLName[];
extern const char kMacRTLDescription[];

extern const char kMacTouchBarName[];
extern const char kMacTouchBarDescription[];

extern const char kMacV2SandboxName[];
extern const char kMacV2SandboxDescription[];

extern const char kMacViewsNativeAppWindowsName[];
extern const char kMacViewsNativeAppWindowsDescription[];

extern const char kMacViewsTaskManagerName[];
extern const char kMacViewsTaskManagerDescription[];

extern const char kTabDetachingInFullscreenName[];
extern const char kTabDetachingInFullscreenDescription[];

extern const char kTabStripKeyboardFocusName[];
extern const char kTabStripKeyboardFocusDescription[];

extern const char kTextSuggestionsTouchBarName[];
extern const char kTextSuggestionsTouchBarDescription[];

// Non-Mac --------------------------------------------------------------------

#else  // !defined(OS_MACOSX)

extern const char kPermissionPromptPersistenceToggleName[];
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // defined(OS_MACOSX)

// Chrome OS ------------------------------------------------------------------

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAllowTouchpadThreeFingerClickName[];
extern const char kAllowTouchpadThreeFingerClickDescription[];

extern const char kArcAvailableForChildName[];
extern const char kArcAvailableForChildDescription[];

extern const char kArcBootCompleted[];
extern const char kArcBootCompletedDescription[];

extern const char kArcCupsApiName[];
extern const char kArcCupsApiDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcInputMethodName[];
extern const char kArcInputMethodDescription[];

extern const char kArcNativeBridgeExperimentName[];
extern const char kArcNativeBridgeExperimentDescription[];

extern const char kArcUsbHostName[];
extern const char kArcUsbHostDescription[];

extern const char kArcVpnName[];
extern const char kArcVpnDescription[];

extern const char kAshDisableLoginDimAndBlurName[];
extern const char kAshDisableLoginDimAndBlurDescription[];

extern const char kAshDisableSmoothScreenRotationName[];
extern const char kAshDisableSmoothScreenRotationDescription[];

extern const char kAshEnableDisplayMoveWindowAccelsName[];
extern const char kAshEnableDisplayMoveWindowAccelsDescription[];

extern const char kAshEnableKeyboardShortcutViewerName[];
extern const char kAshEnableKeyboardShortcutViewerDescription[];

extern const char kAshEnableMirroredScreenName[];
extern const char kAshEnableMirroredScreenDescription[];

extern const char kAshEnablePersistentWindowBoundsName[];
extern const char kAshEnablePersistentWindowBoundsDescription[];

extern const char kAshEnableTrilinearFilteringName[];
extern const char kAshEnableTrilinearFilteringDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAshKeyboardShortcutViewerAppName[];
extern const char kAshKeyboardShortcutViewerAppDescription[];

extern const char kAshShelfColorName[];
extern const char kAshShelfColorDescription[];

extern const char kAshShelfColorScheme[];
extern const char kAshShelfColorSchemeDescription[];
extern const char kAshShelfColorSchemeLightVibrant[];
extern const char kAshShelfColorSchemeNormalVibrant[];
extern const char kAshShelfColorSchemeDarkVibrant[];
extern const char kAshShelfColorSchemeLightMuted[];
extern const char kAshShelfColorSchemeNormalMuted[];
extern const char kAshShelfColorSchemeDarkMuted[];

extern const char kBulkPrintersName[];
extern const char kBulkPrintersDescription[];

extern const char kCaptivePortalBypassProxyName[];
extern const char kCaptivePortalBypassProxyDescription[];

extern const char kChromeVoxArcSupportName[];
extern const char kChromeVoxArcSupportDescription[];

extern const char kCrOSComponentName[];
extern const char kCrOSComponentDescription[];

extern const char kCrOSContainerName[];
extern const char kCrOSContainerDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kCrostiniFilesName[];
extern const char kCrostiniFilesDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisableLockScreenAppsName[];
extern const char kDisableLockScreenAppsDescription[];

extern const char kDisableSystemTimezoneAutomaticDetectionName[];
extern const char kDisableSystemTimezoneAutomaticDetectionDescription[];

extern const char kDisableTabletAutohideTitlebarsName[];
extern const char kDisableTabletAutohideTitlebarsDescription[];

extern const char kDisableTabletSplitViewName[];
extern const char kDisableTabletSplitViewDescription[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kEnableAppListSearchAutocompleteName[];
extern const char kEnableAppListSearchAutocompleteDescription[];

extern const char kEnableAppShortcutSearchName[];
extern const char kEnableAppShortcutSearchDescription[];

extern const char kEnableAppDataSearchName[];
extern const char kEnableAppDataSearchDescription[];

extern const char kEnableAppsGridGapFeatureName[];
extern const char kEnableAppsGridGapFeatureDescription[];

extern const char kEnableArcUnifiedAudioFocusName[];
extern const char kEnableArcUnifiedAudioFocusDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnableChromeOsAccountManagerName[];
extern const char kEnableChromeOsAccountManagerDescription[];

extern const char kEnableContinueReadingName[];
extern const char kEnableContinueReadingDescription[];

extern const char kEnableDragAppsInTabletModeName[];
extern const char kEnableDragAppsInTabletModeDescription[];

extern const char kEnableDragTabsInTabletModeName[];
extern const char kEnableDragTabsInTabletModeDescription[];

extern const char kEnableDriveFsName[];
extern const char kEnableDriveFsDescription[];

extern const char kEnableEhvInputName[];
extern const char kEnableEhvInputDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

extern const char kEnableFloatingVirtualKeyboardName[];
extern const char kEnableFloatingVirtualKeyboardDescription[];

extern const char kEnableFullscreenHandwritingVirtualKeyboardName[];
extern const char kEnableFullscreenHandwritingVirtualKeyboardDescription[];

extern const char kEnableGoogleAssistantName[];
extern const char kEnableGoogleAssistantDescription[];

extern const char kEnableHomeLauncherName[];
extern const char kEnableHomeLauncherDescription[];

extern const char kEnableHomeLauncherGesturesName[];
extern const char kEnableHomeLauncherGesturesDescription[];

extern const char kEnableImeMenuName[];
extern const char kEnableImeMenuDescription[];

extern const char kEnableMediaSessionAshMediaKeysName[];
extern const char kEnableMediaSessionAshMediaKeysDescription[];

extern const char kEnableNewStyleLauncherName[];
extern const char kEnableNewStyleLauncherDescription[];

extern const char kEnableOobeRecommendAppsScreenName[];
extern const char kEnableOobeRecommendAppsScreenDescription[];

extern const char kEnablePerUserTimezoneName[];
extern const char kEnablePerUserTimezoneDescription[];

extern const char kEnablePlayStoreSearchName[];
extern const char kEnablePlayStoreSearchDescription[];

extern const char kEnableSettingsShortcutSearchName[];
extern const char kEnableSettingsShortcutSearchDescription[];

extern const char kEnableStylusVirtualKeyboardName[];
extern const char kEnableStylusVirtualKeyboardDescription[];

extern const char kEnableUnifiedMultiDeviceSettingsName[];
extern const char kEnableUnifiedMultiDeviceSettingsDescription[];

extern const char kEnableUnifiedMultiDeviceSetupName[];
extern const char kEnableUnifiedMultiDeviceSetupDescription[];

extern const char kEnableVirtualKeyboardMdUiName[];
extern const char kEnableVirtualKeyboardMdUiDescription[];

extern const char kEnableVirtualKeyboardUkmName[];
extern const char kEnableVirtualKeyboardUkmDescription[];

extern const char kEnableZeroStateSuggestionsName[];
extern const char kEnableZeroStateSuggestionsDescription[];

extern const char kEolNotificationName[];
extern const char kEolNotificationDescription[];

extern const char kExperimentalAccessibilityFeaturesName[];
extern const char kExperimentalAccessibilityFeaturesDescription[];

extern const char kFileManagerTouchModeName[];
extern const char kFileManagerTouchModeDescription[];

extern const char kFirstRunUiTransitionsName[];
extern const char kFirstRunUiTransitionsDescription[];

extern const char kForceEnableStylusToolsName[];
extern const char kForceEnableStylusToolsDescription[];

extern const char kGestureEditingName[];
extern const char kGestureEditingDescription[];

extern const char kGestureTypingName[];
extern const char kGestureTypingDescription[];

extern const char kInputViewName[];
extern const char kInputViewDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMaterialDesignInkDropAnimationSpeedName[];
extern const char kMaterialDesignInkDropAnimationSpeedDescription[];
extern const char kMaterialDesignInkDropAnimationFast[];
extern const char kMaterialDesignInkDropAnimationSlow[];

extern const char kMemoryPressureThresholdName[];
extern const char kMemoryPressureThresholdDescription[];
extern const char kConservativeThresholds[];
extern const char kAggressiveCacheDiscardThresholds[];
extern const char kAggressiveTabDiscardThresholds[];
extern const char kAggressiveThresholds[];

extern const char kMtpWriteSupportName[];
extern const char kMtpWriteSupportDescription[];

extern const char kMultiDeviceApiName[];
extern const char kMultiDeviceApiDescription[];

extern const char kNativeSmbName[];
extern const char kNativeSmbDescription[];

extern const char kNetworkPortalNotificationName[];
extern const char kNetworkPortalNotificationDescription[];

extern const char kNewKoreanImeName[];
extern const char kNewKoreanImeDescription[];

extern const char kNewZipUnpackerName[];
extern const char kNewZipUnpackerDescription[];

extern const char kOfficeEditingComponentAppName[];
extern const char kOfficeEditingComponentAppDescription[];

extern const char kPhysicalKeyboardAutocorrectName[];
extern const char kPhysicalKeyboardAutocorrectDescription[];

extern const char kPrinterProviderSearchAppName[];
extern const char kPrinterProviderSearchAppDescription[];

extern const char kQuickUnlockPinName[];
extern const char kQuickUnlockPinDescription[];
extern const char kQuickUnlockPinSignin[];
extern const char kQuickUnlockPinSigninDescription[];
extern const char kQuickUnlockFingerprint[];
extern const char kQuickUnlockFingerprintDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSingleProcessMashName[];
extern const char kSingleProcessMashDescription[];

extern const char kSlideTopChromeWithPageScrollsName[];
extern const char kSlideTopChromeWithPageScrollsDescription[];

extern const char kSmartTextSelectionName[];
extern const char kSmartTextSelectionDescription[];

extern const char kUiShowCompositedLayerBordersName[];
extern const char kUiShowCompositedLayerBordersDescription[];
extern const char kUiShowCompositedLayerBordersRenderPass[];
extern const char kUiShowCompositedLayerBordersSurface[];
extern const char kUiShowCompositedLayerBordersLayer[];
extern const char kUiShowCompositedLayerBordersAll[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiModeName[];
extern const char kUiModeDescription[];
extern const char kUiModeTablet[];
extern const char kUiModeClamshell[];
extern const char kUiModeAuto[];

extern const char kUnfilteredBluetoothDevicesName[];
extern const char kUnfilteredBluetoothDevicesDescription[];

extern const char kUsbguardName[];
extern const char kUsbguardDescription[];

extern const char kShillSandboxingName[];
extern const char kShillSandboxingDescription[];

extern const char kUseMashName[];
extern const char kUseMashDescription[];

extern const char kUseMonitorColorSpaceName[];
extern const char kUseMonitorColorSpaceDescription[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVideoPlayerChromecastSupportName[];
extern const char kVideoPlayerChromecastSupportDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardOverscrollName[];
extern const char kVirtualKeyboardOverscrollDescription[];

extern const char kVoiceInputName[];
extern const char kVoiceInputDescription[];

extern const char kWakeOnPacketsName[];
extern const char kWakeOnPacketsDescription[];

#endif  // #if defined(OS_CHROMEOS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX)

extern const char kEnableInputImeApiName[];
extern const char kEnableInputImeApiDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

extern const char kExperimentalUiName[];
extern const char kExperimentalUiDescription[];

#if defined(OS_WIN) || defined(OS_MACOSX)

extern const char kAutomaticTabDiscardingName[];
extern const char kAutomaticTabDiscardingDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

extern const char kDirectManipulationStylusName[];
extern const char kDirectManipulationStylusDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

// Feature flags --------------------------------------------------------------

#if BUILDFLAG(ENABLE_VR)

extern const char kWebVrVsyncAlignName[];
extern const char kWebVrVsyncAlignDescription[];

#if BUILDFLAG(ENABLE_OCULUS_VR)
extern const char kOculusVRName[];
extern const char kOculusVRDescription[];
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
extern const char kOpenVRName[];
extern const char kOpenVRDescription[];
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
extern const char kXRSandboxName[];
extern const char kXRSandboxDescription[];
#endif  // ENABLE_ISOLATED_XR_SERVICE

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_NACL)

extern const char kNaclDebugMaskName[];
extern const char kNaclDebugMaskDescription[];
extern const char kNaclDebugMaskChoiceDebugAll[];
extern const char kNaclDebugMaskChoiceExcludeUtilsPnacl[];
extern const char kNaclDebugMaskChoiceIncludeDebug[];

extern const char kNaclDebugName[];
extern const char kNaclDebugDescription[];

extern const char kNaclName[];
extern const char kNaclDescription[];

extern const char kPnaclSubzeroName[];
extern const char kPnaclSubzeroDescription[];

#endif  // BUILDFLAG(ENABLE_NACL)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kDisplayCutoutAPIName[];
extern const char kDisplayCutoutAPIDescription[];

#endif  // defined(OS_ANDROID)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
