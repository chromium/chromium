// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
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

extern const char kEnableMediaInternalsName[];
extern const char kEnableMediaInternalsDescription[];

#if BUILDFLAG(ENABLE_PDF)
extern const char kAccessiblePDFFormName[];
extern const char kAccessiblePDFFormDescription[];
#endif

extern const char kAccountIdMigrationName[];
extern const char kAccountIdMigrationDescription[];

extern const char kAddPasswordsInSettingsName[];
extern const char kAddPasswordsInSettingsDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAnimatedImageResumeName[];
extern const char kAnimatedImageResumeDescription[];

extern const char kAriaElementReflectionName[];
extern const char kAriaElementReflectionDescription[];

extern const char kCOLRV1FontsDescription[];

extern const char kCSSCascadeLayersName[];
extern const char kCSSCascadeLayersDescription[];

extern const char kCSSContainerQueriesName[];
extern const char kCSSContainerQueriesDescription[];

extern const char kContentLanguagesInLanguagePickerName[];
extern const char kContentLanguagesInLanguagePickerDescription[];

extern const char kConversionMeasurementDebugModeName[];
extern const char kConversionMeasurementDebugModeDescription[];

extern const char kDebugHistoryInterventionNoUserActivationName[];
extern const char kDebugHistoryInterventionNoUserActivationDescription[];

extern const char kDefaultChromeAppsMigrationName[];
extern const char kDefaultChromeAppsMigrationDescription[];

extern const char kDeferredFontShapingName[];
extern const char kDeferredFontShapingDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

extern const char kLauncherAppSortName[];
extern const char kLauncherAppSortDescription[];

extern const char kLeakDetectionUnauthenticated[];
extern const char kLeakDetectionUnauthenticatedDescription[];

extern const char kDetectFormSubmissionOnFormClearName[];
extern const char kDetectFormSubmissionOnFormClearDescription[];

extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

extern const char kMuteCompromisedPasswordsName[];
extern const char kMuteCompromisedPasswordsDescription[];

extern const char kPasswordNotesName[];
extern const char kPasswordNotesDescription[];

extern const char kEnableBluetoothSerialPortProfileInSerialApiName[];
extern const char kEnableBluetoothSerialPortProfileInSerialApiDescription[];

extern const char kWebBluetoothBondOnDemandName[];
extern const char kWebBluetoothBondOnDemandDescription[];

extern const char kEnableDrDcName[];
extern const char kEnableDrDcDescription[];

extern const char kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedName[];
extern const char
    kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedDescription[];

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kWebFilterInterstitialRefreshName[];
extern const char kWebFilterInterstitialRefreshDescription[];
#endif  // ENABLE_SUPERVISED_USERS

extern const char kU2FPermissionPromptName[];
extern const char kU2FPermissionPromptDescription[];

extern const char kU2FSecurityKeyAPIName[];
extern const char kU2FSecurityKeyAPIDescription[];

extern const char kUpcomingSharingFeaturesName[];
extern const char kUpcomingSharingFeaturesDescription[];

extern const char kUseStorkSmdsServerAddressName[];
extern const char kUseStorkSmdsServerAddressDescription[];

extern const char kUseWallpaperStagingUrlName[];
extern const char kUseWallpaperStagingUrlDescription[];

extern const char kSemanticColorsDebugOverrideName[];
extern const char kSemanticColorsDebugOverrideDescription[];

extern const char kUseMessagesStagingUrlName[];
extern const char kUseMessagesStagingUrlDescription[];

extern const char kUseCustomMessagesDomainName[];
extern const char kUseCustomMessagesDomainDescription[];

extern const char kAndroidPictureInPictureAPIName[];
extern const char kAndroidPictureInPictureAPIDescription[];

extern const char kAssistantConsentModalName[];
extern const char kAssistantConsentModalDescription[];

extern const char kAssistantConsentSimplifiedTextName[];
extern const char kAssistantConsentSimplifiedTextDescription[];

extern const char kAssistantConsentV2Name[];
extern const char kAssistantConsentV2Description[];

extern const char kEnableAutoDisableAccessibilityName[];
extern const char kEnableAutoDisableAccessibilityDescription[];

extern const char kAutofillAlwaysReturnCloudTokenizedCardName[];
extern const char kAutofillAlwaysReturnCloudTokenizedCardDescription[];

extern const char kAutofillAutoTriggerManualFallbackForCardsName[];
extern const char kAutofillAutoTriggerManualFallbackForCardsDescription[];

extern const char kAutofillCenterAligngedSuggestionsName[];
extern const char kAutofillCenterAligngedSuggestionsDescription[];

extern const char kAutofillVisualImprovementsForSuggestionUiName[];
extern const char kAutofillVisualImprovementsForSuggestionUiDescription[];

extern const char kAutofillTypeSpecificPopupWidthName[];
extern const char kAutofillTypeSpecificPopupWidthDescription[];

extern const char kAutofillEnableGoogleIssuedCardName[];
extern const char kAutofillEnableGoogleIssuedCardDescription[];

extern const char kAutofillEnableMerchantBoundVirtualCardsName[];
extern const char kAutofillEnableMerchantBoundVirtualCardsDescription[];

extern const char kAutofillEnableOfferNotificationForPromoCodesName[];
extern const char kAutofillEnableOfferNotificationForPromoCodesDescription[];

extern const char kAutofillEnableOffersInClankKeyboardAccessoryName[];
extern const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[];

extern const char kAutofillEnableSendingBcnInGetUploadDetailsName[];
extern const char kAutofillEnableSendingBcnInGetUploadDetailsDescription[];

extern const char kAutofillEnableStickyManualFallbackForCardsName[];
extern const char kAutofillEnableStickyManualFallbackForCardsDescription[];

extern const char kAutofillEnableToolbarStatusChipName[];
extern const char kAutofillEnableToolbarStatusChipDescription[];

extern const char kAutofillEnableUnmaskCardRequestSetInstrumentIdName[];
extern const char kAutofillEnableUnmaskCardRequestSetInstrumentIdDescription[];

extern const char kAutofillEnableUpdateVirtualCardEnrollmentName[];
extern const char kAutofillEnableUpdateVirtualCardEnrollmentDescription[];

extern const char kAutofillEnableVirtualCardName[];
extern const char kAutofillEnableVirtualCardDescription[];

extern const char
    kAutofillEnableVirtualCardManagementInDesktopSettingsPageName[];
extern const char
    kAutofillEnableVirtualCardManagementInDesktopSettingsPageDescription[];

extern const char kAutofillEnableVirtualCardsRiskBasedAuthenticationName[];
extern const char
    kAutofillEnableVirtualCardsRiskBasedAuthenticationDescription[];

extern const char kAutofillFillMerchantPromoCodeFieldsName[];
extern const char kAutofillFillMerchantPromoCodeFieldsDescription[];

extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[];
extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[];

extern const char kAutofillParseMerchantPromoCodeFieldsName[];
extern const char kAutofillParseMerchantPromoCodeFieldsDescription[];

extern const char kAutofillSaveAndFillVPAName[];
extern const char kAutofillSaveAndFillVPADescription[];

extern const char kAutofillSuggestVirtualCardsOnIncompleteFormName[];
extern const char kAutofillSuggestVirtualCardsOnIncompleteFormDescription[];

extern const char kAutofillUseConsistentPopupSettingsIconsName[];
extern const char kAutofillUseConsistentPopupSettingsIconsDescription[];

extern const char kAutofillUseImprovedLabelDisambiguationName[];
extern const char kAutofillUseImprovedLabelDisambiguationDescription[];

extern const char kAutoScreenBrightnessName[];
extern const char kAutoScreenBrightnessDescription[];

extern const char kBackForwardCacheName[];
extern const char kBackForwardCacheDescription[];

extern const char kEnableBackForwardCacheForScreenReaderName[];
extern const char kEnableBackForwardCacheForScreenReaderDescription[];

extern const char kBentoBarName[];
extern const char kBentoBarDescription[];

extern const char kDragWindowToNewDeskName[];
extern const char kDragWindowToNewDeskDescription[];

extern const char kBiometricReauthForPasswordFillingName[];
extern const char kBiometricReauthForPasswordFillingDescription[];

extern const char kTouchToFillPasswordSubmissionName[];
extern const char kTouchToFillPasswordSubmissionDescription[];

extern const char kBorealisBigGlName[];
extern const char kBorealisBigGlDescription[];

extern const char kBorealisDiskManagementName[];
extern const char kBorealisDiskManagementDescription[];

extern const char kBorealisForceBetaClientName[];
extern const char kBorealisForceBetaClientDescription[];

extern const char kBorealisLinuxModeName[];
extern const char kBorealisLinuxModeDescription[];

extern const char kBorealisPermittedName[];
extern const char kBorealisPermittedDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kCanvasOopRasterizationName[];
extern const char kCanvasOopRasterizationDescription[];

extern const char kCertificateTransparency2022PolicyName[];
extern const char kCertificateTransparency2022PolicyDescription[];

extern const char kCertificateTransparency2022PolicyAllCertsName[];
extern const char kCertificateTransparency2022PolicyAllCertsDescription[];

extern const char kCheckOfflineCapabilityName[];
extern const char kCheckOfflineCapabilityDescription[];

extern const char kChromeLabsName[];
extern const char kChromeLabsDescription[];

extern const char kConsolidatedSiteStorageControlsName[];
extern const char kConsolidatedSiteStorageControlsDescription[];

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

extern const char kClientStorageAccessContextAuditingName[];
extern const char kClientStorageAccessContextAuditingDescription[];

extern const char kCloseAllTabsModalDialogName[];
extern const char kCloseAllTabsModalDialogDescription[];

extern const char kClosedTabCacheName[];
extern const char kClosedTabCacheDescription[];

extern const char kConditionalTabStripAndroidName[];
extern const char kConditionalTabStripAndroidDescription[];

extern const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[];
extern const char
    kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kChromeTipsInMainMenuName[];
extern const char kChromeTipsInMainMenuDescription[];

extern const char kChromeTipsInMainMenuNewBadgeName[];
extern const char kChromeTipsInMainMenuNewBadgeDescription[];
#endif

extern const char kChromeWhatsNewUIName[];
extern const char kChromeWhatsNewUIDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kChromeWhatsNewInMainMenuNewBadgeName[];
extern const char kChromeWhatsNewInMainMenuNewBadgeDescription[];
#endif

extern const char kDarkLightTestName[];
extern const char kDarkLightTestDescription[];

extern const char kDisableProcessReuse[];
extern const char kDisableProcessReuseDescription[];

extern const char kDiscountConsentV2Name[];
extern const char kDiscountConsentV2Description[];

extern const char kDoubleBufferCompositingName[];
extern const char kDoubleBufferCompositingDescription[];

extern const char kDnsOverHttpsName[];
extern const char kDnsOverHttpsDescription[];

extern const char kDnsHttpssvcName[];
extern const char kDnsHttpssvcDescription[];

extern const char kEditContextName[];
extern const char kEditContextDescription[];

extern const char kEnableAutomaticSnoozeName[];
extern const char kEnableAutomaticSnoozeDescription[];

extern const char kEnableFirstPartySetsName[];
extern const char kEnableFirstPartySetsDescription[];

extern const char kIsolatedSandboxedIframesName[];
extern const char kIsolatedSandboxedIframesDescription[];

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

extern const char kDocumentTransitionName[];
extern const char kDocumentTransitionDescription[];

extern const char kDocumentTransitionSlowdownFactorName[];
extern const char kDocumentTransitionSlowdownFactorDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableAutofillCreditCardAuthenticationName[];
extern const char kEnableAutofillCreditCardAuthenticationDescription[];

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

extern const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterName[];
extern const char
    kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription[];

extern const char kEnableExperimentalCookieFeaturesName[];
extern const char kEnableExperimentalCookieFeaturesDescription[];

extern const char kEnableNavigationPredictorName[];
extern const char kEnableNavigationPredictorDescription[];

extern const char kEnablePreconnectToSearchName[];
extern const char kEnablePreconnectToSearchDescription[];

extern const char kEnableRawDrawName[];
extern const char kEnableRawDrawDescription[];

extern const char kEnableDelegatedCompositingName[];
extern const char kEnableDelegatedCompositingDescription[];

extern const char kEnableRemovingAllThirdPartyCookiesName[];
extern const char kEnableRemovingAllThirdPartyCookiesDescription[];

extern const char kEnableBrowsingDataLifetimeManagerName[];
extern const char kEnableBrowsingDataLifetimeManagerDescription[];

extern const char kColorProviderRedirectionForThemeProviderName[];
extern const char kColorProviderRedirectionForThemeProviderDescription[];

extern const char kDesktopPWAsAdditionalWindowingControlsName[];
extern const char kDesktopPWAsAdditionalWindowingControlsDescription[];

extern const char kDesktopPWAsPrefixAppNameInWindowTitleName[];
extern const char kDesktopPWAsPrefixAppNameInWindowTitleDescription[];

extern const char kDesktopPWAsRemoveStatusBarName[];
extern const char kDesktopPWAsRemoveStatusBarDescription[];

extern const char kDesktopPWAsDefaultOfflinePageName[];
extern const char kDesktopPWAsDefaultOfflinePageDescription[];

extern const char kDesktopPWAsElidedExtensionsMenuName[];
extern const char kDesktopPWAsElidedExtensionsMenuDescription[];

extern const char kDesktopPWAsNotificationIconAndTitleName[];
extern const char kDesktopPWAsNotificationIconAndTitleDescription[];

extern const char kDesktopPWAsLaunchHandlerName[];
extern const char kDesktopPWAsLaunchHandlerDescription[];

extern const char kDesktopPWAsLinkCapturingName[];
extern const char kDesktopPWAsLinkCapturingDescription[];

extern const char kDesktopPWAsManifestIdName[];
extern const char kDesktopPWAsManifestIdDescription[];

extern const char kDesktopPWAsTabStripName[];
extern const char kDesktopPWAsTabStripDescription[];

extern const char kDesktopPWAsTabStripLinkCapturingName[];
extern const char kDesktopPWAsTabStripLinkCapturingDescription[];

extern const char kDesktopPWAsTabStripSettingsName[];
extern const char kDesktopPWAsTabStripSettingsDescription[];

extern const char kDesktopPWAsSubAppsName[];
extern const char kDesktopPWAsSubAppsDescription[];

extern const char kDesktopPWAsUrlHandlingName[];
extern const char kDesktopPWAsUrlHandlingDescription[];

extern const char kDesktopPWAsWebBundlesName[];
extern const char kDesktopPWAsWebBundlesDescription[];

extern const char kDesktopPWAsWindowControlsOverlayName[];
extern const char kDesktopPWAsWindowControlsOverlayDescription[];

extern const char kDeviceForceScheduledRebootName[];
extern const char kDeviceForceScheduledRebootDescription[];

extern const char kDevicePostureName[];
extern const char kDevicePostureDescription[];

extern const char kRestrictedApiOriginsName[];
extern const char kRestrictedApiOriginsDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription[];

extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName[];
extern const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription[];

extern const char kEnablePreinstalledWebAppDuplicationFixerName[];
extern const char kEnablePreinstalledWebAppDuplicationFixerDescription[];

extern const char kEnableSyncRequiresPoliciesLoadedName[];
extern const char kEnableSyncRequiresPoliciesLoadedDescription[];

extern const char kEnableTLS13EarlyDataName[];
extern const char kEnableTLS13EarlyDataDescription[];

extern const char kEnhancedNetworkVoicesName[];
extern const char kEnhancedNetworkVoicesDescription[];

extern const char kAccessibilityOSSettingsVisibilityName[];
extern const char kAccessibilityOSSettingsVisibilityDescription[];

extern const char kPostQuantumCECPQ2Name[];
extern const char kPostQuantumCECPQ2Description[];

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

extern const char kEnableLazyFrameLoadingName[];
extern const char kEnableLazyFrameLoadingDescription[];

extern const char kEnableLazyImageLoadingName[];
extern const char kEnableLazyImageLoadingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableTranslateSubFramesName[];
extern const char kEnableTranslateSubFramesDescription[];

extern const char kEnableWindowsGamingInputDataFetcherName[];
extern const char kEnableWindowsGamingInputDataFetcherDescription[];

extern const char kBlockInsecurePrivateNetworkRequestsName[];
extern const char kBlockInsecurePrivateNetworkRequestsDescription[];

extern const char kPrivateNetworkAccessSendPreflightsName[];
extern const char kPrivateNetworkAccessSendPreflightsDescription[];

extern const char kPrivateNetworkAccessRespectPreflightResultsName[];
extern const char kPrivateNetworkAccessRespectPreflightResultsDescription[];

extern const char kDeprecateAltClickName[];
extern const char kDeprecateAltClickDescription[];

extern const char kDeprecateAltBasedSixPackName[];
extern const char kDeprecateAltBasedSixPackDescription[];

extern const char kDiagnosticsAppNavigationName[];
extern const char kDiagnosticsAppNavigationDescription[];

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
extern const char kMemlogStackModeNative[];
extern const char kMemlogStackModeNativeWithThreadNames[];

extern const char kDownloadAutoResumptionNativeName[];
extern const char kDownloadAutoResumptionNativeDescription[];

extern const char kDownloadBubbleName[];
extern const char kDownloadBubbleDescription[];

extern const char kDownloadLaterName[];
extern const char kDownloadLaterDescription[];

extern const char kDownloadLaterDebugOnWifiName[];
extern const char kDownloadLaterDebugOnWifiNameDescription[];

extern const char kDownloadRangeName[];
extern const char kDownloadRangeDescription[];

extern const char kEnableFencedFramesName[];
extern const char kEnableFencedFramesDescription[];

extern const char kEnableFirmwareUpdaterAppName[];
extern const char kEnableFirmwareUpdaterAppDescription[];

extern const char kEnableGamepadButtonAxisEventsName[];
extern const char kEnableGamepadButtonAxisEventsDescription[];

extern const char kEnableLensStandaloneFlagId[];
extern const char kEnableLensStandaloneName[];
extern const char kEnableLensStandaloneDescription[];

extern const char kEnableLoginDetectionName[];
extern const char kEnableLoginDetectionDescription[];

extern const char kEnableManagedConfigurationWebApiName[];
extern const char kEnableManagedConfigurationWebApiDescription[];

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePenetratingImageSelectionName[];
extern const char kEnablePenetratingImageSelectionDescription[];

extern const char kEnablePortalsName[];
extern const char kEnablePortalsDescription[];

extern const char kEnablePortalsCrossOriginName[];
extern const char kEnablePortalsCrossOriginDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableRgbKeyboardName[];
extern const char kEnableRgbKeyboardDescription[];

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

extern const char kRetailCouponsName[];
extern const char kRetailCouponsDescription[];

extern const char kEnableCssSelectorFragmentAnchorName[];
extern const char kEnableCssSelectorFragmentAnchorDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];
extern const char kEnableResamplingScrollEventsExperimentalPredictionName[];
extern const char
    kEnableResamplingScrollEventsExperimentalPredictionDescription[];

extern const char kEnableRestrictedWebApisName[];
extern const char kEnableRestrictedWebApisDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kEnableWebAuthenticationCableDiscoCredsName[];
extern const char kEnableWebAuthenticationCableDiscoCredsDescription[];

extern const char kEnableWebAuthenticationChromeOSAuthenticatorName[];
extern const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[];

extern const char kEnableZeroCopyTabCaptureName[];
extern const char kEnableZeroCopyTabCaptureDescription[];

extern const char kExperimentalWebAssemblyFeaturesName[];
extern const char kExperimentalWebAssemblyFeaturesDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmLazyCompilationName[];
extern const char kEnableWasmLazyCompilationDescription[];

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

extern const char kExtensionsMenuAccessControlName[];
extern const char kExtensionsMenuAccessControlDescription[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

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

extern const char kFedCmName[];
extern const char kFedCmDescription[];

extern const char kFileHandlingAPIName[];
extern const char kFileHandlingAPIDescription[];

extern const char kFileHandlingIconsName[];
extern const char kFileHandlingIconsDescription[];

extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFullUserAgentName[];
extern const char kFullUserAgentDescription[];

extern const char kGlobalMediaControlsModernUIName[];
extern const char kGlobalMediaControlsModernUIDescription[];

extern const char kOpenscreenCastStreamingSessionName[];
extern const char kOpenscreenCastStreamingSessionDescription[];

extern const char kCastStreamingAv1Name[];
extern const char kCastStreamingAv1Description[];

extern const char kCastStreamingVp9Name[];
extern const char kCastStreamingVp9Description[];

extern const char kCastUseBlocklistForRemotingQueryName[];
extern const char kCastUseBlocklistForRemotingQueryDescription[];

extern const char kCastForceEnableRemotingQueryName[];
extern const char kCastForceEnableRemotingQueryDescription[];

extern const char kGoogleLensSdkIntentName[];
extern const char kGoogleLensSdkIntentDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];

extern const char kHandwritingGestureEditingName[];
extern const char kHandwritingGestureEditingDescription[];

extern const char kHandwritingLegacyRecognitionName[];
extern const char kHandwritingLegacyRecognitionDescription[];

extern const char kHandwritingLegacyRecognitionAllLangName[];
extern const char kHandwritingLegacyRecognitionAllLangDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHeavyAdPrivacyMitigationsName[];
extern const char kHeavyAdPrivacyMitigationsDescription[];

extern const char kHeavyAdInterventionName[];
extern const char kHeavyAdInterventionDescription[];

extern const char kTabAudioMutingName[];
extern const char kTabAudioMutingDescription[];

extern const char kTabSearchMediaTabsId[];
extern const char kTabSearchMediaTabsName[];
extern const char kTabSearchMediaTabsDescription[];

extern const char kTabSwitcherOnReturnName[];
extern const char kTabSwitcherOnReturnDescription[];

extern const char kHideShelfControlsInTabletModeName[];
extern const char kHideShelfControlsInTabletModeDescription[];

extern const char kHttpsOnlyModeName[];
extern const char kHttpsOnlyModeDescription[];

extern const char kIgnoreGpuBlocklistName[];
extern const char kIgnoreGpuBlocklistDescription[];

extern const char kImprovedDesksKeyboardShortcutsName[];
extern const char kImprovedDesksKeyboardShortcutsDescription[];

extern const char kImprovedKeyboardShortcutsName[];
extern const char kImprovedKeyboardShortcutsDescription[];

extern const char kCompositorThreadedScrollbarScrollingName[];
extern const char kCompositorThreadedScrollbarScrollingDescription[];

extern const char kImpulseScrollAnimationsName[];
extern const char kImpulseScrollAnimationsDescription[];

extern const char kIncognitoBrandConsistencyForAndroidName[];
extern const char kIncognitoBrandConsistencyForAndroidDescription[];

extern const char kIncognitoReauthenticationForAndroidName[];
extern const char kIncognitoReauthenticationForAndroidDescription[];

extern const char kIncognitoDownloadsWarningName[];
extern const char kIncognitoDownloadsWarningDescription[];

extern const char kUpdateHistoryEntryPointsInIncognitoName[];
extern const char kUpdateHistoryEntryPointsInIncognitoDescription[];

extern const char kIncognitoNtpRevampName[];
extern const char kIncognitoNtpRevampDescription[];

extern const char kIncognitoScreenshotName[];
extern const char kIncognitoScreenshotDescription[];

extern const char kInitialNavigationEntryName[];
extern const char kInitialNavigationEntryDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kInProductHelpSnoozeName[];
extern const char kInProductHelpSnoozeDescription[];

extern const char kInProductHelpUseClientConfigName[];
extern const char kInProductHelpUseClientConfigDescription[];

extern const char kInstalledAppsInCbdName[];
extern const char kInstalledAppsInCbdDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kJourneysName[];
extern const char kJourneysDescription[];

extern const char kJourneysOmniboxActionName[];
extern const char kJourneysOmniboxActionDescription[];

extern const char kJourneysOnDeviceClusteringBackendName[];
extern const char kJourneysOnDeviceClusteringBackendDescription[];

extern const char kLargeFaviconFromGoogleName[];
extern const char kLargeFaviconFromGoogleDescription[];

extern const char kLensCameraAssistedSearchName[];
extern const char kLensCameraAssistedSearchDescription[];

extern const char kLocationBarModelOptimizationsName[];
extern const char kLocationBarModelOptimizationsDescription[];

extern const char kLogJsConsoleMessagesName[];
extern const char kLogJsConsoleMessagesDescription[];

extern const char kUnthrottledNestedTimeoutName[];
extern const char kUnthrottledNestedTimeoutDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMediaSessionWebRTCName[];
extern const char kMediaSessionWebRTCDescription[];

extern const char kMetricsSettingsAndroidName[];
extern const char kMetricsSettingsAndroidDescription[];

extern const char kMojoLinuxChannelSharedMemName[];
extern const char kMojoLinuxChannelSharedMemDescription[];

extern const char kMouseSubframeNoImplicitCaptureName[];
extern const char kMouseSubframeNoImplicitCaptureDescription[];

extern const char kUsernameFirstFlowName[];
extern const char kUsernameFirstFlowDescription[];

extern const char kUsernameFirstFlowFallbackCrowdsourcingName[];
extern const char kUsernameFirstFlowFallbackCrowdsourcingDescription[];

extern const char kUsernameFirstFlowFillingName[];
extern const char kUsernameFirstFlowFillingDescription[];

extern const char kCanvas2DLayersName[];
extern const char kCanvas2DLayersDescription[];

extern const char kDestroyProfileOnBrowserCloseName[];
extern const char kDestroyProfileOnBrowserCloseDescription[];

extern const char kDestroySystemProfilesName[];
extern const char kDestroySystemProfilesDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNotificationsRevampName[];
extern const char kNotificationsRevampDescription[];

extern const char kNotificationSchedulerName[];
extern const char kNotificationSchedulerDescription[];

extern const char kNotificationSchedulerDebugOptionName[];
extern const char kNotificationSchedulerDebugOptionDescription[];
extern const char kNotificationSchedulerImmediateBackgroundTaskDescription[];

extern const char kNotificationsSystemFlagName[];
extern const char kNotificationsSystemFlagDescription[];

extern const char kOriginAgentClusterDefaultName[];
extern const char kOriginAgentClusterDefaultDescription[];

extern const char kOmitCorsClientCertName[];
extern const char kOmitCorsClientCertDescription[];

extern const char kOmniboxActiveSearchEnginesName[];
extern const char kOmniboxActiveSearchEnginesDescription[];

extern const char kOmniboxAdaptiveSuggestionsCountName[];
extern const char kOmniboxAdaptiveSuggestionsCountDescription[];

extern const char kOmniboxAggregateShortcutsName[];
extern const char kOmniboxAggregateShortcutsDescription[];

extern const char kOmniboxAssistantVoiceSearchName[];
extern const char kOmniboxAssistantVoiceSearchDescription[];

extern const char kOmniboxBlurWithEscapeName[];
extern const char kOmniboxBlurWithEscapeDescription[];

extern const char kOmniboxBookmarkPathsName[];
extern const char kOmniboxBookmarkPathsDescription[];

extern const char kOmniboxClobberTriggersContextualWebZeroSuggestName[];
extern const char kOmniboxClobberTriggersContextualWebZeroSuggestDescription[];

extern const char kOmniboxClosePopupWithEscapeName[];
extern const char kOmniboxClosePopupWithEscapeDescription[];

extern const char kOmniboxDisableCGIParamMatchingName[];
extern const char kOmniboxDisableCGIParamMatchingDescription[];

extern const char kOmniboxDocumentProviderAsoName[];
extern const char kOmniboxDocumentProviderAsoDescription[];

extern const char kOmniboxExperimentalSuggestScoringName[];
extern const char kOmniboxExperimentalSuggestScoringDescription[];

extern const char kOmniboxMostVisitedTilesName[];
extern const char kOmniboxMostVisitedTilesDescription[];

extern const char kOmniboxPreserveLongerShortcutsTextName[];
extern const char kOmniboxPreserveLongerShortcutsTextDescription[];

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

extern const char kOmniboxOnFocusSuggestionsContextualWebAllowSRPName[];
extern const char kOmniboxOnFocusSuggestionsContextualWebAllowSRPDescription[];

extern const char kOmniboxOnFocusSuggestionsContextualWebName[];
extern const char kOmniboxOnFocusSuggestionsContextualWebDescription[];

extern const char kOmniboxPedalsBatch3NonEnglishName[];
extern const char kOmniboxPedalsBatch3NonEnglishDescription[];

extern const char kOmniboxShortBookmarkSuggestionsName[];
extern const char kOmniboxShortBookmarkSuggestionsDescription[];

extern const char kOmniboxSiteSearchStarterPackName[];
extern const char kOmniboxSiteSearchStarterPackDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

extern const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPName[];
extern const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription[];

extern const char kOmniboxZeroSuggestPrefetchingName[];
extern const char kOmniboxZeroSuggestPrefetchingDescription[];

extern const char kOmniboxMaxZeroSuggestMatchesName[];
extern const char kOmniboxMaxZeroSuggestMatchesDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxUpdatedConnectionSecurityIndicatorsName[];
extern const char kOmniboxUpdatedConnectionSecurityIndicatorsDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxDynamicMaxAutocompleteName[];
extern const char kOmniboxDynamicMaxAutocompleteDescription[];

extern const char kEnableSearchPrefetchName[];
extern const char kEnableSearchPrefetchDescription[];

extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

extern const char kOptimizationGuideModelDownloadingName[];
extern const char kOptimizationGuideModelDownloadingDescription[];

extern const char kOptimizationGuideModelPushNotificationName[];
extern const char kOptimizationGuideModelPushNotificationDescription[];

extern const char kOsSettingsAppNotificationsPageName[];
extern const char kOsSettingsAppNotificationsPageDescription[];

extern const char kOverviewButtonName[];
extern const char kOverviewButtonDescription[];

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
extern const char kOverrideUnsupportedPageLanguageForHrefTranslateName[];
extern const char kOverrideUnsupportedPageLanguageForHrefTranslateDescription[];
extern const char kOverrideSimilarLanguagesForHrefTranslateName[];
extern const char kOverrideSimilarLanguagesForHrefTranslateDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kPageContentAnnotationsName[];
extern const char kPageContentAnnotationsDescription[];

extern const char kPageEntitiesPageContentAnnotationsName[];
extern const char kPageEntitiesPageContentAnnotationsDescription[];

extern const char kPageInfoAboutThisSiteName[];
extern const char kPageInfoAboutThisSiteDescription[];

extern const char kPageInfoHistoryDesktopName[];
extern const char kPageInfoHistoryDesktopDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPasswordChangeInSettingsName[];
extern const char kPasswordChangeInSettingsDescription[];

extern const char kPasswordChangeName[];
extern const char kPasswordChangeDescription[];

extern const char kPasswordImportName[];
extern const char kPasswordImportDescription[];

extern const char kPasswordsAccountStorageRevisedOptInFlowName[];
extern const char kPasswordsAccountStorageRevisedOptInFlowDescription[];

extern const char kPasswordDomainCapabilitiesFetchingName[];
extern const char kPasswordDomainCapabilitiesFetchingDescription[];

extern const char kPasswordScriptsFetchingName[];
extern const char kPasswordScriptsFetchingDescription[];

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

extern const char kPermissionQuietChipName[];
extern const char kPermissionQuietChipDescription[];

extern const char kPersistentQuotaIsTemporaryQuotaName[];
extern const char kPersistentQuotaIsTemporaryQuotaDescription[];

extern const char kPersonalizationHubName[];
extern const char kPersonalizationHubDescription[];

extern const char kPlaybackSpeedButtonName[];
extern const char kPlaybackSpeedButtonDescription[];

extern const char kPointerLockOptionsName[];
extern const char kPointerLockOptionsDescription[];

extern const char kPrerender2Name[];
extern const char kPrerender2Description[];

extern const char kOmniboxTriggerForPrerender2Name[];
extern const char kOmniboxTriggerForPrerender2Description[];

extern const char kSupportSearchSuggestionForPrerender2Name[];
extern const char kSupportSearchSuggestionForPrerender2Description[];

extern const char kPrivacyAdvisorName[];
extern const char kPrivacyAdvisorDescription[];

extern const char kPrivacyGuideName[];
extern const char kPrivacyGuideDescription[];

extern const char kPrivacySandboxV3Name[];
extern const char kPrivacySandboxV3Description[];

extern const char kProminentDarkModeActiveTabTitleName[];
extern const char kProminentDarkModeActiveTabTitleDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kPwaUpdateDialogForAppIconName[];
extern const char kPwaUpdateDialogForAppIconDescription[];

extern const char kPwaUpdateDialogForAppTitleName[];
extern const char kPwaUpdateDialogForAppTitleDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kQuickDimName[];
extern const char kQuickDimDescription[];

extern const char kSettingsAppNotificationSettingsName[];
extern const char kSettingsAppNotificationSettingsDescription[];

extern const char kReadLaterFlagId[];
extern const char kReadLaterName[];
extern const char kReadLaterDescription[];

extern const char kReadLaterNewBadgePromoName[];
extern const char kReadLaterNewBadgePromoDescription[];

extern const char kRecordWebAppDebugInfoName[];
extern const char kRecordWebAppDebugInfoDescription[];

extern const char kReduceUserAgentName[];
extern const char kReduceUserAgentDescription[];

extern const char kRestrictGamepadAccessName[];
extern const char kRestrictGamepadAccessDescription[];

extern const char kMBIModeName[];
extern const char kMBIModeDescription[];

extern const char kIntensiveWakeUpThrottlingName[];
extern const char kIntensiveWakeUpThrottlingDescription[];

extern const char kSamePartyCookiesConsideredFirstPartyName[];
extern const char kSamePartyCookiesConsideredFirstPartyDescription[];

extern const char kPartitionedCookiesName[];
extern const char kPartitionedCookiesDescription[];
// TODO(crbug.com/1296161): Remove this when the CHIPS OT ends.
extern const char kPartitionedCookiesBypassOriginTrialName[];
extern const char kPartitionedCookiesBypassOriginTrialDescription[];

extern const char kScrollableTabStripFlagId[];
extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kScrollableTabStripButtonsName[];
extern const char kScrollableTabStripButtonsDescription[];

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

extern const char kShoppingListName[];
extern const char kShoppingListDescription[];

extern const char kSidePanelFlagId[];
extern const char kSidePanelName[];
extern const char kSidePanelDescription[];

extern const char kSidePanelDragAndDropFlagId[];
extern const char kSidePanelDragAndDropName[];
extern const char kSidePanelDragAndDropDescription[];

extern const char kSharedClipboardUIName[];
extern const char kSharedClipboardUIDescription[];

extern const char kSharingDesktopScreenshotsName[];
extern const char kSharingDesktopScreenshotsDescription[];

extern const char kSharingDesktopScreenshotsEditName[];
extern const char kSharingDesktopScreenshotsEditDescription[];

extern const char kSharingPreferVapidName[];
extern const char kSharingPreferVapidDescription[];

extern const char kSharingSendViaSyncName[];
extern const char kSharingSendViaSyncDescription[];

extern const char kShelfDragToPinName[];
extern const char kShelfDragToPinDescription[];

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

extern const char kSplitCacheByNetworkIsolationKeyName[];
extern const char kSplitCacheByNetworkIsolationKeyDescription[];

extern const char kStoragePressureEventName[];
extern const char kStoragePressureEventDescription[];

extern const char kStoreHoursAndroidName[];
extern const char kStoreHoursAndroidDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kStylusBatteryStatusName[];
extern const char kStylusBatteryStatusDescription[];

extern const char kSubframeShutdownDelayName[];
extern const char kSubframeShutdownDelayDescription[];

extern const char kSuppressToolbarCapturesName[];
extern const char kSuppressToolbarCapturesDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncTrustedVaultPassphrasePromoName[];
extern const char kSyncTrustedVaultPassphrasePromoDescription[];

extern const char kSyncTrustedVaultPassphraseRecoveryName[];
extern const char kSyncTrustedVaultPassphraseRecoveryDescription[];

extern const char kSystemProxyForSystemServicesName[];
extern const char kSystemProxyForSystemServicesDescription[];

extern const char kTabEngagementReportingName[];
extern const char kTabEngagementReportingDescription[];

extern const char kTabGridLayoutAndroidName[];
extern const char kTabGridLayoutAndroidDescription[];

extern const char kCommerceDeveloperName[];
extern const char kCommerceDeveloperDescription[];

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

extern const char kAppsShortcutDefaultOffName[];
extern const char kAppsShortcutDefaultOffDescription[];

extern const char kTabGroupsAutoCreateName[];
extern const char kTabGroupsAutoCreateDescription[];

extern const char kTabGroupsNewBadgePromoName[];
extern const char kTabGroupsNewBadgePromoDescription[];

extern const char kTabGroupsSaveName[];
extern const char kTabGroupsSaveDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabOutlinesInLowContrastThemesName[];
extern const char kTabOutlinesInLowContrastThemesDescription[];

extern const char kTabSearchFuzzySearchName[];
extern const char kTabSearchFuzzySearchDescription[];

extern const char kTabStripImprovementsAndroidName[];
extern const char kTabStripImprovementsAndroidDescription[];

extern const char kTailoredSecurityIntegrationName[];
extern const char kTailoredSecurityIntegrationDescription[];

extern const char kTFLiteLanguageDetectionName[];
extern const char kTFLiteLanguageDetectionDescription[];

extern const char kTintCompositedContentName[];
extern const char kTintCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kThrottleForegroundTimersName[];
extern const char kThrottleForegroundTimersDescription[];

extern const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesName[];
extern const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesDescription[];

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

extern const char kTrustTokensName[];
extern const char kTrustTokensDescription[];

extern const char kTurnOffStreamingMediaCachingOnBatteryName[];
extern const char kTurnOffStreamingMediaCachingOnBatteryDescription[];

extern const char kTurnOffStreamingMediaCachingAlwaysName[];
extern const char kTurnOffStreamingMediaCachingAlwaysDescription[];

extern const char kUnifiedSidePanelFlagId[];
extern const char kUnifiedSidePanelName[];
extern const char kUnifiedSidePanelDescription[];

extern const char kUnifiedPasswordManagerAndroidName[];
extern const char kUnifiedPasswordManagerAndroidDescription[];

extern const char kUnifiedPasswordManagerDesktopName[];
extern const char kUnifiedPasswordManagerDesktopDescription[];

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

extern const char kVerticalSnapName[];
extern const char kVerticalSnapDescription[];

extern const char kGlobalVaapiLockName[];
extern const char kGlobalVaapiLockDescription[];

extern const char kVp9kSVCHWDecodingName[];
extern const char kVp9kSVCHWDecodingDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWallpaperFullScreenPreviewName[];
extern const char kWallpaperFullScreenPreviewDescription[];

extern const char kWallpaperGooglePhotosIntegrationName[];
extern const char kWallpaperGooglePhotosIntegrationDescription[];

extern const char kWallpaperPerDeskName[];
extern const char kWallpaperPerDeskDescription[];

extern const char kWebBluetoothName[];
extern const char kWebBluetoothDescription[];

extern const char kWebBluetoothNewPermissionsBackendName[];
extern const char kWebBluetoothNewPermissionsBackendDescription[];

extern const char kWebBundlesName[];
extern const char kWebBundlesDescription[];

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

extern const char kWebPaymentsExperimentalFeaturesName[];
extern const char kWebPaymentsExperimentalFeaturesDescription[];

extern const char kPaymentRequestBasicCardName[];
extern const char kPaymentRequestBasicCardDescription[];

extern const char kAppStoreBillingDebugName[];
extern const char kAppStoreBillingDebugDescription[];

extern const char kWebrtcCaptureMultiChannelApmName[];
extern const char kWebrtcCaptureMultiChannelApmDescription[];

extern const char kWebrtcHideLocalIpsWithMdnsName[];
extern const char kWebrtcHideLocalIpsWithMdnsDecription[];

extern const char kWebrtcHybridAgcName[];
extern const char kWebrtcHybridAgcDescription[];

extern const char kWebrtcAnalogAgcClippingControlName[];
extern const char kWebrtcAnalogAgcClippingControlDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcUseMinMaxVEADimensionsName[];
extern const char kWebrtcUseMinMaxVEADimensionsDescription[];

extern const char kWebUsbDeviceDetectionName[];
extern const char kWebUsbDeviceDetectionDescription[];

extern const char kWebXrForceRuntimeName[];
extern const char kWebXrForceRuntimeDescription[];

extern const char kWebXrRuntimeChoiceNone[];
extern const char kWebXrRuntimeChoiceOpenXR[];

extern const char kWebXrIncubationsName[];
extern const char kWebXrIncubationsDescription[];

extern const char kWindowsFollowCursorName[];
extern const char kWindowsFollowCursorDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

extern const char kSharedHighlightingV2Name[];
extern const char kSharedHighlightingV2Description[];

extern const char kSharedHighlightingAmpName[];
extern const char kSharedHighlightingAmpDescription[];

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

extern const char kExtensionWorkflowJustificationName[];
extern const char kExtensionWorkflowJustificationDescription[];

extern const char kForceMajorVersionInMinorPositionInUserAgentName[];
extern const char kForceMajorVersionInMinorPositionInUserAgentDescription[];

extern const char kDurableClientHintsCacheName[];
extern const char kDurableClientHintsCacheDescription[];

extern const char kReduceUserAgentMinorVersionName[];
extern const char kReduceUserAgentMinorVersionDescription[];

extern const char kWebSQLAccessName[];
extern const char kWebSQLAccessDescription[];

// Android --------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

extern const char kAccessibilityPageZoomName[];
extern const char kAccessibilityPageZoomDescription[];

extern const char kActivateMetricsReportingEnabledPolicyAndroidName[];
extern const char kActivateMetricsReportingEnabledPolicyAndroidDescription[];

extern const char kAddToHomescreenIPHName[];
extern const char kAddToHomescreenIPHDescription[];

extern const char kAImageReaderName[];
extern const char kAImageReaderDescription[];

extern const char kAndroidDetailedLanguageSettingsName[];
extern const char kAndroidDetailedLanguageSettingsDescription[];

extern const char kAndroidForceAppLanguagePromptName[];
extern const char kAndroidForceAppLanguagePromptDescription[];

extern const char kAndroidLayoutChangeTabReparentingName[];
extern const char kAndroidLayoutChangeTabReparentingDescription[];

extern const char kAndroidSurfaceControlName[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAssistantIntentPageUrlName[];
extern const char kAssistantIntentPageUrlDescription[];

extern const char kAssistantIntentTranslateInfoName[];
extern const char kAssistantIntentTranslateInfoDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

extern const char kAppMenuMobileSiteOptionName[];
extern const char kAppMenuMobileSiteOptionDescription[];

extern const char kBookmarkBottomSheetName[];
extern const char kBookmarkBottomSheetDescription[];

extern const char kCCTIncognitoName[];
extern const char kCCTIncognitoDescription[];

extern const char kCCTIncognitoAvailableToThirdPartyName[];
extern const char kCCTIncognitoAvailableToThirdPartyDescription[];

extern const char kCCTResizable90MaximumHeightName[];
extern const char kCCTResizable90MaximumHeightDescription[];
extern const char kCCTResizableAllowResizeByUserGestureName[];
extern const char kCCTResizableAllowResizeByUserGestureDescription[];
extern const char kCCTResizableForFirstPartiesName[];
extern const char kCCTResizableForFirstPartiesDescription[];
extern const char kCCTResizableForThirdPartiesName[];
extern const char kCCTResizableForThirdPartiesDescription[];

extern const char kChimeAlwaysShowNotificationDescription[];
extern const char kChimeAlwaysShowNotificationName[];

extern const char kChimeAndroidSdkDescription[];
extern const char kChimeAndroidSdkName[];

extern const char kContinuousSearchName[];
extern const char kContinuousSearchDescription[];

extern const char kChromeShareLongScreenshotName[];
extern const char kChromeShareLongScreenshotDescription[];

extern const char kChromeSharingHubLaunchAdjacentName[];
extern const char kChromeSharingHubLaunchAdjacentDescription[];

extern const char kCloseTabSuggestionsName[];
extern const char kCloseTabSuggestionsDescription[];

extern const char kCriticalPersistedTabDataName[];
extern const char kCriticalPersistedTabDataDescription[];

extern const char kContextMenuPopupStyleName[];
extern const char kContextMenuPopupStyleDescription[];

extern const char kContextualSearchDebugName[];
extern const char kContextualSearchDebugDescription[];

extern const char kContextualSearchDelayedIntelligenceName[];
extern const char kContextualSearchDelayedIntelligenceDescription[];

extern const char kContextualSearchForceCaptionName[];
extern const char kContextualSearchForceCaptionDescription[];

extern const char kContextualSearchLongpressResolveName[];
extern const char kContextualSearchLongpressResolveDescription[];

extern const char kContextualSearchMlTapSuppressionName[];
extern const char kContextualSearchMlTapSuppressionDescription[];

extern const char KContextualSearchNewSettingsName[];
extern const char KContextualSearchNewSettingsDescription[];

extern const char kContextualSearchRankerQueryName[];
extern const char kContextualSearchRankerQueryDescription[];

extern const char kContextualSearchThinWebViewImplementationName[];
extern const char kContextualSearchThinWebViewImplementationDescription[];

extern const char kContextualSearchTranslationsName[];
extern const char kContextualSearchTranslationsDescription[];

extern const char kContextualTriggersSelectionHandlesName[];
extern const char kContextualTriggersSelectionHandlesDescription[];

extern const char kContextualTriggersSelectionMenuName[];
extern const char kContextualTriggersSelectionMenuDescription[];

extern const char kContextualTriggersSelectionSizeName[];
extern const char kContextualTriggersSelectionSizeDescription[];

extern const char kCpuAffinityRestrictToLittleCoresName[];
extern const char kCpuAffinityRestrictToLittleCoresDescription[];

extern const char kDynamicColorAndroidName[];
extern const char kDynamicColorAndroidDescription[];

extern const char kDynamicColorButtonsAndroidName[];
extern const char kDynamicColorButtonsAndroidDescription[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAutofillRefreshStyleName[];
extern const char kEnableAutofillRefreshStyleDescription[];

extern const char kChromeManagementPageAndroidName[];
extern const char kChromeManagementPageAndroidDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnableDangerousDownloadDialogName[];
extern const char kEnableDangerousDownloadDialogDescription[];

extern const char kEnableDuplicateDownloadDialogName[];
extern const char kEnableDuplicateDownloadDialogDescription[];

extern const char kEnableMixedContentDownloadDialogName[];
extern const char kEnableMixedContentDownloadDialogDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kFeatureNotificationGuideName[];
extern const char kFeatureNotificationGuideDescription[];

extern const char kFeatureNotificationGuideSkipCheckForLowEngagedUsersName[];
extern const char
    kFeatureNotificationGuideSkipCheckForLowEngagedUsersDescription[];

extern const char kFeedBackToTopName[];
extern const char kFeedBackToTopDescription[];

extern const char kFeedInteractiveRefreshName[];
extern const char kFeedInteractiveRefreshDescription[];

extern const char kFeedLoadingPlaceholderName[];
extern const char kFeedLoadingPlaceholderDescription[];

extern const char kFeedStampName[];
extern const char kFeedStampDescription[];

extern const char kGridTabSwitcherForTabletsName[];
extern const char kGridTabSwitcherForTabletsDescription[];

extern const char kHomepagePromoCardName[];
extern const char kHomepagePromoCardDescription[];

extern const char kInstanceSwitcherName[];
extern const char kInstanceSwitcherDescription[];

extern const char kInstantStartName[];
extern const char kInstantStartDescription[];

extern const char kIntentBlockExternalFormRedirectsNoGestureName[];
extern const char kIntentBlockExternalFormRedirectsNoGestureDescription[];

extern const char kInterestFeedV2Name[];
extern const char kInterestFeedV2Description[];

extern const char kInterestFeedV2HeartsName[];
extern const char kInterestFeedV2HeartsDescription[];

extern const char kInterestFeedV2AutoplayName[];
extern const char kInterestFeedV2AutoplayDescription[];

extern const char kInterestFeedV1ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV1ClickAndViewActionsConditionalUploadDescription[];

extern const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[];

extern const char kLightweightReactionsAndroidName[];
extern const char kLightweightReactionsAndroidDescription[];

extern const char kMessagesForAndroidAdsBlockedName[];
extern const char kMessagesForAndroidAdsBlockedDescription[];

extern const char kMessagesForAndroidChromeSurveyName[];
extern const char kMessagesForAndroidChromeSurveyDescription[];

extern const char kMessagesForAndroidInfrastructureName[];
extern const char kMessagesForAndroidInfrastructureDescription[];

extern const char kMessagesForAndroidInstantAppsName[];
extern const char kMessagesForAndroidInstantAppsDescription[];

extern const char kMessagesForAndroidNearOomReductionName[];
extern const char kMessagesForAndroidNearOomReductionDescription[];

extern const char kMessagesForAndroidNotificationBlockedName[];
extern const char kMessagesForAndroidNotificationBlockedDescription[];

extern const char kMessagesForAndroidPasswordsName[];
extern const char kMessagesForAndroidPasswordsDescription[];

extern const char kMessagesForAndroidPermissionUpdateName[];
extern const char kMessagesForAndroidPermissionUpdateDescription[];

extern const char kMessagesForAndroidPopupBlockedName[];
extern const char kMessagesForAndroidPopupBlockedDescription[];

extern const char kMessagesForAndroidPWAInstallName[];
extern const char kMessagesForAndroidPWAInstallDescription[];

extern const char kMessagesForAndroidReaderModeName[];
extern const char kMessagesForAndroidReaderModeDescription[];

extern const char kMessagesForAndroidSafetyTipName[];
extern const char kMessagesForAndroidSafetyTipDescription[];

extern const char kMessagesForAndroidSaveCardName[];
extern const char kMessagesForAndroidSaveCardDescription[];

extern const char kMessagesForAndroidSyncErrorName[];
extern const char kMessagesForAndroidSyncErrorDescription[];

extern const char kMessagesForAndroidUpdatePasswordName[];
extern const char kMessagesForAndroidUpdatePasswordDescription[];

extern const char kNewWindowAppMenuName[];
extern const char kNewWindowAppMenuDescription[];

extern const char kOfflineIndicatorV2Name[];
extern const char kOfflineIndicatorV2Description[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kPageInfoDiscoverabilityTimeoutsName[];
extern const char kPageInfoDiscoverabilityTimeoutsDescription[];

extern const char kPageInfoHistoryName[];
extern const char kPageInfoHistoryDescription[];

extern const char kPageInfoStoreInfoName[];
extern const char kPageInfoStoreInfoDescription[];

extern const char kPasswordProtectionForSignedInUsersName[];
extern const char kPasswordProtectionForSignedInUsersDescription[];

extern const char kPersistShareHubOnAppSwitchName[];
extern const char kPersistShareHubOnAppSwitchDescription[];

extern const char kPhotoPickerVideoSupportName[];
extern const char kPhotoPickerVideoSupportDescription[];

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

extern const char kImproveReaderModePromptName[];
extern const char kImproveReaderModePromptDescription[];

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

extern const char kRelatedSearchesAlternateUxName[];
extern const char kRelatedSearchesAlternateUxDescription[];

extern const char kRelatedSearchesInBarName[];
extern const char kRelatedSearchesInBarDescription[];

extern const char kRelatedSearchesSimplifiedUxName[];
extern const char kRelatedSearchesSimplifiedUxDescription[];

extern const char kRelatedSearchesUiName[];
extern const char kRelatedSearchesUiDescription[];

extern const char kRequestDesktopSiteExceptionsName[];
extern const char kRequestDesktopSiteExceptionsDescription[];

extern const char kRequestDesktopSiteGlobalName[];
extern const char kRequestDesktopSiteGlobalDescription[];

extern const char kRequestDesktopSiteForTabletsName[];
extern const char kRequestDesktopSiteForTabletsDescription[];

extern const char kSecurePaymentConfirmationAndroidName[];
extern const char kSecurePaymentConfirmationAndroidDescription[];

extern const char kShowScrollableMVTOnNTPAndroidName[];
extern const char kShowScrollableMVTOnNTPAndroidDescription[];

extern const char kSendTabToSelfV2Name[];
extern const char kSendTabToSelfV2Description[];

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

extern const char kSyncAndroidPromosWithSingleButtonName[];
extern const char kSyncAndroidPromosWithSingleButtonDescription[];

extern const char kTabGroupsForTabletsName[];
extern const char kTabGroupsForTabletsDescription[];

extern const char kThemeRefactorAndroidName[];
extern const char kThemeRefactorAndroidDescription[];

extern const char kToolbarIphAndroidName[];
extern const char kToolbarIphAndroidDescription[];

extern const char kTouchDragAndContextMenuName[];
extern const char kTouchDragAndContextMenuDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuTypeName[];
extern const char kUpdateMenuTypeDescription[];
extern const char kUpdateMenuTypeNone[];
extern const char kUpdateMenuTypeUpdateAvailable[];
extern const char kUpdateMenuTypeUnsupportedOSVersion[];

extern const char kUseRealColorSpaceForAndroidVideoName[];
extern const char kUseRealColorSpaceForAndroidVideoDescription[];

extern const char kUserMediaScreenCapturingName[];
extern const char kUserMediaScreenCapturingDescription[];

extern const char kVideoTutorialsName[];
extern const char kVideoTutorialsDescription[];
extern const char kVideoTutorialsInstantFetchName[];
extern const char kVideoTutorialsInstantFetchDescription[];

extern const char kAdaptiveButtonInTopToolbarName[];
extern const char kAdaptiveButtonInTopToolbarDescription[];
extern const char kAdaptiveButtonInTopToolbarCustomizationName[];
extern const char kAdaptiveButtonInTopToolbarCustomizationDescription[];
extern const char kShareButtonInTopToolbarName[];
extern const char kShareButtonInTopToolbarDescription[];
extern const char kVoiceButtonInTopToolbarName[];
extern const char kVoiceButtonInTopToolbarDescription[];

extern const char kWebFeedName[];
extern const char kWebFeedDescription[];

extern const char kWebFeedOnboardingName[];
extern const char kWebFeedOnboardingDescription[];

extern const char kWebFeedSortName[];
extern const char kWebFeedSortDescription[];

extern const char kXsurfaceMetricsReportingName[];
extern const char kXsurfaceMetricsReportingDescription[];

extern const char kWebNotesPublishName[];
extern const char kWebNotesPublishDescription[];

extern const char kWebNotesDynamicTemplatesName[];
extern const char kWebNotesDynamicTemplatesDescription[];

extern const char kOmniboxPedalsAndroidBatch1Name[];
extern const char kOmniboxPedalsAndroidBatch1Description[];

// Non-Android ----------------------------------------------------------------

#else  // !BUILDFLAG(IS_ANDROID)

extern const char kAppManagementAppDetailsName[];
extern const char kAppManagementAppDetailsDescription[];

extern const char kAllowAllSitesToInitiateMirroringName[];
extern const char kAllowAllSitesToInitiateMirroringDescription[];

extern const char kBlockMigratedDefaultChromeAppSyncName[];
extern const char kBlockMigratedDefaultChromeAppSyncDescription[];

extern const char kEnableAccessibilityLiveCaptionName[];
extern const char kEnableAccessibilityLiveCaptionDescription[];

extern const char kEnableUserCloudSigninRestrictionPolicyName[];
extern const char kEnableUserCloudSigninRestrictionPolicyDescription[];

extern const char kCopyLinkToTextName[];
extern const char kCopyLinkToTextDescription[];

extern const char kGlobalMediaControlsCastStartStopName[];
extern const char kGlobalMediaControlsCastStartStopDescription[];

extern const char kMuteNotificationSnoozeActionName[];
extern const char kMuteNotificationSnoozeActionDescription[];

extern const char kNtpCacheOneGoogleBarName[];
extern const char kNtpCacheOneGoogleBarDescription[];

extern const char kNtpModulesName[];
extern const char kNtpModulesDescription[];

extern const char kNtpDriveModuleName[];
extern const char kNtpDriveModuleDescription[];

#if !defined(OFFICIAL_BUILD)
extern const char kNtpDummyModulesName[];
extern const char kNtpDummyModulesDescription[];
#endif

extern const char kNtpPhotosModuleName[];
extern const char kNtpPhotosModuleDescription[];

extern const char kNtpPhotosModuleOptInTitleName[];
extern const char kNtpPhotosModuleOptInTitleDescription[];

extern const char kNtpPhotosModuleOptInArtWorkName[];
extern const char kNtpPhotosModuleOptInArtWorkDescription[];

extern const char kNtpPhotosModuleSoftOptOutName[];
extern const char kNtpPhotosModuleSoftOptOutDescription[];

extern const char kNtpRecipeTasksModuleName[];
extern const char kNtpRecipeTasksModuleDescription[];

extern const char kNtpShoppingTasksModuleName[];
extern const char kNtpShoppingTasksModuleDescription[];

extern const char kNtpChromeCartModuleName[];
extern const char kNtpChromeCartModuleDescription[];

extern const char kNtpSafeBrowsingModuleName[];
extern const char kNtpSafeBrowsingModuleDescription[];

extern const char kNtpModulesDragAndDropName[];
extern const char kNtpModulesDragAndDropDescription[];

extern const char kNtpModulesFirstRunExperienceName[];
extern const char kNtpModulesFirstRunExperienceDescription[];

extern const char kNtpModulesRedesignedName[];
extern const char kNtpModulesRedesignedDescription[];

extern const char kNtpModulesRedesignedLayoutName[];
extern const char kNtpModulesRedesignedLayoutDescription[];

extern const char kNtpRealboxMatchOmniboxThemeName[];
extern const char kNtpRealboxMatchOmniboxThemeDescription[];

extern const char kNtpRealboxMatchSearchboxThemeName[];
extern const char kNtpRealboxMatchSearchboxThemeDescription[];

extern const char kNtpRealboxPedalsName[];
extern const char kNtpRealboxPedalsDescription[];

extern const char kNtpRealboxSuggestionAnswersName[];
extern const char kNtpRealboxSuggestionAnswersDescription[];

extern const char kNtpRealboxTailSuggestName[];
extern const char kNtpRealboxTailSuggestDescription[];

extern const char kNtpRealboxUseGoogleGIconName[];
extern const char kNtpRealboxUseGoogleGIconDescription[];

extern const char kEnableReaderModeName[];
extern const char kEnableReaderModeDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kKernelnextVMsName[];
extern const char kKernelnextVMsDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescriptions[];

extern const char kOmniboxExperimentalKeywordModeName[];
extern const char kOmniboxExperimentalKeywordModeDescription[];

extern const char kOmniboxPedalsBatch2NonEnglishName[];
extern const char kOmniboxPedalsBatch2NonEnglishDescription[];

extern const char kOmniboxPedalsTranslationConsoleName[];
extern const char kOmniboxPedalsTranslationConsoleDescription[];

extern const char kScreenAIName[];
extern const char kScreenAIDescription[];

extern const char kSCTAuditingName[];
extern const char kSCTAuditingDescription[];

#endif  // BUILDFLAG(IS_ANDROID)

// Windows --------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kEnableIncognitoShortcutOnDesktopName[];
extern const char kEnableIncognitoShortcutOnDesktopDescription[];

extern const char kEnableMediaFoundationVideoCaptureName[];
extern const char kEnableMediaFoundationVideoCaptureDescription[];

extern const char kHardwareSecureDecryptionName[];
extern const char kHardwareSecureDecryptionDescription[];

extern const char kHardwareSecureDecryptionExperimentName[];
extern const char kHardwareSecureDecryptionExperimentDescription[];

extern const char kMediaFoundationClearName[];
extern const char kMediaFoundationClearDescription[];

extern const char kPervasiveSystemAccentColorName[];
extern const char kPervasiveSystemAccentColorDescription[];

extern const char kPwaUninstallInWindowsOsName[];
extern const char kPwaUninstallInWindowsOsDescription[];

extern const char kRawAudioCaptureName[];
extern const char kRawAudioCaptureDescription[];

extern const char kRunVideoCaptureServiceInBrowserProcessName[];
extern const char kRunVideoCaptureServiceInBrowserProcessDescription[];

extern const char kUseAngleDescriptionWindows[];

extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];
extern const char kUseAngleD3D11on12[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

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

extern const char kWin10TabSearchCaptionButtonName[];
extern const char kWin10TabSearchCaptionButtonDescription[];
#endif  // BUILDFLAG(IS_WIN)

// Mac ------------------------------------------------------------------------

#if BUILDFLAG(IS_MAC)

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

extern const char kMetalName[];
extern const char kMetalDescription[];

extern const char kScreenTimeName[];
extern const char kScreenTimeDescription[];

extern const char kUseAngleDescriptionMac[];
extern const char kUseAngleMetal[];

#endif  // BUILDFLAG(IS_MAC)

// Windows and Mac ------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

extern const char kUseAngleName[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Chrome OS ------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAllowDisableMouseAccelerationName[];
extern const char kAllowDisableMouseAccelerationDescription[];

extern const char kAllowDisableTouchpadHapticFeedbackName[];
extern const char kAllowDisableTouchpadHapticFeedbackDescription[];

extern const char kAllowRepeatedUpdatesName[];
extern const char kAllowRepeatedUpdatesDescription[];

extern const char kAllowScrollSettingsName[];
extern const char kAllowScrollSettingsDescription[];

extern const char kAllowTouchpadHapticClickSettingsName[];
extern const char kAllowTouchpadHapticClickSettingsDescription[];

extern const char kAmbientModeAnimationName[];
extern const char kAmbientModeAnimationDescription[];

extern const char kAmbientModeNewUrlName[];
extern const char kAmbientModeNewUrlDescription[];

extern const char kAppDiscoveryForOobeName[];
extern const char kAppDiscoveryForOobeDescription[];

extern const char kAppDiscoveryRemoteUrlSearchName[];
extern const char kAppDiscoveryRemoteUrlSearchDescription[];

extern const char kAppProvisioningStaticName[];
extern const char kAppProvisioningStaticDescription[];

extern const char kArcAccountRestrictionsName[];
extern const char kArcAccountRestrictionsDescription[];

extern const char kArcCompatSnapName[];
extern const char kArcCompatSnapDesc[];

extern const char kArcCustomTabsExperimentName[];
extern const char kArcCustomTabsExperimentDescription[];

extern const char kArcDocumentsProviderUnknownSizeName[];
extern const char kArcDocumentsProviderUnknownSizeDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcKeyboardShortcutHelperIntegrationName[];
extern const char kArcKeyboardShortcutHelperIntegrationDescription[];

extern const char kArcMouseWheelSmoothScrollName[];
extern const char kArcMouseWheelSmoothScrollDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcNativeBridge64BitSupportExperimentName[];
extern const char kArcNativeBridge64BitSupportExperimentDescription[];

extern const char kArcRightClickLongPressName[];
extern const char kArcRightClickLongPressDescription[];

extern const char kArcRtVcpuDualCoreName[];
extern const char kArcRtVcpuDualCoreDesc[];

extern const char kArcRtVcpuQuadCoreName[];
extern const char kArcRtVcpuQuadCoreDesc[];

extern const char kArcUsbDeviceDefaultAttachToVmName[];
extern const char kArcUsbDeviceDefaultAttachToVmDescription[];

extern const char kArcVmBalloonPolicyName[];
extern const char kArcVmBalloonPolicyDesc[];

extern const char kArcEnableUsapName[];
extern const char kArcEnableUsapDesc[];

extern const char kArcEnableVirtioBlkForDataName[];
extern const char kArcEnableVirtioBlkForDataDesc[];

extern const char kAshEnablePipRoundedCornersName[];
extern const char kAshEnablePipRoundedCornersDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAudioUrlName[];
extern const char kAudioUrlDescription[];

extern const char kAutoFramingOverrideName[];
extern const char kAutoFramingOverrideDescription[];

extern const char kAutocorrectParamsTuningName[];
extern const char kAutocorrectParamsTuningDescription[];

extern const char kBluetoothFixA2dpPacketSizeName[];
extern const char kBluetoothFixA2dpPacketSizeDescription[];

extern const char kBluetoothRevampName[];
extern const char kBluetoothRevampDescription[];

extern const char kBluetoothWbsDogfoodName[];
extern const char kBluetoothWbsDogfoodDescription[];

extern const char kBluetoothUseFlossName[];
extern const char kBluetoothUseFlossDescription[];

extern const char kCalendarViewName[];
extern const char kCalendarViewDescription[];

extern const char kDefaultLinkCapturingInBrowserName[];
extern const char kDefaultLinkCapturingInBrowserDescription[];

extern const char kDesksTemplatesName[];
extern const char kDesksTemplatesDescription[];

extern const char kDesksTrackpadSwipeImprovementsName[];
extern const char kDesksTrackpadSwipeImprovementsDescription[];

extern const char kPreferConstantFrameRateName[];
extern const char kPreferConstantFrameRateDescription[];

extern const char kForceControlFaceAeName[];
extern const char kForceControlFaceAeDescription[];

extern const char kHdrNetOverrideName[];
extern const char kHdrNetOverrideDescription[];

extern const char kCameraAppDocumentManualCropName[];
extern const char kCameraAppDocumentManualCropDescription[];

extern const char kCategoricalSearchName[];
extern const char kCategoricalSearchDescription[];

extern const char kCellularBypassESimInstallationConnectivityCheckName[];
extern const char kCellularBypassESimInstallationConnectivityCheckDescription[];

extern const char kCellularForbidAttachApnName[];
extern const char kCellularForbidAttachApnDescription[];

extern const char kCellularUseAttachApnName[];
extern const char kCellularUseAttachApnDescription[];

extern const char kCellularUseExternalEuiccName[];
extern const char kCellularUseExternalEuiccDescription[];

extern const char kComponentUpdaterTestRequestName[];
extern const char kComponentUpdaterTestRequestDescription[];

extern const char kContextualNudgesName[];
extern const char kContextualNudgesDescription[];

extern const char kCroshSWAName[];
extern const char kCroshSWADescription[];

extern const char kCrosLanguageSettingsUpdate2Name[];
extern const char kCrosLanguageSettingsUpdate2Description[];

extern const char kCrosOnDeviceGrammarCheckName[];
extern const char kCrosOnDeviceGrammarCheckDescription[];

extern const char kSystemExtensionsName[];
extern const char kSystemExtensionsDescription[];

extern const char kCrostiniBullseyeUpgradeName[];
extern const char kCrostiniBullseyeUpgradeDescription[];

extern const char kCrostiniDiskResizingName[];
extern const char kCrostiniDiskResizingDescription[];

extern const char kCrostiniContainerInstallName[];
extern const char kCrostiniContainerInstallDescription[];

extern const char kCrostiniGpuSupportName[];
extern const char kCrostiniGpuSupportDescription[];

extern const char kCrostiniUseDlcName[];
extern const char kCrostiniUseDlcDescription[];

extern const char kCrostiniResetLxdDbName[];
extern const char kCrostiniResetLxdDbDescription[];

extern const char kCrostiniUseLxd4Name[];
extern const char kCrostiniUseLxd4Description[];

extern const char kCrostiniMultiContainerName[];
extern const char kCrostiniMultiContainerDescription[];

extern const char kCrostiniImeSupportName[];
extern const char kCrostiniImeSupportDescription[];

extern const char kCrostiniVirtualKeyboardSupportName[];
extern const char kCrostiniVirtualKeyboardSupportDescription[];

extern const char kBruschettaName[];
extern const char kBruschettaDescription[];

extern const char kCryptAuthV2DedupDeviceLastActivityTimeName[];
extern const char kCryptAuthV2DedupDeviceLastActivityTimeDescription[];

extern const char kDisableBufferBWCompressionName[];
extern const char kDisableBufferBWCompressionDescription[];

extern const char kDisableCameraFrameRotationAtSourceName[];
extern const char kDisableCameraFrameRotationAtSourceDescription[];

extern const char kForceSpectreVariant2MitigationName[];
extern const char kForceSpectreVariant2MitigationDescription[];

extern const char kDisableCancelAllTouchesName[];
extern const char kDisableCancelAllTouchesDescription[];

extern const char kDisableIdleSocketsCloseOnMemoryPressureName[];
extern const char kDisableIdleSocketsCloseOnMemoryPressureDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisplayAlignmentAssistanceName[];
extern const char kDisplayAlignmentAssistanceDescription[];

extern const char kFastPairName[];
extern const char kFastPairDescription[];

extern const char kFastPairLowPowerName[];
extern const char kFastPairLowPowerDescription[];

extern const char kFastPairSoftwareScanningName[];
extern const char kFastPairSoftwareScanningDescription[];

extern const char kUseHDRTransferFunctionName[];
extern const char kUseHDRTransferFunctionDescription[];

extern const char kDisableOfficeEditingComponentAppName[];
extern const char kDisableOfficeEditingComponentAppDescription[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kQuickSettingsPWANotificationsName[];
extern const char kQuickSettingsPWANotificationsDescription[];

extern const char kDriveFsBidirectionalNativeMessagingName[];
extern const char kDriveFsBidirectionalNativeMessagingDescription[];

extern const char kEnableAppReinstallZeroStateName[];
extern const char kEnableAppReinstallZeroStateDescription[];

extern const char kEnableAssistantRoutinesName[];
extern const char kEnableAssistantRoutinesDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnhancedClipboardName[];
extern const char kEnhancedClipboardDescription[];

extern const char kEnhancedClipboardNudgeSessionResetName[];
extern const char kEnhancedClipboardNudgeSessionResetDescription[];

extern const char kEnhancedClipboardScreenshotNudgeName[];
extern const char kEnhancedClipboardScreenshotNudgeDescription[];

extern const char kEnableCrOSActionRecorderName[];
extern const char kEnableCrOSActionRecorderDescription[];

extern const char kEnableDnsProxyName[];
extern const char kEnableDnsProxyDescription[];

extern const char kDnsProxyEnableDOHName[];
extern const char kDnsProxyEnableDOHDescription[];

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

extern const char kEnableInputEventLoggingName[];
extern const char kEnableInputEventLoggingDescription[];

extern const char kEnableInputInDiagnosticsAppName[];
extern const char kEnableInputInDiagnosticsAppDescription[];

extern const char kEnableKeyboardBacklightToggleName[];
extern const char kEnableKeyboardBacklightToggleDescription[];

extern const char kEnableLauncherSearchNormalizationName[];
extern const char kEnableLauncherSearchNormalizationDescription[];

extern const char kEnableLibinputToHandleTouchpadName[];
extern const char kEnableLibinputToHandleTouchpadDescription[];

extern const char kEnableNeuralPalmAdaptiveHoldName[];
extern const char kEnableNeuralPalmAdaptiveHoldDescription[];

extern const char kEnableNeuralPalmRejectionModelV2Name[];
extern const char kEnableNeuralPalmRejectionModelV2Description[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableOsFeedbackName[];
extern const char kEnableOsFeedbackDescription[];

extern const char kEnableNewShortcutMappingName[];
extern const char kEnableNewShortcutMappingDescription[];

extern const char kEnablePalmOnMaxTouchMajorName[];
extern const char kEnablePalmOnMaxTouchMajorDescription[];

extern const char kEnablePalmOnToolTypePalmName[];
extern const char kEnablePalmOnToolTypePalmDescription[];

extern const char kEnablePalmSuppressionName[];
extern const char kEnablePalmSuppressionDescription[];

extern const char kDisableQuickAnswersV2TranslationName[];
extern const char kDisableQuickAnswersV2TranslationDescription[];

extern const char kQuickAnswersAlwaysTriggerForSingleWordName[];
extern const char kQuickAnswersAlwaysTriggerForSingleWordDescription[];

extern const char kESimPolicyName[];
extern const char kESimPolicyDescription[];

extern const char kTrimOnMemoryPressureName[];
extern const char kTrimOnMemoryPressureDescription[];

extern const char kEchePhoneHubPermissionsOnboardingName[];
extern const char kEchePhoneHubPermissionsOnboardingDescription[];

extern const char kEcheSWAName[];
extern const char kEcheSWADescription[];

extern const char kEcheCustomWidgetName[];
extern const char kEcheCustomWidgetDescription[];

extern const char kEcheSWADebugModeName[];
extern const char kEcheSWADebugModeDescription[];

extern const char kEcheSWAInBackgroundName[];
extern const char kEcheSWAInBackgroundDescription[];

extern const char kEnableIdleInhibitName[];
extern const char kEnableIdleInhibitDescription[];

extern const char kEnableIkev2VpnName[];
extern const char kEnableIkev2VpnDescription[];

extern const char kEnableNetworkingInDiagnosticsAppName[];
extern const char kEnableNetworkingInDiagnosticsAppDescription[];

extern const char kEnableOAuthIppName[];
extern const char kEnableOAuthIppDescription[];

extern const char kEnableRevenLogSourceName[];
extern const char kEnableRevenLogSourceDescription[];

extern const char kEnableSuggestedFilesName[];
extern const char kEnableSuggestedFilesDescription[];

extern const char kEnableSuggestedLocalFilesName[];
extern const char kEnableSuggestedLocalFilesDescription[];

extern const char kEnableVariableRefreshRateName[];
extern const char kEnableVariableRefreshRateDescription[];

extern const char kEnableWireGuardName[];
extern const char kEnableWireGuardDescription[];

extern const char kEnforceAshExtensionKeeplistName[];
extern const char kEnforceAshExtensionKeeplistDescription[];

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

extern const char kExperimentalAccessibilityDictationOfflineName[];
extern const char kExperimentalAccessibilityDictationOfflineDescription[];

extern const char kExperimentalAccessibilityDictationCommandsName[];
extern const char kExperimentalAccessibilityDictationCommandsDescription[];

extern const char kExperimentalAccessibilityDictationHintsName[];
extern const char kExperimentalAccessibilityDictationHintsDescription[];

extern const char kExperimentalAccessibilityDictationWithPumpkinName[];
extern const char kExperimentalAccessibilityDictationWithPumpkinDescription[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char
    kExperimentalAccessibilitySwitchAccessMultistepAutomationName[];
extern const char
    kExperimentalAccessibilitySwitchAccessMultistepAutomationDescription[];

extern const char kExtendedOpenVpnSettingsName[];
extern const char kExtendedOpenVpnSettingsDescription[];

extern const char kMagnifierContinuousMouseFollowingModeSettingName[];
extern const char kMagnifierContinuousMouseFollowingModeSettingDescription[];

extern const char kDockedMagnifierResizingName[];
extern const char kDockedMagnifierResizingDescription[];

extern const char kFilesArchivemountName[];
extern const char kFilesArchivemountDescription[];

extern const char kFilesArchivemount2Name[];
extern const char kFilesArchivemount2Description[];

extern const char kFilesExtractArchiveName[];
extern const char kFilesExtractArchiveDescription[];

extern const char kFilesSinglePartitionFormatName[];
extern const char kFilesSinglePartitionFormatDescription[];

extern const char kFilesSWAName[];
extern const char kFilesSWADescription[];

extern const char kFilesTrashName[];
extern const char kFilesTrashDescription[];

extern const char kFilesWebDriveOfficeName[];
extern const char kFilesWebDriveOfficeDescription[];

extern const char kFiltersInRecentsName[];
extern const char kFiltersInRecentsDescription[];

extern const char kFocusFollowsCursorName[];
extern const char kFocusFollowsCursorDescription[];

extern const char kFrameThrottleFpsName[];
extern const char kFrameThrottleFpsDescription[];
extern const char kFrameThrottleFpsDefault[];
extern const char kFrameThrottleFps5[];
extern const char kFrameThrottleFps10[];
extern const char kFrameThrottleFps15[];
extern const char kFrameThrottleFps20[];
extern const char kFrameThrottleFps25[];
extern const char kFrameThrottleFps30[];

extern const char kFullRestoreForLacrosName[];
extern const char kFullRestoreForLacrosDescription[];

extern const char kFuseBoxName[];
extern const char kFuseBoxDescription[];

extern const char kGuestOsFilesName[];
extern const char kGuestOsFilesDescription[];

extern const char kHelpAppBackgroundPageName[];
extern const char kHelpAppBackgroundPageDescription[];

extern const char kHelpAppDiscoverTabName[];
extern const char kHelpAppDiscoverTabDescription[];

extern const char kHelpAppLauncherSearchName[];
extern const char kHelpAppLauncherSearchDescription[];

extern const char kHelpAppSearchServiceIntegrationName[];
extern const char kHelpAppSearchServiceIntegrationDescription[];

extern const char kHoldingSpaceInProgressAnimationV2Name[];
extern const char kHoldingSpaceInProgressAnimationV2Description[];

extern const char kHoldingSpaceInProgressDownloadsIntegrationName[];
extern const char kHoldingSpaceInProgressDownloadsIntegrationDescription[];

extern const char kDiacriticsOnPhysicalKeyboardLongpressName[];
extern const char kDiacriticsOnPhysicalKeyboardLongpressDescription[];

extern const char kImeAssistAutocorrectName[];
extern const char kImeAssistAutocorrectDescription[];

extern const char kImeAssistEmojiEnhancedName[];
extern const char kImeAssistEmojiEnhancedDescription[];

extern const char kImeAssistMultiWordName[];
extern const char kImeAssistMultiWordDescription[];

extern const char kImeAssistMultiWordExpandedName[];
extern const char kImeAssistMultiWordExpandedDescription[];

extern const char kImeAssistMultiWordLacrosSupportName[];
extern const char kImeAssistMultiWordLacrosSupportDescription[];

extern const char kLacrosProfileMigrationForAnyUserName[];
extern const char kLacrosProfileMigrationForAnyUserDescription[];

extern const char kLacrosProfileMigrationForceOffName[];
extern const char kLacrosProfileMigrationForceOffDescription[];

extern const char kImeAssistPersonalInfoName[];
extern const char kImeAssistPersonalInfoDescription[];

extern const char kVirtualKeyboardDarkModeName[];
extern const char kVirtualKeyboardDarkModeDescription[];

extern const char kVirtualKeyboardNewHeaderName[];
extern const char kVirtualKeyboardNewHeaderDescription[];

extern const char kCrosLanguageSettingsImeOptionsInSettingsName[];
extern const char kCrosLanguageSettingsImeOptionsInSettingsDescription[];

extern const char kImeSystemEmojiPickerName[];
extern const char kImeSystemEmojiPickerDescription[];

extern const char kImeSystemEmojiPickerExtensionName[];
extern const char kImeSystemEmojiPickerExtensionDescription[];

extern const char kImeSystemEmojiPickerClipboardName[];
extern const char kImeSystemEmojiPickerClipboardDescription[];

extern const char kImeSystemEmojiPickerSearchExtensionName[];
extern const char kImeSystemEmojiPickerSearchExtensionDescription[];

extern const char kImeStylusHandwritingName[];
extern const char kImeStylusHandwritingDescription[];

extern const char kKeyboardBasedDisplayArrangementInSettingsName[];
extern const char kKeyboardBasedDisplayArrangementInSettingsDescription[];

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

extern const char kLacrosSupportName[];
extern const char kLacrosSupportDescription[];

extern const char kLimitShelfItemsToActiveDeskName[];
extern const char kLimitShelfItemsToActiveDeskDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kLocalWebApprovalsName[];
extern const char kLocalWebApprovalsDescription[];

extern const char kEnableHardwareMirrorModeName[];
extern const char kEnableHardwareMirrorModeDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMediaAppHandlesAudioName[];
extern const char kMediaAppHandlesAudioDescription[];

extern const char kMediaAppHandlesPdfName[];
extern const char kMediaAppHandlesPdfDescription[];

extern const char kMeteredShowToggleName[];
extern const char kMeteredShowToggleDescription[];

extern const char kMicrophoneMuteNotificationsName[];
extern const char kMicrophoneMuteNotificationsDescription[];

extern const char kMicrophoneMuteSwitchDeviceName[];
extern const char kMicrophoneMuteSwitchDeviceDescription[];

extern const char kMultilingualTypingName[];
extern const char kMultilingualTypingDescription[];

extern const char kNearbySharingArcName[];
extern const char kNearbySharingArcDescription[];

extern const char kNearbySharingBackgroundScanningName[];
extern const char kNearbySharingBackgroundScanningDescription[];

extern const char kNearbySharingOnePageOnboardingName[];
extern const char kNearbySharingOnePageOnboardingDescription[];

extern const char kNearbySharingReceiveWifiCredentialsName[];
extern const char kNearbySharingReceiveWifiCredentialsDescription[];

extern const char kNearbySharingSelfShareName[];
extern const char kNearbySharingSelfShareDescription[];

extern const char kNearbySharingVisibilityReminderName[];
extern const char kNearbySharingVisibilityReminderDescription[];

extern const char kNearbySharingWifiLanName[];
extern const char kNearbySharingWifiLanDescription[];

extern const char kOobeHidDetectionRevampName[];
extern const char kOobeHidDetectionRevampDescription[];

extern const char kPcieBillboardNotificationName[];
extern const char kPcieBillboardNotificationDescription[];

extern const char kPerformantSplitViewResizing[];
extern const char kPerformantSplitViewResizingDescription[];

extern const char kPhoneHubCallNotificationName[];
extern const char kPhoneHubCallNotificationDescription[];

extern const char kPhoneHubCameraRollName[];
extern const char kPhoneHubCameraRollDescription[];

extern const char kProductivityLauncherName[];
extern const char kProductivityLauncherDescription[];

extern const char kProjectorName[];
extern const char kProjectorDescription[];

extern const char kProjectorAnnotatorName[];
extern const char kProjectorAnnotatorDescription[];

extern const char kProjectorExcludeTranscriptName[];
extern const char kProjectorExcludeTranscriptDescription[];

extern const char kForceShowContinueSectionName[];
extern const char kForceShowContinueSectionDescription[];

extern const char kReduceDisplayNotificationsName[];
extern const char kReduceDisplayNotificationsDescription[];

extern const char kReleaseNotesNotificationAllChannelsName[];
extern const char kReleaseNotesNotificationAllChannelsDescription[];

extern const char kArcGhostWindowName[];
extern const char kArcGhostWindowDescription[];

extern const char kArcWindowPredictorName[];
extern const char kArcWindowPredictorDescription[];

extern const char kArcInputOverlayName[];
extern const char kArcInputOverlayDescription[];

extern const char kScanAppMultiPageScanName[];
extern const char kScanAppMultiPageScanDescription[];

extern const char kScanAppSearchablePdfName[];
extern const char kScanAppSearchablePdfDescription[];

extern const char kSecondaryGoogleAccountUsageName[];
extern const char kSecondaryGoogleAccountUsageDescription[];

extern const char kSharesheetCopyToClipboardName[];
extern const char kSharesheetCopyToClipboardDescription[];

extern const char kShimlessRMAFlowName[];
extern const char kShimlessRMAFlowDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kShowBluetoothDebugLogToggleName[];
extern const char kShowBluetoothDebugLogToggleDescription[];

extern const char kBluetoothSessionizedMetricsName[];
extern const char kBluetoothSessionizedMetricsDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSmartLockUIRevampName[];
extern const char kSmartLockUIRevampDescription[];

extern const char kSnoopingProtectionName[];
extern const char kSnoopingProtectionDescription[];

extern const char kSpectreVariant2MitigationName[];
extern const char kSpectreVariant2MitigationDescription[];

extern const char kSyncSettingsCategorizationName[];
extern const char kSyncSettingsCategorizationDescription[];

extern const char kSystemChinesePhysicalTypingName[];
extern const char kSystemChinesePhysicalTypingDescription[];

extern const char kSystemJapanesePhysicalTypingName[];
extern const char kSystemJapanesePhysicalTypingDescription[];

extern const char kSystemTransliterationPhysicalTypingName[];
extern const char kSystemTransliterationPhysicalTypingDescription[];

extern const char kQuickSettingsNetworkRevampName[];
extern const char kQuickSettingsNetworkRevampDescription[];

extern const char kTerminalSSHName[];
extern const char kTerminalSSHDescription[];

extern const char kTerminalTmuxIntegrationName[];
extern const char kTerminalTmuxIntegrationDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kTrafficCountersHandlerEnabledName[];
extern const char kTrafficCountersHandlerEnabledDescription[];

extern const char kTrafficCountersSettingsUiName[];
extern const char kTrafficCountersSettingsUiDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUsbNotificationControllerName[];
extern const char kUsbNotificationControllerDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kUseMultipleOverlaysName[];
extern const char kUseMultipleOverlaysDescription[];

extern const char kUXStudy1Name[];
extern const char kUXStudy1Description[];

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

extern const char kWakeOnWifiAllowedName[];
extern const char kWakeOnWifiAllowedDescription[];

extern const char kWebAppsCrosapiName[];
extern const char kWebAppsCrosapiDescription[];

extern const char kWifiConnectMacAddressRandomizationName[];
extern const char kWifiConnectMacAddressRandomizationDescription[];

extern const char kWifiSyncAllowDeletesName[];
extern const char kWifiSyncAllowDeletesDescription[];

extern const char kWifiSyncAndroidName[];
extern const char kWifiSyncAndroidDescription[];

extern const char kWindowControlMenu[];
extern const char kWindowControlMenuDescription[];

extern const char kLauncherNudgeName[];
extern const char kLauncherNudgeDescription[];

extern const char kLauncherNudgeShortIntervalName[];
extern const char kLauncherNudgeShortIntervalDescription[];

extern const char kLauncherNudgeSessionResetName[];
extern const char kLauncherNudgeSessionResetDescription[];

// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kDesktopCaptureLacrosV2Name[];
extern const char kDesktopCaptureLacrosV2Description[];

extern const char kLacrosMergeIcuDataFileName[];
extern const char kLacrosMergeIcuDataFileDescription[];

extern const char kLacrosNonSyncingProfilesName[];
extern const char kLacrosNonSyncingProfilesDescription[];

extern const char kLacrosResourcesFileSharingName[];
extern const char kLacrosResourcesFileSharingDescription[];

extern const char kLacrosScreenCoordinatesEnabledName[];
extern const char kLacrosScreenCoordinatesEnabledDescription[];
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kAllowDefaultWebAppMigrationForChromeOsManagedUsersName[];
extern const char
    kAllowDefaultWebAppMigrationForChromeOsManagedUsersDescription[];

extern const char kBluetoothAdvertisementMonitoringName[];
extern const char kBluetoothAdvertisementMonitoringDescription[];

extern const char kCrOSDspBasedAecAllowedName[];
extern const char kCrOSDspBasedAecAllowedDescription[];

extern const char kCrOSDspBasedNsAllowedName[];
extern const char kCrOSDspBasedNsAllowedDescription[];

extern const char kCrOSDspBasedAgcAllowedName[];
extern const char kCrOSDspBasedAgcAllowedDescription[];

extern const char kCrOSEnforceSystemAecName[];
extern const char kCrOSEnforceSystemAecDescription[];

extern const char kCrOSEnforceSystemAecAgcName[];
extern const char kCrOSEnforceSystemAecAgcDescription[];

extern const char kCrOSEnforceSystemAecNsName[];
extern const char kCrOSEnforceSystemAecNsDescription[];

extern const char kCrOSEnforceSystemAecNsAgcName[];
extern const char kCrOSEnforceSystemAecNsAgcDescription[];

extern const char kDefaultCalculatorWebAppName[];
extern const char kDefaultCalculatorWebAppDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kDeprecateLowUsageCodecsName[];
extern const char kDeprecateLowUsageCodecsDescription[];

extern const char kVaapiAV1DecoderName[];
extern const char kVaapiAV1DecoderDescription[];

extern const char kEnableTtsLacrosSupportName[];
extern const char kEnableTtsLacrosSupportDescription[];

extern const char kIntentChipSkipsPickerName[];
extern const char kIntentChipSkipsPickerDescription[];

extern const char kLinkCapturingInfoBarName[];
extern const char kLinkCapturingInfoBarDescription[];

extern const char kLinkCapturingUiUpdateName[];
extern const char kLinkCapturingUiUpdateDescription[];

extern const char kMessagesPreinstallName[];
extern const char kMessagesPreinstallDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
extern const char kVaapiVP9kSVCEncoderName[];
extern const char kVaapiVP9kSVCEncoderDescription[];
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
extern const char kChromeOSDirectVideoDecoderName[];
extern const char kChromeOSDirectVideoDecoderDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kZeroCopyVideoCaptureName[];
extern const char kZeroCopyVideoCaptureDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)
extern const char kDownloadShelfWebUI[];
extern const char kDownloadShelfWebUIDescription[];
#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

extern const char kWebuiFeedbackName[];
extern const char kWebuiFeedbackDescription[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)

extern const char kDesktopRestructuredLanguageSettingsName[];
extern const char kDesktopRestructuredLanguageSettingsDescription[];

extern const char kDesktopDetailedLanguageSettingsName[];
extern const char kDesktopDetailedLanguageSettingsDescription[];

extern const char kQuickCommandsName[];
extern const char kQuickCommandsDescription[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // defined (OS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
extern const char kWebShareName[];
extern const char kWebShareDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
extern const char kOzonePlatformHintChoiceDefault[];
extern const char kOzonePlatformHintChoiceAuto[];
extern const char kOzonePlatformHintChoiceX11[];
extern const char kOzonePlatformHintChoiceWayland[];

extern const char kOzonePlatformHintName[];
extern const char kOzonePlatformHintDescription[];

extern const char kCleanUndecryptablePasswordsLinuxName[];
extern const char kCleanUndecryptablePasswordsLinuxDescription[];

extern const char kForcePasswordInitialSyncWhenDecryptionFailsName[];
extern const char kForcePasswordInitialSyncWhenDecryptionFailsDescription[];
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
extern const char kSkipUndecryptablePasswordsName[];
extern const char kSkipUndecryptablePasswordsDescription[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_JXL_DECODER)
extern const char kEnableJXLName[];
extern const char kEnableJXLDescription[];
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

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
extern const char kPaintPreviewStartupName[];
extern const char kPaintPreviewStartupDescription[];
#endif  // ENABLE_PAINT_PREVIEW && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
extern const char kSideSearchName[];
extern const char kSideSearchDescription[];

extern const char kSideSearchClearCacheWhenClosedName[];
extern const char kSideSearchClearCacheWhenClosedDescription[];

extern const char kSideSearchDSESupportName[];
extern const char kSideSearchDSESupportDescription[];
#endif  // BUILDFLAG(ENABLE_SIDE_SEARCH)

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
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
extern const char kWebrtcPipeWireCapturerName[];
extern const char kWebrtcPipeWireCapturerDescription[];
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kWebKioskEnableLacrosName[];
extern const char kWebKioskEnableLacrosDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
