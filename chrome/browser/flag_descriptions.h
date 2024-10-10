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
#include "components/compose/buildflags.h"
#include "components/flags_ui/feature_entry.h"
#include "components/nacl/common/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "net/net_buildflags.h"
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

extern const char kAlignWakeUpsName[];
extern const char kAlignWakeUpsDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAndroidAppIntegrationName[];
extern const char kAndroidAppIntegrationDescription[];

extern const char kAndroidAppIntegrationWithFaviconName[];
extern const char kAndroidAppIntegrationWithFaviconDescription[];

extern const char kAndroidBottomToolbarName[];
extern const char kAndroidBottomToolbarDescription[];

extern const char kAndroidExtendedKeyboardShortcutsName[];
extern const char kAndroidExtendedKeyboardShortcutsDescription[];

extern const char kAndroidNoSurfaceSyncForBrowserControlsName[];
extern const char kAndroidNoSurfaceSyncForBrowserControlsDescription[];

extern const char kAndroidBrowserControlsInVizName[];
extern const char kAndroidBrowserControlsInVizDescription[];

extern const char kAnimatedImageResumeName[];
extern const char kAnimatedImageResumeDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kAnnotatorModeName[];
extern const char kAnnotatorModeDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kAriaElementReflectionName[];
extern const char kAriaElementReflectionDescription[];

extern const char kAutomotiveFullscreenToolbarImprovementsName[];
extern const char kAutomotiveFullscreenToolbarImprovementsDescription[];

extern const char kAuxiliarySearchDonationName[];
extern const char kAuxiliarySearchDonationDescription[];

extern const char kBackgroundResourceFetchName[];
extern const char kBackgroundResourceFetchDescription[];

extern const char
    kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsName[];
extern const char
    kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kBirchWeatherName[];
extern const char kBirchWeatherDescription[];

extern const char kCampaignsComponentUpdaterTestTagName[];
extern const char kCampaignsComponentUpdaterTestTagDescription[];
extern const char kCampaignsOverrideName[];
extern const char kCampaignsOverrideDescription[];
#endif  // IS_CHROMEOS_ASH

extern const char kCOLRV1FontsDescription[];

extern const char kCertVerificationNetworkTimeName[];
extern const char kCertVerificationNetworkTimeDescription[];

extern const char kClassifyUrlOnProcessResponseEventName[];
extern const char kClassifyUrlOnProcessResponseEventDescription[];

extern const char kClearUndecryptablePasswordsName[];
extern const char kClearUndecryptablePasswordsDescription[];

extern const char kClickToCallName[];
extern const char kClickToCallDescription[];

extern const char kClipboardMaximumAgeName[];
extern const char kClipboardMaximumAgeDescription[];

extern const char kComputePressureRateObfuscationMitigationName[];
extern const char kComputePressureRateObfuscationMitigationDescription[];

extern const char kComputePressureBreakCalibrationMitigationName[];
extern const char kComputePressureBreakCalibrationMitigationDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kContainerName[];
extern const char kContainerDescription[];
extern const char kContainerDebugName[];
extern const char kContainerDebugDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kContainerTypeNoLayoutContainmentName[];
extern const char kContainerTypeNoLayoutContainmentDescription[];

extern const char kContentSettingsPartitioningName[];
extern const char kContentSettingsPartitioningDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kCoralFeatureKeyName[];
extern const char kCoralFeatureKeyDescription[];

extern const char kCrosSwitcherName[];
extern const char kCrosSwitcherDescription[];
#endif  // IS_CHROMEOS_ASH

extern const char kCssGamutMappingName[];
extern const char kCssGamutMappingDescription[];

extern const char kCssTextBoxTrimName[];
extern const char kCssTextBoxTrimDescription[];

extern const char kCustomizeChromeSidePanelExtensionsCardName[];
extern const char kCustomizeChromeSidePanelExtensionsCardDescription[];

extern const char kCustomizeChromeWallpaperSearchName[];
extern const char kCustomizeChromeWallpaperSearchDescription[];

extern const char kCustomizeChromeWallpaperSearchButtonName[];
extern const char kCustomizeChromeWallpaperSearchButtonDescription[];

extern const char kCustomizeChromeWallpaperSearchInspirationCardName[];
extern const char kCustomizeChromeWallpaperSearchInspirationCardDescription[];

extern const char kDataSharingName[];
extern const char kDataSharingDescription[];

extern const char kDataSharingJoinOnlyName[];
extern const char kDataSharingJoinOnlyDescription[];

extern const char kHistorySyncAlternativeIllustrationName[];
extern const char kHistorySyncAlternativeIllustrationDescription[];

extern const char kDeprecateUnloadName[];
extern const char kDeprecateUnloadDescription[];

extern const char kDeferRendererTasksAfterInputName[];
extern const char kDeferRendererTasksAfterInputDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kDemoModeComponentUpdaterTestTagName[];
extern const char kDemoModeComponentUpdaterTestTagDescription[];
#endif  // IS_CHROMEOS_ASH

extern const char kDevToolsPrivacyUIName[];
extern const char kDevToolsPrivacyUIDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kDisableInstanceLimitName[];
extern const char kDisableInstanceLimitDescription[];
#endif

extern const char kDownloadWarningImprovementsName[];
extern const char kDownloadWarningImprovementsDescription[];

extern const char kEnableBenchmarkingName[];
extern const char kEnableBenchmarkingDescription[];
extern const char kEnableBenchmarkingChoiceDisabled[];
extern const char kEnableBenchmarkingChoiceDefaultFeatureStates[];
extern const char kEnableBenchmarkingChoiceMatchFieldTrialTestingConfig[];

extern const char kEnableBookmarksSelectedTypeOnSigninForTestingName[];
extern const char kEnableBookmarksSelectedTypeOnSigninForTestingDescription[];

extern const char kFontationsFontBackendName[];
extern const char kFontationsFontBackendDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kForestFeatureName[];
extern const char kForestFeatureDescription[];
#endif

extern const char kAvifGainmapHdrImagesName[];
extern const char kAvifGainmapHdrImagesDescription[];

extern const char kCrabbyAvifName[];
extern const char kCrabbyAvifDescription[];

extern const char kTangibleSyncName[];
extern const char kTangibleSyncDescription[];

extern const char kMainThreadCompositingPriorityName[];
extern const char kMainThreadCompositingPriorityDescription[];

extern const char kMediaSessionEnterPictureInPictureName[];
extern const char kMediaSessionEnterPictureInPictureDescription[];

extern const char kEnableBluetoothSerialPortProfileInSerialApiName[];
extern const char kEnableBluetoothSerialPortProfileInSerialApiDescription[];

extern const char kEnableDrDcName[];
extern const char kEnableDrDcDescription[];

extern const char kEnableExtensionsPermissionsForSupervisedUsersOnDesktopName[];
extern const char
    kEnableExtensionsPermissionsForSupervisedUsersOnDesktopDescription[];

extern const char
    kEnableSupervisedUserSkipParentApprovalToInstallExtensionsName[];
extern const char
    EnableSupervisedUserSkipParentApprovalToInstallExtensionsDescription[];

extern const char kEnableSupervisedUserProfileSignInIphName[];
extern const char kEnableSupervisedUserProfileSignInIphDescription[];

extern const char kUpdatedSupervisedUserExtensionApprovalStringsName[];
extern const char kUpdatedSupervisedUserExtensionApprovalStringsDescription[];

extern const char kUseAndroidStagingSmdsName[];
extern const char kUseAndroidStagingSmdsDescription[];

extern const char kUseFrameIntervalDeciderName[];
extern const char kUseFrameIntervalDeciderDescription[];

extern const char kUseSharedImagesForPepperVideoName[];
extern const char kUseSharedImagesForPepperVideoDescription[];

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

extern const char kImageDescriptionsAlternateRoutingName[];
extern const char kImageDescriptionsAlternateRoutingDescription[];

extern const char kImprovedSigninUIOnDesktopName[];
extern const char kImprovedSigninUIOnDesktopDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kAutofillEnableAmountExtractionDesktopName[];
extern const char kAutofillEnableAmountExtractionDesktopDescription[];
#endif

extern const char kAutofillEnableAndroidNKeyForFidoAuthenticationName[];
extern const char kAutofillEnableAndroidNKeyForFidoAuthenticationDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kAutofillEnableBuyNowPayLaterForAffirmName[];
extern const char kAutofillEnableBuyNowPayLaterForAffirmDescription[];

extern const char kAutofillEnableBuyNowPayLaterForZipName[];
extern const char kAutofillEnableBuyNowPayLaterForZipDescription[];

extern const char kAutofillEnableBuyNowPayLaterSyncingName[];
extern const char kAutofillEnableBuyNowPayLaterSyncingDescription[];
#endif

extern const char kAutofillEnableCvcStorageAndFillingName[];
extern const char kAutofillEnableCvcStorageAndFillingDescription[];

extern const char kAutofillEnableCvcStorageAndFillingEnhancementName[];
extern const char kAutofillEnableCvcStorageAndFillingEnhancementDescription[];

extern const char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName[];
extern const char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription[];

extern const char kAutofillEnableFpanRiskBasedAuthenticationName[];
extern const char kAutofillEnableFpanRiskBasedAuthenticationDescription[];

extern const char kAutofillEnableManualFallbackIPHName[];
extern const char kAutofillEnableManualFallbackIPHDescription[];

extern const char kAutofillEnableCardArtImageName[];
extern const char kAutofillEnableCardArtImageDescription[];

extern const char kAutofillEnableCardArtServerSideStretchingName[];
extern const char kAutofillEnableCardArtServerSideStretchingDescription[];

extern const char kAutofillEnableCardBenefitsForAmericanExpressName[];
extern const char kAutofillEnableCardBenefitsForAmericanExpressDescription[];

extern const char kAutofillEnableCardBenefitsForCapitalOneName[];
extern const char kAutofillEnableCardBenefitsForCapitalOneDescription[];

extern const char kAutofillEnableCardBenefitsIphName[];
extern const char kAutofillEnableCardBenefitsIphDescription[];

extern const char kAutofillEnableCardBenefitsSyncName[];
extern const char kAutofillEnableCardBenefitsSyncDescription[];

extern const char kAutofillEnableCardInfoRuntimeRetrievalName[];
extern const char kAutofillEnableCardInfoRuntimeRetrievalDescription[];

extern const char kAutofillEnableCardProductNameName[];
extern const char kAutofillEnableCardProductNameDescription[];

extern const char kAutofillGranularFillingAvailableName[];
extern const char kAutofillGranularFillingAvailableDescription[];

extern const char kAutofillForUnclassifiedFieldsAvailableName[];
extern const char kAutofillForUnclassifiedFieldsAvailableDescription[];

extern const char kAutofillEnableLocalIbanName[];
extern const char kAutofillEnableLocalIbanDescription[];

extern const char kAutofillEnableLogFormEventsToAllParsedFormTypesName[];
extern const char kAutofillEnableLogFormEventsToAllParsedFormTypesDescription[];

extern const char kAutofillEnableNewCardArtAndNetworkImagesName[];
extern const char kAutofillEnableNewCardArtAndNetworkImagesDescription[];

extern const char kAutofillEnableOffersInClankKeyboardAccessoryName[];
extern const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAutofillEnablePaymentSettingsCardPromoAndScanCardName[];
extern const char
    kAutofillEnablePaymentSettingsCardPromoAndScanCardDescription[];

extern const char kAutofillEnablePaymentSettingsServerCardSaveName[];
extern const char kAutofillEnablePaymentSettingsServerCardSaveDescription[];
#endif

extern const char kAutofillEnablePrefetchingRiskDataForRetrievalName[];
extern const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[];

extern const char kAutofillEnableRankingFormulaAddressProfilesName[];
extern const char kAutofillEnableRankingFormulaAddressProfilesDescription[];

extern const char kAutofillEnableRankingFormulaCreditCardsName[];
extern const char kAutofillEnableRankingFormulaCreditCardsDescription[];

extern const char kAutofillEnableSaveCardLoadingAndConfirmationName[];
extern const char kAutofillEnableSaveCardLoadingAndConfirmationDescription[];

extern const char kAutofillEnableSaveCardLocalSaveFallbackName[];
extern const char kAutofillEnableSaveCardLocalSaveFallbackDescription[];

extern const char kAutofillEnableServerIbanName[];
extern const char kAutofillEnableServerIbanDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAutofillEnableSyncingOfPixBankAccountsName[];
extern const char kAutofillEnableSyncingOfPixBankAccountsDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kAutofillEnableVcn3dsAuthenticationName[];
extern const char kAutofillEnableVcn3dsAuthenticationDescription[];

extern const char kAutofillEnableVcnGrayOutForMerchantOptOutName[];
extern const char kAutofillEnableVcnGrayOutForMerchantOptOutDescription[];

extern const char kAutofillEnableVcnEnrollLoadingAndConfirmationName[];
extern const char kAutofillEnableVcnEnrollLoadingAndConfirmationDescription[];

extern const char kAutofillEnableVerveCardSupportName[];
extern const char kAutofillEnableVerveCardSupportDescription[];

extern const char kAutofillEnableVirtualCardMetadataName[];
extern const char kAutofillEnableVirtualCardMetadataDescription[];

extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[];
extern const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[];

extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[];
extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[];

extern const char kAutofillRemovePaymentsButterDropdownName[];
extern const char kAutofillRemovePaymentsButterDropdownDescription[];

extern const char kAutofillRequireValidLocalCardsInSettingsName[];
extern const char kAutofillRequireValidLocalCardsInSettingsDescription[];

extern const char kAutofillSharedStorageServerCardDataName[];
extern const char kAutofillSharedStorageServerCardDataDescription[];

extern const char kAutofillSkipAndroidBottomSheetForIbanName[];
extern const char kAutofillSkipAndroidBottomSheetForIbanDescription[];

extern const char kAutofillMoreProminentPopupName[];
extern const char kAutofillMoreProminentPopupDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAutofillSyncEwalletAccountsName[];
extern const char kAutofillSyncEwalletAccountsDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kAutofillUnmaskCardRequestTimeoutName[];
extern const char kAutofillUnmaskCardRequestTimeoutDescription[];

extern const char kAutofillUploadCardRequestTimeoutName[];
extern const char kAutofillUploadCardRequestTimeoutDescription[];

extern const char kAutofillUpstreamUpdatedUiName[];
extern const char kAutofillUpstreamUpdatedUiDescription[];

extern const char kAutofillVcnEnrollRequestTimeoutName[];
extern const char kAutofillVcnEnrollRequestTimeoutDescription[];

extern const char kAutofillVirtualViewStructureAndroidName[];
extern const char kAutofillVirtualViewStructureAndroidDescription[];

extern const char kAutoPictureInPictureForVideoPlaybackName[];
extern const char kAutoPictureInPictureForVideoPlaybackDescription[];

extern const char kAutoPictureInPictureVideoHeuristicsName[];
extern const char kAutoPictureInPictureVideoHeuristicsDescription[];

extern const char kBackForwardCacheName[];
extern const char kBackForwardCacheDescription[];

extern const char kBackForwardTransitionsName[];
extern const char kBackForwardTransitionsDescription[];

extern const char kBackgroundListeningName[];
extern const char kBackgroundListeningDescription[];

extern const char kBiometricReauthForPasswordFillingName[];
extern const char kBiometricReauthForPasswordFillingDescription[];

extern const char kBindCookiesToPortName[];
extern const char kBindCookiesToPortDescription[];

extern const char kBindCookiesToSchemeName[];
extern const char kBindCookiesToSchemeDescription[];

extern const char kFailFastQuietChipName[];
extern const char kFailFastQuietChipDescription[];

extern const char kBorealisBigGlName[];
extern const char kBorealisBigGlDescription[];

extern const char kBorealisDGPUName[];
extern const char kBorealisDGPUDescription[];

extern const char kBorealisEnableUnsupportedHardwareName[];
extern const char kBorealisEnableUnsupportedHardwareDescription[];

extern const char kBorealisForceBetaClientName[];
extern const char kBorealisForceBetaClientDescription[];

extern const char kBorealisForceDoubleScaleName[];
extern const char kBorealisForceDoubleScaleDescription[];

extern const char kBorealisLinuxModeName[];
extern const char kBorealisLinuxModeDescription[];

extern const char kBorealisPermittedName[];
extern const char kBorealisPermittedDescription[];

extern const char kBorealisProvisionName[];
extern const char kBorealisProvisionDescription[];

extern const char kBorealisScaleClientByDPIName[];
extern const char kBorealisScaleClientByDPIDescription[];

extern const char kBorealisZinkGlDriverName[];
extern const char kBorealisZinkGlDriverDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kSearchInCCTName[];
extern const char kSearchInCCTDescription[];
extern const char kSearchInCCTAlternateTapHandlingName[];
extern const char kSearchInCCTAlternateTapHandlingDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kSeparateWebAppShortcutBadgeIconName[];
extern const char kSeparateWebAppShortcutBadgeIconDescription[];

extern const char kSeparateLocalAndAccountThemesName[];
extern const char kSeparateLocalAndAccountThemesDescription[];

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
extern const char kCameraMicEffectsName[];
extern const char kCameraMicEffectsDescription[];

extern const char kCameraMicPreviewName[];
extern const char kCameraMicPreviewDescription[];

extern const char kGetUserMediaDeferredDeviceSettingsSelectionName[];
extern const char kGetUserMediaDeferredDeviceSettingsSelectionDescription[];
#endif

extern const char kCanvasHibernationName[];
extern const char kCanvasHibernationDescription[];

extern const char kCanvasOopRasterizationName[];
extern const char kCanvasOopRasterizationDescription[];

#if BUILDFLAG(IS_CHROMEOS)
extern const char kCaptivePortalPopupWindowName[];
extern const char kCaptivePortalPopupWindowDescription[];
#endif

extern const char kCapturedSurfaceControlName[];
extern const char kCapturedSurfaceControlDescription[];

extern const char kChromeLabsName[];
extern const char kChromeLabsDescription[];

extern const char kCommerceHintAndroidName[];
extern const char kCommerceHintAndroidDescription[];

extern const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[];
extern const char
    kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[];

extern const char kDisableProcessReuse[];
extern const char kDisableProcessReuseDescription[];

extern const char kDiscountConsentV2Name[];
extern const char kDiscountConsentV2Description[];

extern const char kDisruptiveNotificationPermissionRevocationName[];
extern const char kDisruptiveNotificationPermissionRevocationDescription[];

extern const char kDoubleBufferCompositingName[];
extern const char kDoubleBufferCompositingDescription[];

extern const char kCodeBasedRBDName[];
extern const char kCodeBasedRBDDescription[];

extern const char kCompressionDictionaryTransportName[];
extern const char kCompressionDictionaryTransportDescription[];

extern const char kCompressionDictionaryTransportBackendName[];
extern const char kCompressionDictionaryTransportBackendDescription[];

extern const char kCompressionDictionaryTransportOverHttp1Name[];
extern const char kCompressionDictionaryTransportOverHttp1Description[];

extern const char kCompressionDictionaryTransportOverHttp2Name[];
extern const char kCompressionDictionaryTransportOverHttp2Description[];

extern const char kCompressionDictionaryTransportRequireKnownRootCertName[];
extern const char
    kCompressionDictionaryTransportRequireKnownRootCertDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kContextualSearchWithCredentialsForDebugName[];
extern const char kContextualSearchWithCredentialsForDebugDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kUseDMSAAForTilesName[];
extern const char kUseDMSAAForTilesDescription[];

extern const char kUseDMSAAForTilesAndroidGLName[];
extern const char kUseDMSAAForTilesAndroidGLDescription[];

extern const char kUseDnsHttpsSvcbAlpnName[];
extern const char kUseDnsHttpsSvcbAlpnDescription[];

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

extern const char kViewTransitionOnNavigationName[];
extern const char kViewTransitionOnNavigationDescription[];

extern const char kViewTransitionOnNavigationIframeName[];
extern const char kViewTransitionOnNavigationIframeDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnterpriseProfileBadgingForAvatarName[];
extern const char kEnterpriseProfileBadgingForAvatarDescription[];

extern const char kEnterpriseProfileBadgingForMenuName[];
extern const char kEnterpriseProfileBadgingForMenuDescription[];

extern const char kEnterpriseUpdatedProfileCreationScreenName[];
extern const char kEnterpriseUpdatedProfileCreationScreenDescription[];

extern const char kEnableExperimentalCookieFeaturesName[];
extern const char kEnableExperimentalCookieFeaturesDescription[];

extern const char kEnableRawDrawName[];
extern const char kEnableRawDrawDescription[];

extern const char kEnableDelegatedCompositingName[];
extern const char kEnableDelegatedCompositingDescription[];

extern const char kEnableRenderPassDrawnRectName[];
extern const char kEnableRenderPassDrawnRectDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kEnablePixPaymentsName[];
extern const char kEnablePixPaymentsDescription[];

extern const char kEnablePixPaymentsInLandscapeModeName[];
extern const char kEnablePixPaymentsInLandscapeModeDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kEnableRemovingAllThirdPartyCookiesName[];
extern const char kEnableRemovingAllThirdPartyCookiesDescription[];

extern const char kDesktopPWAsAdditionalWindowingControlsName[];
extern const char kDesktopPWAsAdditionalWindowingControlsDescription[];

extern const char kDesktopPWAsAppTitleName[];
extern const char kDesktopPWAsAppTitleDescription[];

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

extern const char kDesktopPWAsScopeExtensionsName[];
extern const char kDesktopPWAsScopeExtensionsDescription[];

extern const char kDesktopPWAsBorderlessName[];
extern const char kDesktopPWAsBorderlessDescription[];

extern const char kDeviceForceScheduledRebootName[];
extern const char kDeviceForceScheduledRebootDescription[];

extern const char kDevicePostureName[];
extern const char kDevicePostureDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kDocumentPictureInPictureAnimateResizeName[];
extern const char kDocumentPictureInPictureAnimateResizeDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

extern const char kEnableTLS13EarlyDataName[];
extern const char kEnableTLS13EarlyDataDescription[];

extern const char kEnableTLS13KyberName[];
extern const char kEnableTLS13KyberDescription[];

extern const char kUseMLKEMName[];
extern const char kUseMLKEMDescription[];

extern const char kAccessibilityAcceleratorName[];
extern const char kAccessibilityAcceleratorDescription[];

extern const char kAccessibilityCaretBlinkIntervalSettingName[];
extern const char kAccessibilityCaretBlinkIntervalSettingDescription[];

extern const char kAccessibilityDisableTrackpadName[];
extern const char kAccessibilityDisableTrackpadDescription[];

extern const char kAccessibilityFlashScreenFeatureName[];
extern const char kAccessibilityFlashScreenFeatureDescription[];

extern const char kAccessibilityOverscrollSettingFeatureName[];
extern const char kAccessibilityOverscrollSettingFeatureDescription[];

extern const char kAccessibilityServiceName[];
extern const char kAccessibilityServiceDescription[];

extern const char kAccessibilityShakeToLocateName[];
extern const char kAccessibilityShakeToLocateDescription[];

extern const char kExperimentalAccessibilityColorEnhancementSettingsName[];
extern const char
    kExperimentalAccessibilityColorEnhancementSettingsDescription[];

extern const char kAccessibilityChromeVoxPageMigrationName[];
extern const char kAccessibilityChromeVoxPageMigrationDescription[];

extern const char kAccessibilityReducedAnimationsName[];
extern const char kAccessibilityReducedAnimationsDescription[];

extern const char kAccessibilityReducedAnimationsInKioskName[];
extern const char kAccessibilityReducedAnimationsInKioskDescription[];

extern const char kAccessibilityFaceGazeName[];
extern const char kAccessibilityFaceGazeDescription[];

extern const char kAccessibilityFaceGazeGravityWellsName[];
extern const char kAccessibilityFaceGazeGravityWellsDescription[];

extern const char kAccessibilityMagnifierFollowsChromeVoxName[];
extern const char kAccessibilityMagnifierFollowsChromeVoxDescription[];

extern const char kAccessibilityMagnifierFollowsStsName[];
extern const char kAccessibilityMagnifierFollowsStsDescription[];

extern const char kAccessibilityMagnifyAcceleratorDialogName[];
extern const char kAccessibilityMagnifyAcceleratorDialogDescription[];

extern const char kAccessibilityMouseKeysName[];
extern const char kAccessibilityMouseKeysDescription[];

extern const char kAccessibilitySelectToSpeakShortcutName[];
extern const char kAccessibilitySelectToSpeakShortcutDescription[];

extern const char kAccessibilitySelectToSpeakPrefsMigrationName[];
extern const char kAccessibilitySelectToSpeakPrefsMigrationDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kBiometricAuthIdentityCheckName[];
extern const char kBiometricAuthIdentityCheckDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kNewMacNotificationAPIName[];
extern const char kNewMacNotificationAPIDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableWindowsGamingInputDataFetcherName[];
extern const char kEnableWindowsGamingInputDataFetcherDescription[];

extern const char kBlockInsecurePrivateNetworkRequestsName[];
extern const char kBlockInsecurePrivateNetworkRequestsDescription[];

extern const char kPipDoubleTapToResizeName[];
extern const char kPipDoubleTapToResizeDescription[];

extern const char kPipTuckName[];
extern const char kPipTuckDescription[];

extern const char kPrivateNetworkAccessSendPreflightsName[];
extern const char kPrivateNetworkAccessSendPreflightsDescription[];

extern const char kPrivateNetworkAccessRespectPreflightResultsName[];
extern const char kPrivateNetworkAccessRespectPreflightResultsDescription[];

extern const char kPrivateNetworkAccessPreflightShortTimeoutName[];
extern const char kPrivateNetworkAccessPreflightShortTimeoutDescription[];

extern const char kPrivateNetworkAccessPermissionPromptName[];
extern const char kPrivateNetworkAccessPermissionPromptDescription[];

extern const char kPrivateNetworkAccessIgnoreWorkerErrorsName[];
extern const char kPrivateNetworkAccessIgnoreWorkerErrorsDescription[];

extern const char kPrivateNetworkAccessIgnoreNavigationErrorsName[];
extern const char kPrivateNetworkAccessIgnoreNavigationErrorsDescription[];

extern const char kDeprecateAltClickName[];
extern const char kDeprecateAltClickDescription[];

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

extern const char kMirrorBackForwardGesturesInRTLName[];
extern const char kMirrorBackForwardGesturesInRTLDescription[];

extern const char kClayBlockingDialogName[];
extern const char kClayBlockingDialogDescription[];

extern const char kTemplateUrlReconciliationName[];
extern const char kTemplateUrlReconciliationDialogDescription[];

extern const char kEnableFencedFramesName[];
extern const char kEnableFencedFramesDescription[];

extern const char kEnableFencedFramesDeveloperModeName[];
extern const char kEnableFencedFramesDeveloperModeDescription[];

extern const char kEnableFencedFramesM120FeaturesName[];
extern const char kEnableFencedFramesM120FeaturesDescription[];

extern const char kEnableGamepadButtonAxisEventsName[];
extern const char kEnableGamepadButtonAxisEventsDescription[];

extern const char kEnableGamepadMultitouchName[];
extern const char kEnableGamepadMultitouchDescription[];

extern const char kEnableIsolatedWebAppsName[];
extern const char kEnableIsolatedWebAppsDescription[];

#if BUILDFLAG(IS_CHROMEOS)
extern const char kEnableIsolatedWebAppAutomaticUpdatesName[];
extern const char kEnableIsolatedWebAppAutomaticUpdatesDescription[];

extern const char kEnableIsolatedWebAppUnmanagedInstallName[];
extern const char kEnableIsolatedWebAppUnmanagedInstallDescription[];

extern const char kEnableIsolatedWebAppManagedGuestSessionInstallName[];
extern const char kEnableIsolatedWebAppManagedGuestSessionInstallDescription[];
#endif

extern const char kEnableIsolatedWebAppDevModeName[];
extern const char kEnableIsolatedWebAppDevModeDescription[];

extern const char kEnableControlledFrameName[];
extern const char kEnableControlledFrameDescription[];

extern const char kEnableFingerprintingProtectionBlocklistName[];
extern const char kEnableFingerprintingProtectionBlocklistDescription[];

extern const char kEnableLensStandaloneFlagId[];
extern const char kEnableLensStandaloneName[];
extern const char kEnableLensStandaloneDescription[];

extern const char kEnableManagedConfigurationWebApiName[];
extern const char kEnableManagedConfigurationWebApiDescription[];

extern const char kDownloadNotificationServiceUnifiedAPIName[];
extern const char kDownloadNotificationServiceUnifiedAPIDescription[];

extern const char kDownloadsMigrateToJobsAPIName[];
extern const char kDownloadsMigrateToJobsAPIDescription[];

extern const char kEnablePerfettoSystemTracingName[];
extern const char kEnablePerfettoSystemTracingDescription[];

extern const char kEnablePeripheralCustomizationName[];
extern const char kEnablePeripheralCustomizationDescription[];

extern const char kEnablePeripheralNotificationName[];
extern const char kEnablePeripheralNotificationDescription[];

extern const char kEnablePeripheralsLoggingName[];
extern const char kEnablePeripheralsLoggingDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableProcessPerSiteUpToMainFrameThresholdName[];
extern const char kEnableProcessPerSiteUpToMainFrameThresholdDescription[];

extern const char kEnableShortcutCustomizationName[];
extern const char kEnableShortcutCustomizationDescription[];

extern const char kEnableSearchCustomizableShortcutsInLauncherName[];
extern const char kEnableSearchCustomizableShortcutsInLauncherDescription[];

extern const char kEnableSuspendStateMachineName[];
extern const char kEnableSuspendStateMachineDescription[];

extern const char kEnableInputDeviceSettingsSplitName[];
extern const char kEnableInputDeviceSettingsSplitDescription[];

extern const char kExperimentalRgbKeyboardPatternsName[];
extern const char kExperimentalRgbKeyboardPatternsDescription[];

extern const char kRetailCouponsName[];
extern const char kRetailCouponsDescription[];

extern const char kDropInputEventsBeforeFirstPaintName[];
extern const char kDropInputEventsBeforeFirstPaintDescription[];

extern const char kBoundaryEventDispatchTracksNodeRemovalName[];
extern const char kBoundaryEventDispatchTracksNodeRemovalDescription[];

extern const char kEnableBubbleCornerRadiusUpdateName[];
extern const char kEnableBubbleCornerRadiusUpdateDescription[];

extern const char kEnableCssSelectorFragmentAnchorName[];
extern const char kEnableCssSelectorFragmentAnchorDescription[];

extern const char kEnablePreferencesAccountStorageName[];
extern const char kEnablePreferencesAccountStorageDescription[];

extern const char kEnableResamplingScrollEventsExperimentalPredictionName[];
extern const char
    kEnableResamplingScrollEventsExperimentalPredictionDescription[];

extern const char kEnableSystemEntropyOnPerformanceNavigationTimingName[];
extern const char
    kEnableSystemEntropyOnPerformanceNavigationTimingDescription[];

extern const char kEnableWebAuthenticationChromeOSAuthenticatorName[];
extern const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[];

extern const char kEnableZeroCopyTabCaptureName[];
extern const char kEnableZeroCopyTabCaptureDescription[];

extern const char kExperimentalWebAssemblyFeaturesName[];
extern const char kExperimentalWebAssemblyFeaturesDescription[];

extern const char kExperimentalWebAssemblyJSPIName[];
extern const char kExperimentalWebAssemblyJSPIDescription[];

extern const char kEnableUnrestrictedUsbName[];
extern const char kEnableUnrestrictedUsbDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmGarbageCollectionName[];
extern const char kEnableWasmGarbageCollectionDescription[];

extern const char kEnableWasmLazyCompilationName[];
extern const char kEnableWasmLazyCompilationDescription[];

extern const char kEnableWasmMemory64Name[];
extern const char kEnableWasmMemory64Description[];

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

extern const char kSafeBrowsingAsyncRealTimeCheckName[];
extern const char kSafeBrowsingAsyncRealTimeCheckDescription[];

extern const char kSafeBrowsingHashPrefixRealTimeLookupsName[];
extern const char kSafeBrowsingHashPrefixRealTimeLookupsDescription[];

extern const char kSafeBrowsingLocalListsUseSBv5Name[];
extern const char kSafeBrowsingLocalListsUseSBv5Description[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kEnableWebHidInWebViewName[];
extern const char kEnableWebHidInWebViewDescription[];

extern const char kExtensionAiDataCollectionName[];
extern const char kExtensionAiDataCollectionDescription[];
extern const char kExtensionsMenuAccessControlName[];
extern const char kExtensionsMenuAccessControlDescription[];
extern const char kIPHExtensionsMenuFeatureName[];
extern const char kIPHExtensionsMenuFeatureDescription[];
extern const char kIPHExtensionsRequestAccessButtonFeatureName[];
extern const char kIPHExtensionsRequestAccessButtonFeatureDescription[];
extern const char kExtensionManifestV2DeprecationWarningName[];
extern const char kExtensionManifestV2DeprecationWarningDescription[];
extern const char kExtensionManifestV2DeprecationDisabledName[];
extern const char kExtensionManifestV2DeprecationDisabledDescription[];
extern const char kExtensionManifestV2DeprecationUnsupportedName[];
extern const char kExtensionManifestV2DeprecationUnsupportedDescription[];

extern const char kExtensionsToolbarZeroStateName[];
extern const char kExtensionsToolbarZeroStateDescription[];
extern const char kExtensionsToolbarZeroStateChoicesDisabled[];
extern const char kExtensionsToolbarZeroStateVistWebStore[];
extern const char kExtensionsToolbarZeroStateExploreExtensionsByCategory[];

extern const char kWebAuthFlowInBrowserTabName[];
extern const char kWebAuthFlowInBrowserTabDescription[];

extern const char kCWSInfoFastCheckName[];
extern const char kCWSInfoFastCheckDescription[];

extern const char kExtensionTelemetryForEnterpriseName[];
extern const char kExtensionTelemetryForEnterpriseDescription[];

extern const char kSafetyCheckExtensionsName[];
extern const char kSafetyCheckExtensionsDescription[];

#if BUILDFLAG(IS_CHROMEOS)
extern const char kExtensionWebFileHandlersName[];
extern const char kExtensionWebFileHandlersDescription[];
#endif  // IS_CHROMEOS
#endif  // ENABLE_EXTENSIONS

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFractionalScrollOffsetsName[];
extern const char kFractionalScrollOffsetsDescription[];

extern const char kFedCmAuthzName[];
extern const char kFedCmAuthzDescription[];

extern const char kFedCmButtonModeName[];
extern const char kFedCmButtonModeDescription[];

extern const char kFedCmIdPRegistrationName[];
extern const char kFedCmIdPRegistrationDescription[];

extern const char kFedCmMetricsEndpointName[];
extern const char kFedCmMetricsEndpointDescription[];

extern const char kFedCmMultiIdpName[];
extern const char kFedCmMultiIdpDescription[];

extern const char kFedCmSelectiveDisclosureName[];
extern const char kFedCmSelectiveDisclosureDescription[];

extern const char kFedCmUseOtherAccountName[];
extern const char kFedCmUseOtherAccountDescription[];

extern const char kFedCmWithStorageAccessAPIName[];
extern const char kFedCmWithStorageAccessAPIDescription[];

extern const char kFedCmWithoutWellKnownEnforcementName[];
extern const char kFedCmWithoutWellKnownEnforcementDescription[];

extern const char kWebIdentityDigitalCredentialsName[];
extern const char kWebIdentityDigitalCredentialsDescription[];

extern const char kFileHandlingIconsName[];
extern const char kFileHandlingIconsDescription[];

extern const char kFileSystemAccessPersistentPermissionUpdatedPageInfoName[];
extern const char
    kFileSystemAccessPersistentPermissionUpdatedPageInfoDescription[];

extern const char kFileSystemObserverName[];
extern const char kFileSystemObserverDescription[];

extern const char kDrawImmediatelyWhenInteractiveName[];
extern const char kDrawImmediatelyWhenInteractiveDescription[];

extern const char kAckOnSurfaceActivationWhenInteractiveName[];
extern const char kAckOnSurfaceActivationWhenInteractiveDescription[];

extern const char kFluentOverlayScrollbarsName[];
extern const char kFluentOverlayScrollbarsDescription[];

extern const char kFluentScrollbarsName[];
extern const char kFluentScrollbarsDescription[];

extern const char kMutationEventsName[];
extern const char kMutationEventsDescription[];

extern const char kKeyboardFocusableScrollersName[];
extern const char kKeyboardFocusableScrollersDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kMediaRemotingWithoutFullscreenName[];
extern const char kMediaRemotingWithoutFullscreenDescription[];

extern const char kRemotePlaybackBackendName[];
extern const char kRemotePlaybackBackendDescription[];

#if !BUILDFLAG(IS_CHROMEOS)
extern const char kGlobalMediaControlsUpdatedUIName[];
extern const char kGlobalMediaControlsUpdatedUIDescription[];
#endif  // !BUILDFLAG(IS_CHROMEOS)

extern const char kGoogleOneOfferFilesBannerName[];
extern const char kGoogleOneOfferFilesBannerDescription[];

extern const char kObservableAPIName[];
extern const char kObservableAPIDescription[];

extern const char kOutlineSilhouetteIconName[];
extern const char kOutlineSilhouetteIconDescription[];

extern const char kAtomicMoveAPIName[];
extern const char kAtomicMoveAPIDescription[];

extern const char kCastStreamingAv1Name[];
extern const char kCastStreamingAv1Description[];

extern const char kCastStreamingHardwareH264Name[];
extern const char kCastStreamingHardwareH264Description[];

extern const char kCastStreamingHardwareVp8Name[];
extern const char kCastStreamingHardwareVp8Description[];

extern const char kCastStreamingHardwareVp9Name[];
extern const char kCastStreamingHardwareVp9Description[];

extern const char kCastStreamingPerformanceOverlayName[];
extern const char kCastStreamingPerformanceOverlayDescription[];

extern const char kCastStreamingVp8Name[];
extern const char kCastStreamingVp8Description[];

extern const char kCastStreamingVp9Name[];
extern const char kCastStreamingVp9Description[];

#if BUILDFLAG(IS_MAC)
extern const char kCastStreamingMacHardwareH264Name[];
extern const char kCastStreamingMacHardwareH264Description[];
extern const char kUseNetworkFrameworkForLocalDiscoveryName[];
extern const char kUseNetworkFrameworkForLocalDiscoveryDescription[];
#endif

#if BUILDFLAG(IS_WIN)
extern const char kCastStreamingWinHardwareH264Name[];
extern const char kCastStreamingWinHardwareH264Description[];
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kFlexFirmwareUpdateName[];
extern const char kFlexFirmwareUpdateDescription[];
#endif

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHeavyAdPrivacyMitigationsName[];
extern const char kHeavyAdPrivacyMitigationsDescription[];

extern const char kTabAudioMutingName[];
extern const char kTabAudioMutingDescription[];

extern const char kCrasProcessorDedicatedThreadName[];
extern const char kCrasProcessorDedicatedThreadDescription[];

extern const char kCrasProcessorWavDumpName[];
extern const char kCrasProcessorWavDumpDescription[];

extern const char kPwaRestoreBackendName[];
extern const char kPwaRestoreBackendDescription[];

extern const char kPwaRestoreUiName[];
extern const char kPwaRestoreUiDescription[];

extern const char kPwaRestoreUiAtStartupName[];
extern const char kPwaRestoreUiAtStartupDescription[];

extern const char kStartSurfaceReturnTimeName[];
extern const char kStartSurfaceReturnTimeDescription[];

extern const char kHideIncognitoMediaMetadataName[];
extern const char kHideIncognitoMediaMetadataDescription[];

extern const char kHistoryEmbeddingsName[];
extern const char kHistoryEmbeddingsDescription[];

extern const char kHistoryEmbeddingsAnswersName[];
extern const char kHistoryEmbeddingsAnswersDescription[];

extern const char kHttpsFirstBalancedModeName[];
extern const char kHttpsFirstBalancedModeDescription[];

extern const char kHttpsFirstDialogUiName[];
extern const char kHttpsFirstDialogUiDescription[];

extern const char kHttpsFirstModeInterstitialAugust2024RefreshName[];
extern const char kHttpsFirstModeInterstitialAugust2024RefreshDescription[];

extern const char kHttpsFirstModeIncognitoName[];
extern const char kHttpsFirstModeIncognitoDescription[];

extern const char kHttpsFirstModeIncognitoNewSettingsName[];
extern const char kHttpsFirstModeIncognitoNewSettingsDescription[];

extern const char kHttpsFirstModeV2Name[];
extern const char kHttpsFirstModeV2Description[];

extern const char kHttpsFirstModeV2ForEngagedSitesName[];
extern const char kHttpsFirstModeV2ForEngagedSitesDescription[];

extern const char kHttpsFirstModeForTypicallySecureUsersName[];
extern const char kHttpsFirstModeForTypicallySecureUsersDescription[];

extern const char kHttpsUpgradesName[];
extern const char kHttpsUpgradesDescription[];

extern const char kIgnoreGpuBlocklistName[];
extern const char kIgnoreGpuBlocklistDescription[];

extern const char kImprovedKeyboardShortcutsName[];
extern const char kImprovedKeyboardShortcutsDescription[];

extern const char kIncrementLocalSurfaceIdForMainframeSameDocNavigationName[];
extern const char
    kIncrementLocalSurfaceIdForMainframeSameDocNavigationDescription[];

extern const char kIncognitoReauthenticationForAndroidName[];
extern const char kIncognitoReauthenticationForAndroidDescription[];

extern const char kIncognitoScreenshotName[];
extern const char kIncognitoScreenshotDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kInProductHelpSnoozeName[];
extern const char kInProductHelpSnoozeDescription[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kUserEducationExperienceVersion2Name[];
extern const char kUserEducationExperienceVersion2Description[];
#endif

extern const char kInstallIsolatedWebAppFromUrl[];
extern const char kInstallIsolatedWebAppFromUrlDescription[];

extern const char kInstantHotspotRebrandName[];
extern const char kInstantHotspotRebrandDescription[];

extern const char kInstantHotspotOnNearbyName[];
extern const char kInstantHotspotOnNearbyDescription[];

extern const char kIndexedDBDefaultDurabilityRelaxed[];
extern const char kIndexedDBDefaultDurabilityRelaxedDescription[];

extern const char kIpProtectionProxyOptOutName[];
extern const char kIpProtectionProxyOptOutDescription[];
extern const char kIpProtectionProxyOptOutChoiceDefault[];
extern const char kIpProtectionProxyOptOutChoiceOptOut[];

extern const char kAutomaticFullscreenContentSettingName[];
extern const char kAutomaticFullscreenContentSettingDescription[];

extern const char kJapaneseOSSettingsName[];
extern const char kJapaneseOSSettingsDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kJourneysName[];
extern const char kJourneysDescription[];

extern const char kJourneysShowAllClustersName[];
extern const char kJourneysShowAllClustersDescription[];

extern const char kJourneysZeroStateFilteringName[];
extern const char kJourneysZeroStateFilteringDescription[];

extern const char kJumpStartOmniboxName[];
extern const char kJumpStartOmniboxDescription[];

extern const char kExtractRelatedSearchesFromPrefetchedZPSResponseName[];
extern const char kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[];

extern const char kLacrosExtensionPrintingName[];
extern const char kLacrosExtensionPrintingDescription[];

extern const char kLanguageDetectionAPIName[];
extern const char kLanguageDetectionAPIDescription[];

extern const char kLegacyTechReportEnableCookieIssueReportsName[];
extern const char kLegacyTechReportEnableCookieIssueReportsDescription[];

extern const char kLegacyTechReportTopLevelUrlName[];
extern const char kLegacyTechReportTopLevelUrlDescription[];

extern const char kLensOverlayName[];
extern const char kLensOverlayDescription[];

extern const char kLensOverlayContextualSearchboxName[];
extern const char kLensOverlayContextualSearchboxDescription[];

extern const char kLensOverlayTranslateButtonName[];
extern const char kLensOverlayTranslateButtonDescription[];

extern const char kLensImageFormatOptimizationsName[];
extern const char kLensImageFormatOptimizationsDescription[];

extern const char kLensImageTranslateName[];
extern const char kLensImageTranslateDescription[];

extern const char kLinkedServicesSettingName[];
extern const char kLinkedServicesSettingDescription[];

extern const char kLensOnQuickActionSearchWidgetName[];
extern const char kLensOnQuickActionSearchWidgetDescription[];

extern const char kLocationBarModelOptimizationsName[];
extern const char kLocationBarModelOptimizationsDescription[];

extern const char kLogJsConsoleMessagesName[];
extern const char kLogJsConsoleMessagesDescription[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMojoLinuxChannelSharedMemName[];
extern const char kMojoLinuxChannelSharedMemDescription[];

extern const char kMostVisitedTilesNewScoringName[];
extern const char kMostVisitedTilesNewScoringDescription[];

extern const char kMostVisitedTilesReselectName[];
extern const char kMostVisitedTilesReselectDescription[];

extern const char kUsernameFirstFlowFallbackCrowdsourcingName[];
extern const char kUsernameFirstFlowFallbackCrowdsourcingDescription[];

extern const char kUsernameFirstFlowWithIntermediateValuesPredictionsName[];
extern const char
    kUsernameFirstFlowWithIntermediateValuesPredictionsDescription[];

extern const char kUsernameFirstFlowWithIntermediateValuesVotingName[];
extern const char kUsernameFirstFlowWithIntermediateValuesVotingDescription[];

extern const char kCanvas2DLayersName[];
extern const char kCanvas2DLayersDescription[];

extern const char kWebMachineLearningNeuralNetworkName[];
extern const char kWebMachineLearningNeuralNetworkDescription[];

extern const char kExperimentalWebMachineLearningNeuralNetworkName[];
extern const char kExperimentalWebMachineLearningNeuralNetworkDescription[];

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

extern const char kOmniboxAnswerActionsName[];
extern const char kOmniboxAnswerActionsDescription[];

extern const char kOmniboxAsyncViewInflationName[];
extern const char kOmniboxAsyncViewInflationDescription[];

extern const char kOmniboxCalcProviderName[];
extern const char kOmniboxCalcProviderDescription[];

extern const char kOmniboxConsumesImeInsetsName[];
extern const char kOmniboxConsumesImeInsetsDescription[];

extern const char kOmniboxDomainSuggestionsName[];
extern const char kOmniboxDomainSuggestionsDescription[];

extern const char kOmniboxForceAllowedToBeDefaultName[];
extern const char kOmniboxForceAllowedToBeDefaultDescription[];

extern const char kOmniboxGroupingFrameworkNonZPSName[];
extern const char kOmniboxGroupingFrameworkZPSName[];
extern const char kOmniboxGroupingFrameworkDescription[];

extern const char kOmniboxMlLogUrlScoringSignalsName[];
extern const char kOmniboxMlLogUrlScoringSignalsDescription[];

extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[];
extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[];

extern const char kOmniboxMlUrlScoreCachingName[];
extern const char kOmniboxMlUrlScoreCachingDescription[];

extern const char kOmniboxMlUrlScoringName[];
extern const char kOmniboxMlUrlScoringDescription[];

extern const char kOmniboxMlUrlScoringModelName[];
extern const char kOmniboxMlUrlScoringModelDescription[];

extern const char kOmniboxMlUrlSearchBlendingName[];
extern const char kOmniboxMlUrlSearchBlendingDescription[];

extern const char kOmniboxMostVisitedTilesHorizontalRenderGroupName[];
extern const char kOmniboxMostVisitedTilesHorizontalRenderGroupDescription[];

extern const char kOmniboxMostVisitedTilesTitleWrapAroundName[];
extern const char kOmniboxMostVisitedTilesTitleWrapAroundDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsName[];
extern const char kOmniboxOnDeviceHeadSuggestionsDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

extern const char kOmniboxOnDeviceTailSuggestionsName[];
extern const char kOmniboxOnDeviceTailSuggestionsDescription[];

extern const char kOmniboxPrefBasedDataCollectionConsentHelperName[];
extern const char kOmniboxPrefBasedDataCollectionConsentHelperDescription[];

extern const char kOmniboxSuppressClipboardSuggestionAfterFirstUsedName[];
extern const char
    kOmniboxSuppressClipboardSuggestionAfterFirstUsedDescription[];

extern const char kOmniboxRichAutocompletionPromisingName[];
extern const char kOmniboxRichAutocompletionPromisingDescription[];

extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[];
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[];

extern const char kOmniboxSuggestionAnswerMigrationName[];
extern const char kOmniboxSuggestionAnswerMigrationDescription[];

extern const char kOmniboxShortcutBoostName[];
extern const char kOmniboxShortcutBoostDescription[];

extern const char kOmniboxShortcutExpandingName[];
extern const char kOmniboxShortcutExpandingDescription[];

extern const char kOmniboxStarterPackExpansionName[];
extern const char kOmniboxStarterPackExpansionDescription[];

extern const char kOmniboxStarterPackIPHName[];
extern const char kOmniboxStarterPackIPHDescription[];

extern const char kOmniboxZeroSuggestPrefetchDebouncingName[];
extern const char kOmniboxZeroSuggestPrefetchDebouncingDescription[];

extern const char kOmniboxZeroSuggestPrefetchingName[];
extern const char kOmniboxZeroSuggestPrefetchingDescription[];

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

#if BUILDFLAG(IS_ANDROID)
extern const char kOmnibox2023RefreshConnectionSecurityIndicatorsName[];
extern const char kOmnibox2023RefreshConnectionSecurityIndicatorsDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kWebUIOmniboxPopupName[];
extern const char kWebUIOmniboxPopupDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxDynamicMaxAutocompleteName[];
extern const char kOmniboxDynamicMaxAutocompleteDescription[];

extern const char kOneTimePermissionName[];
extern const char kOneTimePermissionDescription[];

extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

extern const char kOptimizationGuideModelExecutionName[];
extern const char kOptimizationGuideModelExecutionDescription[];

extern const char kOptimizationGuideEnableDogfoodLoggingName[];
extern const char kOptimizationGuideEnableDogfoodLoggingDescription[];

extern const char kOptimizationGuideOnDeviceModelName[];
extern const char kOptimizationGuideOnDeviceModelDescription[];

extern const char kOptimizationGuidePersonalizedFetchingName[];
extern const char kOptimizationGuidePersonalizedFetchingDescription[];

extern const char kOptimizationGuidePushNotificationName[];
extern const char kOptimizationGuidePushNotificationDescription[];

extern const char kOrcaKeyName[];
extern const char kOrcaKeyDescription[];

extern const char kOsFeedbackDialogName[];
extern const char kOsFeedbackDialogDescription[];

extern const char kOsSettingsAppNotificationsPageName[];
extern const char kOsSettingsAppNotificationsPageDescription[];

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

extern const char kPageImageServiceOptimizationGuideSalientImagesName[];
extern const char kPageImageServiceOptimizationGuideSalientImagesDescription[];

extern const char kPageImageServiceSuggestPoweredImagesName[];
extern const char kPageImageServiceSuggestPoweredImagesDescription[];

extern const char kPageInfoAboutThisPagePersistentEntryName[];
extern const char kPageInfoAboutThisPagePersistentEntryDescription[];

extern const char kPageInfoCookiesSubpageName[];
extern const char kPageInfoCookiesSubpageDescription[];

extern const char kPageInfoHideSiteSettingsName[];
extern const char kPageInfoHideSiteSettingsDescription[];

extern const char kPageInfoHistoryDesktopName[];
extern const char kPageInfoHistoryDesktopDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPasswordGenerationSoftNudgeName[];
extern const char kPasswordGenerationSoftNudgeDescription[];

extern const char kPasswordLeakToggleMoveName[];
extern const char kPasswordLeakToggleMoveDescription[];

extern const char kPasswordManagerShowSuggestionsOnAutofocusName[];
extern const char kPasswordManagerShowSuggestionsOnAutofocusDescription[];

extern const char kPasswordManualFallbackAvailableName[];
extern const char kPasswordManualFallbackAvailableDescription[];

extern const char kPasswordParsingOnSaveUsesPredictionsName[];
extern const char kPasswordParsingOnSaveUsesPredictionsDescription[];

extern const char kPdfOcrName[];
extern const char kPdfOcrDescription[];

extern const char kPdfSearchifyName[];
extern const char kPdfSearchifyDescription[];

extern const char kPdfXfaFormsName[];
extern const char kPdfXfaFormsDescription[];

extern const char kAutoWebContentsDarkModeName[];
extern const char kAutoWebContentsDarkModeDescription[];

extern const char kForcedColorsName[];
extern const char kForcedColorsDescription[];

extern const char kWindowsScrollingPersonalityName[];
extern const char kWindowsScrollingPersonalityDescription[];

extern const char kImprovedSemanticsActivityIndicatorsName[];
extern const char kImprovedSemanticsActivityIndicatorsDescription[];

extern const char kLeftHandSideActivityIndicatorsName[];
extern const char kLeftHandSideActivityIndicatorsDescription[];

#if BUILDFLAG(IS_CHROMEOS)
extern const char kCrosSystemLevelPermissionBlockedWarningsName[];
extern const char kCrosSystemLevelPermissionBlockedWarningsDescription[];
#endif

extern const char kPerformanceInterventionUiName[];
extern const char kPerformanceInterventionUiDescription[];

extern const char kPerformanceInterventionDemoModeName[];
extern const char kPerformanceInterventionDemoModeDescription[];

extern const char kPermissionPredictionsName[];
extern const char kPermissionPredictionsDescription[];

extern const char kPermissionPredictionsV3Name[];
extern const char kPermissionPredictionsV3Description[];

extern const char kPermissionQuietChipName[];
extern const char kPermissionQuietChipDescription[];

extern const char kShowRelatedWebsiteSetsPermissionGrantsName[];
extern const char kShowRelatedWebsiteSetsPermissionGrantsDescription[];

extern const char kPowerBookmarkBackendName[];
extern const char kPowerBookmarkBackendDescription[];

extern const char kSpeculationRulesPrerenderingTargetHintName[];
extern const char kSpeculationRulesPrerenderingTargetHintDescription[];

extern const char kSubframeProcessReuseThresholds[];
extern const char kSubframeProcessReuseThresholdsDescription[];

extern const char kSupportSearchSuggestionForPrerender2Name[];
extern const char kSupportSearchSuggestionForPrerender2Description[];

extern const char kPrerender2EarlyDocumentLifecycleUpdateName[];
extern const char kPrerender2EarlyDocumentLifecycleUpdateDescription[];

extern const char kWarmUpCompositorName[];
extern const char kWarmUpCompositorDescription[];

extern const char kPrerender2WarmUpCompositorName[];
extern const char kPrerender2WarmUpCompositorDescription[];

extern const char kPrerender2ForNewTabPageAndroidName[];
extern const char kPrerender2ForNewTabPageAndroidDescription[];

extern const char kEnableOmniboxSearchPrefetchName[];
extern const char kEnableOmniboxSearchPrefetchDescription[];

extern const char kEnableOmniboxClientSearchPrefetchName[];
extern const char kEnableOmniboxClientSearchPrefetchDescription[];

extern const char kPreloadingOnPerformancePageName[];
extern const char kPreloadingOnPerformancePageDescription[];

extern const char kPrerender2Name[];
extern const char kPrerender2Description[];

extern const char kCbdTimeframeRequiredName[];
extern const char kCbdTimeframeRequiredDescription[];

extern const char kPriceChangeModuleName[];
extern const char kPriceChangeModuleDescription[];

extern const char kPrivacyGuideAndroid3Name[];
extern const char kPrivacyGuideAndroid3Description[];

extern const char kPrivacyGuidePreloadAndroidName[];
extern const char kPrivacyGuidePreloadAndroidDescription[];

extern const char kPrivacySandboxAdsAPIsOverrideName[];
extern const char kPrivacySandboxAdsAPIsOverrideDescription[];

extern const char kPrivacySandboxEnrollmentOverridesName[];
extern const char kPrivacySandboxEnrollmentOverridesDescription[];

extern const char kPrivacySandboxInternalsName[];
extern const char kPrivacySandboxInternalsDescription[];

extern const char kPrivacySandboxPrivacyGuideAdTopicsName[];
extern const char kPrivacySandboxPrivacyGuideAdTopicsDescription[];

extern const char kPrivacySandboxPrivacyPolicyName[];
extern const char kPrivacySandboxPrivacyPolicyDescription[];

extern const char kPrivateStateTokensDevUIName[];
extern const char kPrivateStateTokensDevUIDescription[];

extern const char kProtectedAudiencesConsentedDebugTokenName[];
extern const char kProtectedAudiencesConsentedDebugTokenDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kPwaUpdateDialogForAppIconName[];
extern const char kPwaUpdateDialogForAppIconDescription[];

extern const char kRenderDocumentName[];
extern const char kRenderDocumentDescription[];

extern const char kRelatedWebsiteSetsDevUIName[];
extern const char kRelatedWebsiteSetsDevUIDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kQuickAppAccessTestUIName[];
extern const char kQuickAppAccessTestUIDescription[];

extern const char kQuickDimName[];
extern const char kQuickDimDescription[];

extern const char kQuickDeleteForAndroidName[];
extern const char kQuickDeleteForAndroidDescription[];

extern const char kQuickDeleteAndroidFollowupName[];
extern const char kQuickDeleteAndroidFollowupDescription[];

extern const char kQuickDeleteAndroidSurveyName[];
extern const char kQuickDeleteAndroidSurveyDescription[];

extern const char kQuickShareV2Name[];
extern const char kQuickShareV2Description[];

extern const char kSendTabToSelfIOSPushNotificationsName[];
extern const char kSendTabToSelfIOSPushNotificationsDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kSensitiveContentName[];
extern const char kSensitiveContentDescription[];

extern const char kSensitiveContentWhileSwitchingTabsName[];
extern const char kSensitiveContentWhileSwitchingTabsDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kSettingsAppNotificationSettingsName[];
extern const char kSettingsAppNotificationSettingsDescription[];

extern const char kReadLaterFlagId[];
extern const char kReadLaterName[];
extern const char kReadLaterDescription[];

extern const char kRecordWebAppDebugInfoName[];
extern const char kRecordWebAppDebugInfoDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kRecordPermissionExpirationTimestampsName[];
extern const char kRecordPermissionExpirationTimestampsDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
extern const char kReduceIPAddressChangeNotificationName[];
extern const char kReduceIPAddressChangeNotificationDescription[];
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
extern const char kReplaceSyncPromosWithSignInPromosName[];
extern const char kReplaceSyncPromosWithSignInPromosDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kRestrictGamepadAccessName[];
extern const char kRestrictGamepadAccessDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kRetainOmniboxOnFocusName[];
extern const char kRetainOmniboxOnFocusDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kRoundedDisplay[];
extern const char kRoundedDisplayDescription[];

extern const char kRoundedWindows[];
extern const char kRoundedWindowsDescription[];

extern const char kRubyShortHeuristicsName[];
extern const char kRubyShortHeuristicsDescription[];

extern const char kMBIModeName[];
extern const char kMBIModeDescription[];

extern const char kSafetyCheckUnusedSitePermissionsName[];
extern const char kSafetyCheckUnusedSitePermissionsDescription[];

extern const char kSafetyHubName[];
extern const char kSafetyHubDescription[];

#if BUILDFLAG(IS_ANDROID)
extern const char kSafetyHubMagicStackName[];
extern const char kSafetyHubMagicStackDescription[];

extern const char kSafetyHubFollowupName[];
extern const char kSafetyHubFollowupDescription[];

extern const char kSafetyHubAndroidSurveyName[];
extern const char kSafetyHubAndroidSurveyDescription[];
#else
extern const char kSafetyHubHaTSOneOffSurveyName[];
extern const char kSafetyHubHaTSOneOffSurveyDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kSafetyHubAbusiveNotificationRevocationName[];
extern const char kSafetyHubAbusiveNotificationRevocationDescription[];

extern const char kSameAppWindowCycleName[];
extern const char kSameAppWindowCycleDescription[];

extern const char kTestThirdPartyCookiePhaseoutName[];
extern const char kTestThirdPartyCookiePhaseoutDescription[];

extern const char kScrollableTabStripFlagId[];
extern const char kScrollableTabStripName[];
extern const char kScrollableTabStripDescription[];

extern const char kTabStripCollectionStorageFlagId[];
extern const char kTabStripCollectionStorageName[];
extern const char kTabStripCollectionStorageDescription[];

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

extern const char kDynamicSearchUpdateAnimationName[];
extern const char kDynamicSearchUpdateAnimationDescription[];

extern const char kSecurePaymentConfirmationDebugName[];
extern const char kSecurePaymentConfirmationDebugDescription[];

extern const char kSecurePaymentConfirmationNetworkAndIssuerIconsName[];
extern const char kSecurePaymentConfirmationNetworkAndIssuerIconsDescription[];

extern const char kSegmentationSurveyPageName[];
extern const char kSegmentationSurveyPageDescription[];

extern const char kSidePanelJourneysFlagId[];
extern const char kSidePanelJourneysName[];
extern const char kSidePanelJourneysDescription[];

extern const char kSidePanelJourneysQuerylessFlagId[];
extern const char kSidePanelJourneysQuerylessName[];
extern const char kSidePanelJourneysQuerylessDescription[];

extern const char kSharedZstdName[];
extern const char kSharedZstdDescription[];

extern const char kSharingDesktopScreenshotsName[];
extern const char kSharingDesktopScreenshotsDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kShowFeaturedEnterpriseSiteSearchName[];
extern const char kShowFeaturedEnterpriseSiteSearchDescription[];

extern const char kShowFeaturedEnterpriseSiteSearchIPHName[];
extern const char kShowFeaturedEnterpriseSiteSearchIPHDescription[];
#endif

extern const char kShowPerformanceMetricsHudName[];
extern const char kShowPerformanceMetricsHudDescription[];

extern const char kShowOverdrawFeedbackName[];
extern const char kShowOverdrawFeedbackDescription[];

extern const char kSidePanelResizingFlagId[];
extern const char kSidePanelResizingName[];
extern const char kSidePanelResizingDescription[];

extern const char kSiteInstanceGroupsForDataUrlsName[];
extern const char kSiteInstanceGroupsForDataUrlsDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kSiteSearchSettingsPolicyName[];
extern const char kSiteSearchSettingsPolicyDescription[];

extern const char kPwaNavigationCapturingName[];
extern const char kPwaNavigationCapturingDescription[];
#endif

extern const char kSkiaRendererName[];
extern const char kSkiaRendererDescription[];

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

extern const char kBackdropFilterMirrorEdgeName[];
extern const char kBackdropFilterMirrorEdgeDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSplitCacheByNetworkIsolationKeyName[];
extern const char kSplitCacheByNetworkIsolationKeyDescription[];

extern const char kStorageAccessHeadersName[];
extern const char kStorageAccessHeadersDescription[];

extern const char kStrictOriginIsolationName[];
extern const char kStrictOriginIsolationDescription[];

extern const char kSupportTool[];
extern const char kSupportToolDescription[];

extern const char kSupportToolScreenshot[];
extern const char kSupportToolScreenshotDescription[];

extern const char kSuppressToolbarCapturesName[];
extern const char kSuppressToolbarCapturesDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSyncAutofillWalletCredentialDataName[];
extern const char kSyncAutofillWalletCredentialDataDescription[];

extern const char kSyncAutofillWalletUsageDataName[];
extern const char kSyncAutofillWalletUsageDataDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncTrustedVaultPassphrasePromoName[];
extern const char kSyncTrustedVaultPassphrasePromoDescription[];

extern const char kSystemProxyForSystemServicesName[];
extern const char kSystemProxyForSystemServicesDescription[];

extern const char kSystemShortcutBehaviorName[];
extern const char kSystemShortcutBehaviorDescription[];

extern const char kTabDragDropName[];
extern const char kTabDragDropDescription[];

extern const char kToolbarPhoneCleanupName[];
extern const char kToolbarPhoneCleanupDescription[];

extern const char kCommerceDeveloperName[];
extern const char kCommerceDeveloperDescription[];

extern const char kTabGroupsSaveV2Id[];
extern const char kTabGroupsSaveV2Name[];
extern const char kTabGroupsSaveV2Description[];

extern const char kCompactModeId[];
extern const char kCompactModeName[];
extern const char kCompactModeDescription[];

extern const char kTabGroupsSaveUIUpdateId[];
extern const char kTabGroupsSaveUIUpdateName[];
extern const char kTabGroupsSaveUIUpdateDescription[];

extern const char kTabGroupSyncServiceDesktopMigrationId[];
extern const char kTabGroupSyncServiceDesktopMigrationName[];
extern const char kTabGroupSyncServiceDesktopMigrationDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabSearchFuzzySearchName[];
extern const char kTabSearchFuzzySearchDescription[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kTabSearchPositionSettingId[];
extern const char kTabSearchPositionSettingName[];
extern const char kTabSearchPositionSettingDescription[];
#endif

extern const char kTearOffWebAppAppTabOpensWebAppWindowName[];
extern const char kTearOffWebAppAppTabOpensWebAppWindowDescription[];

extern const char kTabletToolbarReorderingAndroidName[];
extern const char kTabletToolbarReorderingAndroidDescription[];

extern const char kTabResumptionModuleAndroidName[];
extern const char kTabResumptionModuleAndroidDescription[];

extern const char kTabStripStartupRefactoringName[];
extern const char kTabStripStartupRefactoringDescription[];

extern const char kDelayTempStripRemovalName[];
extern const char kDelayTempStripRemovalDescription[];

extern const char kTextBasedAudioDescriptionName[];
extern const char kTextBasedAudioDescriptionDescription[];

extern const char kTextInShelfName[];
extern const char kTextInShelfDescription[];

extern const char kThreadedScrollPreventRenderingStarvationName[];
extern const char kThreadedScrollPreventRenderingStarvationDescription[];

extern const char kTintCompositedContentName[];
extern const char kTintCompositedContentDescription[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kToolbarPinningFlagId[];
extern const char kToolbarPinningName[];
extern const char kToolbarPinningDescription[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kTopChromeToastsName[];
extern const char kTopChromeToastsDescription[];
#endif

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kTouchTextEditingRedesignName[];
extern const char kTouchTextEditingRedesignDescription[];

extern const char kTranslateForceTriggerOnEnglishName[];
extern const char kTranslateForceTriggerOnEnglishDescription[];

extern const char kTreatInsecureOriginAsSecureName[];
extern const char kTreatInsecureOriginAsSecureDescription[];

extern const char kTPCPhaseOutFacilitatedTestingName[];
extern const char kTPCPhaseOutFacilitatedTestingDescription[];

extern const char kTpcdHeuristicsGrantsName[];
extern const char kTpcdHeuristicsGrantsDescription[];

extern const char kTpcdMetadataGrantsName[];
extern const char kTpcdMetadataGrantsDescription[];

extern const char kTpcdTrialSettingsName[];
extern const char kTpcdTrialSettingsDescription[];

extern const char kTopLevelTpcdTrialSettingsName[];
extern const char kTopLevelTpcdTrialSettingsDescription[];

extern const char kTrackingProtection3pcdName[];
extern const char kTrackingProtection3pcdDescription[];

extern const char kUnifiedPasswordManagerAndroidReenrollmentName[];
extern const char kUnifiedPasswordManagerAndroidReenrollmentDescription[];

extern const char kUnsafeWebGPUName[];
extern const char kUnsafeWebGPUDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUIEnableSharedImageCacheForGpuName[];
extern const char kUIEnableSharedImageCacheForGpuDescription[];

extern const char kUserNotesSidePanelName[];
extern const char kUserNotesSidePanelDescription[];

extern const char kUseSearchClickForRightClickName[];
extern const char kUseSearchClickForRightClickDescription[];

extern const char kVcBackgroundReplaceName[];
extern const char kVcBackgroundReplaceDescription[];

extern const char kVcRelightingInferenceBackendName[];
extern const char kVcRelightingInferenceBackendDescription[];

extern const char kVcSegmentationInferenceBackendName[];
extern const char kVcSegmentationInferenceBackendDescription[];

extern const char kVcSegmentationModelName[];
extern const char kVcSegmentationModelDescription[];

extern const char kVcStudioLookName[];
extern const char kVcStudioLookDescription[];

extern const char kVcTrayMicIndicatorName[];
extern const char kVcTrayMicIndicatorDescription[];

extern const char kVcTrayTitleHeaderName[];
extern const char kVcTrayTitleHeaderDescription[];

extern const char kVcLightIntensityName[];
extern const char kVcLightIntensityDescription[];

extern const char kVcWebApiName[];
extern const char kVcWebApiDescription[];

extern const char kVideoPictureInPictureControlsUpdate2024Name[];
extern const char kVideoPictureInPictureControlsUpdate2024Description[];

extern const char kViewportSegmentsName[];
extern const char kViewportSegmentsDescription[];

extern const char kVisitedURLRankingServiceHistoryVisibilityScoreFilterName[];
extern const char
    kVisitedURLRankingServiceHistoryVisibilityScoreFilterDescription[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kGlobalVaapiLockName[];
extern const char kGlobalVaapiLockDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kTaskManagerEndProcessDisabledForExtensionName[];
extern const char kTaskManagerEndProcessDisabledForExtensionDescription[];

extern const char kWallpaperFastRefreshName[];
extern const char kWallpaperFastRefreshDescription[];

extern const char kWallpaperGooglePhotosSharedAlbumsName[];
extern const char kWallpaperGooglePhotosSharedAlbumsDescription[];

extern const char kWallpaperSearchSettingsVisibilityName[];
extern const char kWallpaperSearchSettingsVisibilityDescription[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kWebAuthnEnclaveAuthenticatorName[];
extern const char kWebAuthnEnclaveAuthenticatorDescription[];
#endif

extern const char kWebBluetoothName[];
extern const char kWebBluetoothDescription[];

extern const char kWebBluetoothNewPermissionsBackendName[];
extern const char kWebBluetoothNewPermissionsBackendDescription[];

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

extern const char kWebTransportDeveloperModeName[];
extern const char kWebTransportDeveloperModeDescription[];

extern const char kWebUsbDeviceDetectionName[];
extern const char kWebUsbDeviceDetectionDescription[];

extern const char kWebXrForceRuntimeName[];
extern const char kWebXrForceRuntimeDescription[];

extern const char kWebXrRuntimeChoiceNone[];
extern const char kWebXrRuntimeChoiceArCore[];
extern const char kWebXrRuntimeChoiceCardboard[];
extern const char kWebXrRuntimeChoiceOpenXR[];
extern const char kWebXrRuntimeChoiceOrientationSensors[];

extern const char kWebXrHandInputName[];
extern const char kWebXrHandInputDescription[];

extern const char kWebXrIncubationsName[];
extern const char kWebXrIncubationsDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kZstdContentEncodingName[];
extern const char kZstdContentEncodingDescription[];

extern const char kEnableVulkanName[];
extern const char kEnableVulkanDescription[];

extern const char kDefaultAngleVulkanName[];
extern const char kDefaultAngleVulkanDescription[];

extern const char kVulkanFromAngleName[];
extern const char kVulkanFromAngleDescription[];

extern const char kResponsiveToolbarName[];
extern const char kResponsiveToolbarDescription[];

extern const char kSharedHighlightingManagerName[];
extern const char kSharedHighlightingManagerDescription[];

extern const char kSanitizerApiName[];
extern const char kSanitizerApiDescription[];

extern const char kUsePassthroughCommandDecoderName[];
extern const char kUsePassthroughCommandDecoderDescription[];

extern const char kReduceAcceptLanguageName[];
extern const char kReduceAcceptLanguageDescription[];

extern const char kReduceTransferSizeUpdatedIPCName[];
extern const char kReduceTransferSizeUpdatedIPCDescription[];

extern const char kResetAudioSelectionImprovementPrefName[];
extern const char kResetAudioSelectionImprovementPrefDescription[];

extern const char kResetShortcutCustomizationsName[];
extern const char kResetShortcutCustomizationsDescription[];

extern const char kWebSQLAccessName[];
extern const char kWebSQLAccessDescription[];

extern const char kUseIDNA2008NonTransitionalName[];
extern const char kUseIDNA2008NonTransitionalDescription[];

extern const char kEnablePasswordSharingName[];
extern const char kEnablePasswordSharingDescription[];

extern const char kEnableMantaServiceName[];
extern const char kEnableMantaServiceDescription[];

extern const char kPromptAPIForGeminiNanoName[];
extern const char kPromptAPIForGeminiNanoDescription[];
extern const char* const kAIAPIsForGeminiNanoLinks[2];

extern const char kSummarizationAPIForGeminiNanoName[];
extern const char kSummarizationAPIForGeminiNanoDescription[];

extern const char kWriterAPIForGeminiNanoName[];
extern const char kWriterAPIForGeminiNanoDescription[];

extern const char kRewriterAPIForGeminiNanoName[];
extern const char kRewriterAPIForGeminiNanoDescription[];

// Android --------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

extern const char kAccessibilityIncludeLongClickActionName[];
extern const char kAccessibilityIncludeLongClickActionDescription[];
extern const char kAccessibilityPageZoomName[];
extern const char kAccessibilityPageZoomDescription[];
extern const char kAccessibilityPageZoomEnhancementsName[];
extern const char kAccessibilityPageZoomEnhancementsDescription[];
extern const char kAccessibilitySnapshotStressTestsName[];
extern const char kAccessibilitySnapshotStressTestsDescription[];
extern const char kAccessibilityUnifiedSnapshotsName[];
extern const char kAccessibilityUnifiedSnapshotsDescription[];
extern const char kAccessibilityManageBroadcastReceiverOnBackgroundName[];
extern const char
    kAccessibilityManageBroadcastReceiverOnBackgroundDescription[];

extern const char kAccountBookmarksAndReadingListBehindOptInName[];
extern const char kAccountBookmarksAndReadingListBehindOptInDescription[];

extern const char kAdaptiveButtonInTopToolbarCustomizationName[];
extern const char kAdaptiveButtonInTopToolbarCustomizationDescription[];
extern const char kAdaptiveButtonInTopToolbarPageSummaryName[];
extern const char kAdaptiveButtonInTopToolbarPageSummaryDescription[];

extern const char kAndroidSurfaceControlName[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAnimatedImageDragShadowName[];
extern const char kAnimatedImageDragShadowDescription[];

extern const char kAnimateSuggestionsListAppearanceName[];
extern const char kAnimateSuggestionsListAppearanceDescription[];

extern const char kAndroidElegantTextHeightName[];
extern const char kAndroidElegantTextHeightDescription[];

extern const char kAndroidGoogleSansTextName[];
extern const char kAndroidGoogleSansTextDescription[];

extern const char kAndroidHubFloatingActionButtonName[];
extern const char kAndroidHubFloatingActionButtonDescription[];

extern const char kAndroidHubSearchName[];
extern const char kAndroidHubSearchDescription[];

extern const char kAndroidHubV2Name[];
extern const char kAndroidHubV2Description[];

extern const char kAndroidOpenPdfInlineName[];
extern const char kAndroidOpenPdfInlineDescription[];

extern const char kChromePageInfoSharingName[];
extern const char kChromePageInfoSharingDescription[];

extern const char kAndroidTabDeclutterName[];
extern const char kAndroidTabDeclutterDescription[];

extern const char kAndroidTabDeclutterArchiveAllButActiveTabName[];
extern const char kAndroidTabDeclutterArchiveAllButActiveTabDescription[];

extern const char kAppSpecificHistoryName[];
extern const char kAppSpecificHistoryDescription[];

extern const char kBackGestureActivityTabProviderName[];
extern const char kBackGestureActivityTabProviderDescription[];

extern const char kBackGestureRefactorAndroidName[];
extern const char kBackGestureRefactorAndroidDescription[];

extern const char kBackToHomeAnimationName[];
extern const char kBackToHomeAnimationDescription[];

extern const char kBoardingPassDetectorName[];
extern const char kBoardingPassDetectorDescription[];

extern const char kCCTAuthTabName[];
extern const char kCCTAuthTabDescription[];

extern const char kCCTAuthTabDisableAllExternalIntentsName[];
extern const char kCCTAuthTabDisableAllExternalIntentsDescription[];

extern const char kCCTBeforeUnloadName[];
extern const char kCCTBeforeUnloadDescription[];

extern const char kCCTEphemeralModeName[];
extern const char kCCTEphemeralModeDescription[];

extern const char kCCTIncognitoAvailableToThirdPartyName[];
extern const char kCCTIncognitoAvailableToThirdPartyDescription[];

extern const char kCCTMinimizedName[];
extern const char kCCTMinimizedDescription[];

extern const char kCCTNestedSecurityIconName[];
extern const char kCCTNestedSecurityIconDescription[];

extern const char kCCTGoogleBottomBarName[];
extern const char kCCTGoogleBottomBarDescription[];

extern const char kCCTGoogleBottomBarVariantLayoutsName[];
extern const char kCCTGoogleBottomBarVariantLayoutsDescription[];

extern const char kCCTResizableForThirdPartiesName[];
extern const char kCCTResizableForThirdPartiesDescription[];

extern const char kCCTRevampedBrandingName[];
extern const char kCCTRevampedBrandingDescription[];

extern const char kCCTSignInPromptName[];
extern const char kCCTSignInPromptDescription[];

extern const char kAccountReauthenticationRecentTimeWindowName[];
extern const char kAccountReauthenticationRecentTimeWindowDescription[];
extern const char kAccountReauthenticationRecentTimeWindowDefault[];
extern const char kAccountReauthenticationRecentTimeWindow0mins[];
extern const char kAccountReauthenticationRecentTimeWindow1mins[];
extern const char kAccountReauthenticationRecentTimeWindow5mins[];

extern const char kBottomBrowserControlsRefactorName[];
extern const char kBottomBrowserControlsRefactorDescription[];

extern const char kBrowsingDataModelName[];
extern const char kBrowsingDataModelDescription[];

extern const char kChimeAlwaysShowNotificationDescription[];
extern const char kChimeAlwaysShowNotificationName[];

extern const char kChimeAndroidSdkDescription[];
extern const char kChimeAndroidSdkName[];

extern const char kClankDefaultBrowserPromoName[];
extern const char kClankDefaultBrowserPromoDescription[];

extern const char kClankDefaultBrowserPromoRoleManagerName[];
extern const char kClankDefaultBrowserPromoRoleManagerDescription[];

extern const char kClearLoginDatabaseForAllMigratedUPMUsersDescription[];
extern const char kClearLoginDatabaseForAllMigratedUPMUsersName[];

extern const char kTabStateFlatBufferName[];
extern const char kTabStateFlatBufferDescription[];

extern const char kContextualSearchSuppressShortViewName[];
extern const char kContextualSearchSuppressShortViewDescription[];

extern const char kConvertTrackpadEventsToMouseName[];
extern const char kConvertTrackpadEventsToMouseDescription[];

extern const char kDefaultViewportIsDeviceWidthName[];
extern const char kDefaultViewportIsDeviceWidthDescription[];

extern const char kDeprecatedExternalPickerFunctionName[];
extern const char kDeprecatedExternalPickerFunctionDescription[];

extern const char kDragDropIntoOmniboxName[];
extern const char kDragDropIntoOmniboxDescription[];

extern const char kDragDropTabTearingName[];
extern const char kDragDropTabTearingDescription[];

extern const char kDragDropTabTearingEnableOEMName[];
extern const char kDragDropTabTearingEnableOEMDescription[];

extern const char kDrawCutoutEdgeToEdgeName[];
extern const char kDrawCutoutEdgeToEdgeDescription[];

extern const char kDrawEdgeToEdgeName[];
extern const char kDrawEdgeToEdgeDescription[];

extern const char kDrawKeyNativeEdgeToEdgeName[];
extern const char kDrawKeyNativeEdgeToEdgeDescription[];

extern const char kDrawNativeEdgeToEdgeName[];
extern const char kDrawNativeEdgeToEdgeDescription[];

extern const char kDynamicSafeAreaInsetsName[];
extern const char kDynamicSafeAreaInsetsDescription[];

extern const char kDynamicSafeAreaInsetsOnScrollName[];
extern const char kDynamicSafeAreaInsetsOnScrollDescription[];

extern const char kEdgeToEdgeBottomChinName[];
extern const char kEdgeToEdgeBottomChinDescription[];

extern const char kEdgeToEdgeEverywhereName[];
extern const char kEdgeToEdgeEverywhereDescription[];

extern const char kEdgeToEdgeWebOptInName[];
extern const char kEdgeToEdgeWebOptInDescription[];

extern const char kEducationalTipModuleName[];
extern const char kEducationalTipModuleDescription[];

extern const char kEnableAndroidGamepadVibrationName[];
extern const char kEnableAndroidGamepadVibrationDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnablePixDetectionName[];
extern const char kEnablePixDetectionDescription[];

extern const char kEnablePixDetectionOnDomContentLoadedName[];
extern const char kEnablePixDetectionOnDomContentLoadedDescription[];

extern const char kEwalletPaymentsName[];
extern const char kEwalletPaymentsDescription[];

extern const char kExternalNavigationDebugLogsName[];
extern const char kExternalNavigationDebugLogsDescription[];

extern const char kFeedFollowUiUpdateName[];
extern const char kFeedFollowUiUpdateDescription[];

extern const char kFeedLoadingPlaceholderName[];
extern const char kFeedLoadingPlaceholderDescription[];

extern const char kFeedSignedOutViewDemotionName[];
extern const char kFeedSignedOutViewDemotionDescription[];

extern const char kFeedStampName[];
extern const char kFeedStampDescription[];

extern const char kFeedCloseRefreshName[];
extern const char kFeedCloseRefreshDescription[];

extern const char kFeedContainmentName[];
extern const char kFeedContainmentDescription[];

extern const char kFeedDynamicColorsName[];
extern const char kFeedDynamicColorsDescription[];

extern const char kFeedDiscoFeedEndpointName[];
extern const char kFeedDiscoFeedEndpointDescription[];

extern const char kFetchGaiaHashOnSignInName[];
extern const char kFetchGaiaHashOnSignInDescription[];

extern const char kForceListTabSwitcherName[];
extern const char kForceListTabSwitcherDescription[];

extern const char kFullscreenInsetsApiMigrationName[];
extern const char kFullscreenInsetsApiMigrationDescription[];

extern const char kFullscreenInsetsApiMigrationOnAutomotiveName[];
extern const char kFullscreenInsetsApiMigrationOnAutomotiveDescription[];

extern const char kGtsCloseTabAnimationName[];
extern const char kGtsCloseTabAnimationDescription[];

extern const char kRefreshFeedOnRestartName[];
extern const char kRefreshFeedOnRestartDescription[];

extern const char kInfoCardAcknowledgementTrackingName[];
extern const char kInfoCardAcknowledgementTrackingDescription[];

extern const char kInterestFeedV2Name[];
extern const char kInterestFeedV2Description[];

extern const char kInterestFeedV2HeartsName[];
extern const char kInterestFeedV2HeartsDescription[];

extern const char kLogoPolishName[];
extern const char kLogoPolishDescription[];

extern const char kLogoPolishAnimationKillSwitchName[];
extern const char kLogoPolishAnimationKillSwitchDescription[];

extern const char kMagicStackAndroidName[];
extern const char kMagicStackAndroidDescription[];

extern const char kMayLaunchUrlUsesSeparateStoragePartitionName[];
extern const char kMayLaunchUrlUsesSeparateStoragePartitionDescription[];

extern const char kSegmentationPlatformAndroidHomeModuleRankerV2Name[];
extern const char kSegmentationPlatformAndroidHomeModuleRankerV2Description[];

extern const char kMediaPickerAdoptionStudyName[];
extern const char kMediaPickerAdoptionStudyDescription[];

extern const char kMessagesForAndroidAdsBlockedName[];
extern const char kMessagesForAndroidAdsBlockedDescription[];

extern const char kMessagesForAndroidSaveCardName[];
extern const char kMessagesForAndroidSaveCardDescription[];

extern const char kMigrateSyncingUserToSignedInName[];
extern const char kMigrateSyncingUserToSignedInDescription[];

extern const char kMouseAndTrackpadDropdownMenuName[];
extern const char kMouseAndTrackpadDropdownMenuDescription[];

extern const char kNavBarColorMatchesTabBackgroundName[];
extern const char kNavBarColorMatchesTabBackgroundDescription[];

extern const char kNotificationOneTapUnsubscribeName[];
extern const char kNotificationOneTapUnsubscribeDescription[];

extern const char kNotificationPermissionRationaleName[];
extern const char kNotificationPermissionRationaleDescription[];

extern const char kNotificationPermissionRationaleBottomSheetName[];
extern const char kNotificationPermissionRationaleBottomSheetDescription[];

extern const char kOfflineAutoFetchName[];
extern const char kOfflineAutoFetchDescription[];

extern const char kOfflineContentOnNetErrorName[];
extern const char kOfflineContentOnNetErrorDescription[];

extern const char kOmahaMinSdkVersionAndroidName[];
extern const char kOmahaMinSdkVersionAndroidDescription[];
extern const char kOmahaMinSdkVersionAndroidMinSdk1Description[];
extern const char kOmahaMinSdkVersionAndroidMinSdk1000Description[];

extern const char kOmniboxShortcutsAndroidName[];
extern const char kOmniboxShortcutsAndroidDescription[];

extern const char kPasswordSuggestionBottomSheetV2Name[];
extern const char kPasswordSuggestionBottomSheetV2Description[];

extern const char kPaymentLinkDetectionName[];
extern const char kPaymentLinkDetectionDescription[];

extern const char kReadAloudName[];
extern const char kReadAloudDescription[];
extern const char kReadAloudBackgroundPlaybackName[];
extern const char kReadAloudBackgroundPlaybackDescription[];

extern const char kReadAloudInCCTName[];
extern const char kReadAloudInCCTDescription[];

extern const char kReadAloudTapToSeekName[];
extern const char kReadAloudTapToSeekDescription[];

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAllArticles[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

extern const char kReaderModeInCCTName[];
extern const char kReaderModeInCCTDescription[];

extern const char kReengagementNotificationName[];
extern const char kReengagementNotificationDescription[];

extern const char kRelatedSearchesAllLanguageName[];
extern const char kRelatedSearchesAllLanguageDescription[];

extern const char kRelatedSearchesSwitchName[];
extern const char kRelatedSearchesSwitchDescription[];

extern const char kRevokeNotificationsPermissionIfDisabledOnAppLevelName[];
extern const char
    kRevokeNotificationsPermissionIfDisabledOnAppLevelDescription[];

extern const char kRichAutocompletionAndroidName[];
extern const char kRichAutocompletionAndroidDescription[];

extern const char kSafeBrowsingSyncCheckerCheckAllowlistName[];
extern const char kSafeBrowsingSyncCheckerCheckAllowlistDescription[];

extern const char kSecurePaymentConfirmationAndroidName[];
extern const char kSecurePaymentConfirmationAndroidDescription[];

extern const char kShareCustomActionsInCCTName[];
extern const char kShareCustomActionsInCCTDescription[];

extern const char kShowReadyToPayDebugInfoName[];
extern const char kShowReadyToPayDebugInfoDescription[];

extern const char kSendTabToSelfV2Name[];
extern const char kSendTabToSelfV2Description[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kSmartZoomName[];
extern const char kSmartZoomDescription[];

extern const char kSmartSuggestionForLargeDownloadsName[];
extern const char kSmartSuggestionForLargeDownloadsDescription[];

extern const char kFeedPositionAndroidName[];
extern const char kFeedPositionAndroidDescription[];

extern const char kSearchResumptionModuleAndroidName[];
extern const char kSearchResumptionModuleAndroidDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kSupportMultipleServerRequestsForPixPaymentsName[];
extern const char kSupportMultipleServerRequestsForPixPaymentsDescription[];

extern const char kCrossDeviceTabPaneAndroidName[];
extern const char kCrossDeviceTabPaneAndroidDescription[];

extern const char kTabGroupCreationDialogAndroidName[];
extern const char kTabGroupCreationDialogAndroidDescription[];

extern const char kTabGroupPaneAndroidName[];
extern const char kTabGroupPaneAndroidDescription[];

extern const char kTabGroupParityAndroidName[];
extern const char kTabGroupParityAndroidDescription[];

extern const char kTabGroupsForTabletsName[];
extern const char kTabGroupsForTabletsDescription[];

extern const char kTabGroupSyncAndroidName[];
extern const char kTabGroupSyncAndroidDescription[];

extern const char kTabGroupSyncDisableNetworkLayerName[];
extern const char kTabGroupSyncDisableNetworkLayerDescription[];

extern const char kTabGroupSyncForceOffName[];
extern const char kTabGroupSyncForceOffDescription[];

extern const char kTabStripGroupCollapseAndroidName[];
extern const char kTabStripGroupCollapseAndroidDescription[];

extern const char kTabStripGroupContextMenuAndroidName[];
extern const char kTabStripGroupContextMenuAndroidDescription[];

extern const char kTabStripIncognitoMigrationName[];
extern const char kTabStripIncognitoMigrationDescription[];

extern const char kTabStripLayoutOptimizationName[];
extern const char kTabStripLayoutOptimizationDescription[];

extern const char kTabStripTransitionInDesktopWindowName[];
extern const char kTabStripTransitionInDesktopWindowDescription[];

extern const char kTouchDragAndContextMenuName[];
extern const char kTouchDragAndContextMenuDescription[];

extern const char kUndoMigrationOfSyncingUserToSignedInName[];
extern const char kUndoMigrationOfSyncingUserToSignedInDescription[];

extern const char
    kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningName[];
extern const char
    kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningDescription[];

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
extern const char kOpenXRExtendedFeaturesName[];
extern const char kOpenXRExtendedFeaturesDescription[];

extern const char kOpenXRName[];
extern const char kOpenXRDescription[];
#endif

// Non-Android ----------------------------------------------------------------

#else  // !BUILDFLAG(IS_ANDROID)

extern const char kAllowAllSitesToInitiateMirroringName[];
extern const char kAllowAllSitesToInitiateMirroringDescription[];

extern const char kDialMediaRouteProviderName[];
extern const char kDialMediaRouteProviderDescription[];
extern const char kDelayMediaSinkDiscoveryName[];
extern const char kDelayMediaSinkDiscoveryDescription[];
extern const char kShowCastPermissionRejectedErrorName[];
extern const char kShowCastPermissionRejectedErrorDescription[];

extern const char kCastMirroringTargetPlayoutDelayName[];
extern const char kCastMirroringTargetPlayoutDelayDescription[];
extern const char kCastMirroringTargetPlayoutDelayDefault[];
extern const char kCastMirroringTargetPlayoutDelay100ms[];
extern const char kCastMirroringTargetPlayoutDelay150ms[];
extern const char kCastMirroringTargetPlayoutDelay250ms[];
extern const char kCastMirroringTargetPlayoutDelay300ms[];
extern const char kCastMirroringTargetPlayoutDelay350ms[];
extern const char kCastMirroringTargetPlayoutDelay400ms[];

extern const char kEnablePolicyTestPageName[];
extern const char kEnablePolicyTestPageDescription[];

extern const char kEnableLiveCaptionMultilangName[];
extern const char kEnableLiveCaptionMultilangDescription[];

extern const char kEnableCrOSLiveTranslateName[];
extern const char kEnableCrOSLiveTranslateDescription[];

extern const char kEnableCrOSSodaLanguagesName[];
extern const char kEnableCrOSSodaLanguagesDescription[];

extern const char kEnableCrOSSodaConchLanguagesName[];
extern const char kEnableCrOSSodaConchLanguagesDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kEsbDownloadRowPromoFeatureName[];
extern const char kEsbDownloadRowPromoFeatureDescription[];
#endif

extern const char kForceSigninFlowInProfilePickerName[];
extern const char kForceSigninFlowInProfilePickerDescription[];

extern const char kFreezingOnEnergySaverName[];
extern const char kFreezingOnEnergySaverDescription[];

extern const char kFreezingOnEnergySaverTestingName[];
extern const char kFreezingOnEnergySaverTestingDescription[];

extern const char kKeyboardAndPointerLockPromptName[];
extern const char kKeyboardAndPointerLockPromptDescription[];

extern const char kPressAndHoldEscToExitBrowserFullscreenName[];
extern const char kPressAndHoldEscToExitBrowserFullscreenDescription[];

extern const char kReadAnythingWithScreen2xName[];
extern const char kReadAnythingWithScreen2xDescription[];

extern const char kReadAnythingWithAlgorithmName[];
extern const char kReadAnythingWithAlgorithmDescription[];

extern const char kReadAnythingImagesViaAlgorithmName[];
extern const char kReadAnythingImagesViaAlgorithmDescription[];

extern const char kReadAnythingReadAloudName[];
extern const char kReadAnythingReadAloudDescription[];

extern const char kReadAloudAutoVoiceSwitchingName[];
extern const char kReadAloudAutoVoiceSwitchingDescription[];

extern const char kReadAnythingReadAloudAutomaticWordHighlightingName[];
extern const char kReadAnythingReadAloudAutomaticWordHighlightingDescription[];

extern const char kReadAnythingReadAloudPhraseHighlightingName[];
extern const char kReadAnythingReadAloudPhraseHighlightingDescription[];

extern const char kReadAloudLanguagePackDownloadingName[];
extern const char kReadAloudLanguagePackDownloadingDescription[];

extern const char kReadAnythingDocsIntegrationName[];
extern const char kReadAnythingDocsIntegrationDescription[];

extern const char kReadAnythingDocsLoadMoreButtonName[];
extern const char kReadAnythingDocsLoadMoreButtonDescription[];

extern const char kGlobalMediaControlsCastStartStopName[];
extern const char kGlobalMediaControlsCastStartStopDescription[];

extern const char kLinkPreviewName[];
extern const char kLinkPreviewDescription[];

extern const char kMuteNotificationSnoozeActionName[];
extern const char kMuteNotificationSnoozeActionDescription[];

extern const char kNtpAlphaBackgroundCollectionsName[];
extern const char kNtpAlphaBackgroundCollectionsDescription[];

extern const char kNtpBackgroundImageErrorDetectionName[];
extern const char kNtpBackgroundImageErrorDetectionDescription[];

extern const char kNtpCalendarModuleName[];
extern const char kNtpCalendarModuleDescription[];

extern const char kNtpChromeCartModuleName[];
extern const char kNtpChromeCartModuleDescription[];

extern const char kNtpDriveModuleName[];
extern const char kNtpDriveModuleDescription[];

extern const char kNtpDriveModuleSegmentationName[];
extern const char kNtpDriveModuleSegmentationDescription[];

extern const char kNtpDriveModuleShowSixFilesName[];
extern const char kNtpDriveModuleShowSixFilesDescription[];

#if !defined(OFFICIAL_BUILD)
extern const char kNtpDummyModulesName[];
extern const char kNtpDummyModulesDescription[];
#endif

extern const char kNtpMostRelevantTabResumptionModuleName[];
extern const char kNtpMostRelevantTabResumptionModuleDescription[];

extern const char kNtpMostRelevantTabResumptionModuleDeviceIconName[];
extern const char kNtpMostRelevantTabResumptionModuleDeviceIconDescription[];

extern const char kNtpMiddleSlotPromoDismissalName[];
extern const char kNtpMiddleSlotPromoDismissalDescription[];

extern const char kNtpMobilePromoName[];
extern const char kNtpMobilePromoDescription[];

extern const char kNtpModulesDragAndDropName[];
extern const char kNtpModulesDragAndDropDescription[];

extern const char kNtpModulesHeaderIconName[];
extern const char kNtpModulesHeaderIconDescription[];

extern const char kNtpModulesRedesignedName[];
extern const char kNtpModulesRedesignedDescription[];

extern const char kNtpOutlookCalendarModuleName[];
extern const char kNtpOutlookCalendarModuleDescription[];

extern const char kNtpPhotosModuleOptInTitleName[];
extern const char kNtpPhotosModuleOptInTitleDescription[];

extern const char kNtpPhotosModuleOptInArtWorkName[];
extern const char kNtpPhotosModuleOptInArtWorkDescription[];

extern const char kNtpPhotosModuleName[];
extern const char kNtpPhotosModuleDescription[];

extern const char kNtpPhotosModuleSoftOptOutName[];
extern const char kNtpPhotosModuleSoftOptOutDescription[];

extern const char kNtpRealboxContextualAndTrendingSuggestionsName[];
extern const char kNtpRealboxContextualAndTrendingSuggestionsDescription[];

extern const char kNtpRealboxCr23ThemingName[];
extern const char kNtpRealboxCr23ThemingDescription[];

extern const char kNtpRealboxMatchSearchboxThemeName[];
extern const char kNtpRealboxMatchSearchboxThemeDescription[];

extern const char kNtpRealboxPedalsName[];
extern const char kNtpRealboxPedalsDescription[];

extern const char kNtpRealboxUseGoogleGIconName[];
extern const char kNtpRealboxUseGoogleGIconDescription[];

extern const char kNtpSafeBrowsingModuleName[];
extern const char kNtpSafeBrowsingModuleDescription[];

extern const char kNtpSharepointModuleName[];
extern const char kNtpSharepointModuleDescription[];

extern const char kNtpWallpaperSearchButtonName[];
extern const char kNtpWallpaperSearchButtonDescription[];

extern const char kNtpWallpaperSearchButtonAnimationName[];
extern const char kNtpWallpaperSearchButtonAnimationDescription[];

extern const char kNtpWideModulesName[];
extern const char kNtpWideModulesDescription[];

extern const char kHappinessTrackingSurveysForDesktopDemoName[];
extern const char kHappinessTrackingSurveysForDesktopDemoDescription[];

extern const char kHatsWebUIName[];
extern const char kHatsWebUIDescription[];

extern const char kMainNodeAnnotationsName[];
extern const char kMainNodeAnnotationsDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescription[];

extern const char kOmniboxDriveSuggestionsNoSettingName[];
extern const char kOmniboxDriveSuggestionsNoSettingDescription[];

extern const char kOmniboxDriveSuggestionsNoSyncRequirementName[];
extern const char kOmniboxDriveSuggestionsNoSyncRequirementDescription[];

extern const char kProbabilisticMemorySaverName[];
extern const char kProbabilisticMemorySaverDescription[];

extern const char kSCTAuditingName[];
extern const char kSCTAuditingDescription[];

extern const char kSmartCardWebApiName[];
extern const char kSmartCardWebApiDescription[];

extern const char kTabOrganizationName[];
extern const char kTabOrganizationDescription[];

extern const char kTabOrganizationSettingsVisibilityName[];
extern const char kTabOrganizationSettingsVisibilityDescription[];

extern const char kMultiTabOrganizationName[];
extern const char kMultiTabOrganizationDescription[];

extern const char kTabReorganizationName[];
extern const char kTabReorganizationDescription[];

extern const char kTabReorganizationDividerName[];
extern const char kTabReorganizationDividerDescription[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kIOSPromoRefreshedPasswordBubbleName[];
extern const char kIOSPromoRefreshedPasswordBubbleDescription[];

extern const char kIOSPromoAddressBubbleName[];
extern const char kIOSPromoAddressBubbleDescription[];

extern const char kIOSPromoPaymentBubbleName[];
extern const char kIOSPromoPaymentBubbleDescription[];
#endif

extern const char kWebAppSyncGeneratedIconBackgroundFixName[];
extern const char kWebAppSyncGeneratedIconBackgroundFixDescription[];

extern const char kWebAppSyncGeneratedIconRetroactiveFixName[];
extern const char kWebAppSyncGeneratedIconRetroactiveFixDescription[];

extern const char kWebAppSyncGeneratedIconUpdateFixName[];
extern const char kWebAppSyncGeneratedIconUpdateFixDescription[];

extern const char kWebAppUniversalInstallName[];
extern const char kWebAppUniversalInstallDescription[];

extern const char kShortcutsNotAppsName[];
extern const char kShortcutsNotAppsDescription[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kTranslateOpenSettingsName[];
extern const char kTranslateOpenSettingsDescription[];
#endif

extern const char kUserDisplayModeSyncBrowserMitigationName[];
extern const char kUserDisplayModeSyncBrowserMitigationDescription[];

extern const char kUserDisplayModeSyncStandaloneMitigationName[];
extern const char kUserDisplayModeSyncStandaloneMitigationDescription[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
extern const char kWebAppSystemMediaControlsName[];
extern const char kWebAppSystemMediaControlsDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

extern const char kWebAuthenticationPermitEnterpriseAttestationName[];
extern const char kWebAuthenticationPermitEnterpriseAttestationDescription[];

#endif  // BUILDFLAG(IS_ANDROID)

// Windows --------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

extern const char kAuthenticateUsingUserConsentVerifierInteropApiName[];
extern const char kAuthenticateUsingUserConsentVerifierInteropApiDescription[];

extern const char kAuthenticateUsingUserConsentVerifierApiName[];
extern const char kAuthenticateUsingUserConsentVerifierApiDescription[];

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kEnableMediaFoundationVideoCaptureName[];
extern const char kEnableMediaFoundationVideoCaptureDescription[];

extern const char kHardwareSecureDecryptionName[];
extern const char kHardwareSecureDecryptionDescription[];

extern const char kHardwareSecureDecryptionExperimentName[];
extern const char kHardwareSecureDecryptionExperimentDescription[];

extern const char kHardwareSecureDecryptionFallbackName[];
extern const char kHardwareSecureDecryptionFallbackDescription[];

extern const char kHidGetFeatureReportFixName[];
extern const char kHidGetFeatureReportFixDescription[];

extern const char kMediaFoundationClearName[];
extern const char kMediaFoundationClearDescription[];

extern const char kMediaFoundationClearStrategyName[];
extern const char kMediaFoundationClearStrategyDescription[];

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

extern const char kWebRtcAllowWgcScreenCapturerName[];
extern const char kWebRtcAllowWgcScreenCapturerDescription[];

extern const char kWebRtcAllowWgcWindowCapturerName[];
extern const char kWebRtcAllowWgcWindowCapturerDescription[];

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

extern const char kEnableExtensibleEnterpriseSSOName[];
extern const char kEnableExtensibleEnterpriseSSODescription[];

extern const char kImmersiveFullscreenName[];
extern const char kImmersiveFullscreenDescription[];

extern const char kMacFullscreenAnimateTabsName[];
extern const char kMacFullscreenAnimateTabsDescription[];

extern const char kFullscreenAlwaysShowTrafficLightsName[];
extern const char kFullscreenAlwaysShowTrafficLightsDescription[];

extern const char kMacImeLiveConversionFixName[];
extern const char kMacImeLiveConversionFixDescription[];

extern const char kMacLoopbackAudioForScreenShareName[];
extern const char kMacLoopbackAudioForScreenShareDescription[];

extern const char kMacPWAsNotificationAttributionName[];
extern const char kMacPWAsNotificationAttributionDescription[];

extern const char kMacSyscallSandboxName[];
extern const char kMacSyscallSandboxDescription[];

extern const char kRetryGetVideoCaptureDeviceInfosName[];
extern const char kRetryGetVideoCaptureDeviceInfosDescription[];

extern const char kSonomaAccessibilityActivationRefinementsName[];
extern const char kSonomaAccessibilityActivationRefinementsDescription[];

extern const char kUseAngleDescriptionMac[];
extern const char kUseAngleMetal[];

extern const char kUseAdHocSigningForWebAppShimsName[];
extern const char kUseAdHocSigningForWebAppShimsDescription[];

extern const char kUseSCContentSharingPickerName[];
extern const char kUseSCContentSharingPickerDescription[];

#endif  // BUILDFLAG(IS_MAC)

// Windows and Mac ------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

extern const char kLocationProviderManagerName[];
extern const char kLocationProviderManagerDescription[];

extern const char kUseAngleName[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// ChromeOS -------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAccessibilityFilterKeysName[];
extern const char kAccessibilityFilterKeysDescription[];

extern const char kAdaptiveChargingName[];
extern const char kAdaptiveChargingDescription[];

extern const char kAdaptiveChargingForTestingName[];
extern const char kAdaptiveChargingForTestingDescription[];

extern const char kAllowApnModificationPolicyName[];
extern const char kAllowApnModificationPolicyDescription[];

extern const char kAllowCrossDeviceFeatureSuiteName[];
extern const char kAllowCrossDeviceFeatureSuiteDescription[];

extern const char kLinkCrossDeviceInternalsName[];
extern const char kLinkCrossDeviceInternalsDescription[];

extern const char kAllowDevtoolsInSystemUIName[];
extern const char kAllowDevtoolsInSystemUIDescription[];

extern const char kAllowFpmcuBetaFirmwareName[];
extern const char kAllowFpmcuBetaFirmwareDescription[];

extern const char kAllowScrollSettingsName[];
extern const char kAllowScrollSettingsDescription[];

extern const char kAltClickAndSixPackCustomizationName[];
extern const char kAltClickAndSixPackCustomizationDescription[];
extern const char kAlwaysEnableHdcpName[];
extern const char kAlwaysEnableHdcpDescription[];
extern const char kAlwaysEnableHdcpDefault[];
extern const char kAlwaysEnableHdcpType0[];
extern const char kAlwaysEnableHdcpType1[];

extern const char kApnPoliciesName[];
extern const char kApnPoliciesDescription[];

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

extern const char kArcEnableAttestationName[];
extern const char kArcEnableAttestationDescription[];

extern const char kArcExtendInputAnrTimeoutName[];
extern const char kArcExtendInputAnrTimeoutDescription[];

extern const char kArcExtendIntentAnrTimeoutName[];
extern const char kArcExtendIntentAnrTimeoutDescription[];

extern const char kArcExtendServiceAnrTimeoutName[];
extern const char kArcExtendServiceAnrTimeoutDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcFriendlierErrorDialogName[];
extern const char kArcFriendlierErrorDialogDescription[];

extern const char kArcIdleManagerName[];
extern const char kArcIdleManagerDescription[];

extern const char kArcIgnoreHoverEventAnrName[];
extern const char kArcIgnoreHoverEventAnrDescription[];

extern const char kArcInstantResponseWindowOpenName[];
extern const char kArcInstantResponseWindowOpenDescription[];

extern const char kArcNativeBridgeToggleName[];
extern const char kArcNativeBridgeToggleDescription[];

extern const char kArcPerAppLanguageName[];
extern const char kArcPerAppLanguageDescription[];

extern const char kArcResizeCompatName[];
extern const char kArcResizeCompatDescription[];

extern const char kArcRoundedWindowCompatName[];
extern const char kArcRoundedWindowCompatDescription[];

extern const char kArcRtVcpuDualCoreName[];
extern const char kArcRtVcpuDualCoreDesc[];

extern const char kArcRtVcpuQuadCoreName[];
extern const char kArcRtVcpuQuadCoreDesc[];

extern const char kArcSwitchToKeyMintDaemonName[];
extern const char kArcSwitchToKeyMintDaemonDesc[];

extern const char kArcSwitchToKeyMintOnTName[];
extern const char kArcSwitchToKeyMintOnTDesc[];

extern const char kArcSwitchToKeyMintOnTOverrideName[];
extern const char kArcSwitchToKeyMintOnTOverrideDesc[];

extern const char kArcSyncInstallPriorityName[];
extern const char kArcSyncInstallPriorityDescription[];

extern const char kArcTouchscreenEmulationName[];
extern const char kArcTouchscreenEmulationDesc[];

extern const char kArcVmmSwapKBShortcutName[];
extern const char kArcVmmSwapKBShortcutDesc[];

extern const char kArcVmMemorySizeName[];
extern const char kArcVmMemorySizeDesc[];

extern const char kArcAAudioMMAPLowLatencyName[];
extern const char kArcAAudioMMAPLowLatencyDescription[];

extern const char kArcEnableVirtioBlkForDataName[];
extern const char kArcEnableVirtioBlkForDataDesc[];

extern const char kArcExternalStorageAccessName[];
extern const char kArcExternalStorageAccessDescription[];

extern const char kArcUnthrottleOnActiveAudioV2Name[];
extern const char kArcUnthrottleOnActiveAudioV2Description[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAshModifierSplitName[];
extern const char kAshModifierSplitDescription[];

extern const char kAshModifierSplitFeatureKeyName[];
extern const char kAshModifierSplitFeatureKeyDescription[];

extern const char kAshPickerName[];
extern const char kAshPickerDescription[];

extern const char kAshPickerAlwaysShowFeatureTourName[];
extern const char kAshPickerAlwaysShowFeatureTourDescription[];

extern const char kAshPickerCloudName[];
extern const char kAshPickerCloudDescription[];

extern const char kAshPickerGifsName[];
extern const char kAshPickerGifsDescription[];

extern const char kAshPickerGridName[];
extern const char kAshPickerGridDescription[];

extern const char kAshSplitKeyboardRefactorName[];
extern const char kAshSplitKeyboardRefactorDescription[];

extern const char kAssistantIphName[];
extern const char kAssistantIphDescription[];

extern const char kAudioA2DPAdvancedCodecsName[];
extern const char kAudioA2DPAdvancedCodecsDescription[];

extern const char kAudioAPNoiseCancellationName[];
extern const char kAudioAPNoiseCancellationDescription[];

extern const char kAudioHFPMicSRToggleName[];
extern const char kAudioHFPMicSRToggleDescription[];

extern const char kAudioHFPSwbName[];
extern const char kAudioHFPSwbDescription[];

extern const char kAudioHFPOffloadName[];
extern const char kAudioHFPOffloadDescription[];

extern const char kAudioOffloadCrasDSPToSOFName[];
extern const char kAudioOffloadCrasDSPToSOFDescription[];

extern const char kAudioStyleTransferName[];
extern const char kAudioStyleTransferDescription[];

extern const char kAudioAecRequiredForCrasProcessorName[];
extern const char kAudioAecRequiredForCrasProcessorDescription[];

extern const char kAudioSelectionImprovementName[];
extern const char kAudioSelectionImprovementDescription[];

extern const char kAudioSuppressSetRTCAudioActiveName[];
extern const char kAudioSuppressSetRTCAudioActiveDescription[];

extern const char kAutoFramingOverrideName[];
extern const char kAutoFramingOverrideDescription[];

extern const char kAutocorrectByDefaultName[];
extern const char kAutocorrectByDefaultDescription[];

extern const char kAutocorrectParamsTuningName[];
extern const char kAutocorrectParamsTuningDescription[];

extern const char kAutocorrectToggleName[];
extern const char kAutocorrectToggleDescription[];

extern const char kAutocorrectUseReplaceSurroundingTextName[];
extern const char kAutocorrectUseReplaceSurroundingTextDescription[];

extern const char kBatteryBadgeIconName[];
extern const char kBatteryBadgeIconDescription[];

extern const char kBlockTelephonyDevicePhoneMuteName[];
extern const char kBlockTelephonyDevicePhoneMuteDescription[];

extern const char kBluetoothAudioLEAudioOnlyName[];
extern const char kBluetoothAudioLEAudioOnlyDescription[];

extern const char kBluetoothBtsnoopInternalsName[];
extern const char kBluetoothBtsnoopInternalsDescription[];

extern const char kBluetoothFlossTelephonyName[];
extern const char kBluetoothFlossTelephonyDescription[];

extern const char kBluetoothDisconnectWarningName[];
extern const char kBluetoothDisconnectWarningDescription[];

extern const char kBluetoothCoredumpName[];
extern const char kBluetoothCoredumpDescription[];

extern const char kBluetoothFlossCoredumpName[];
extern const char kBluetoothFlossCoredumpDescription[];

extern const char kBluetoothUseFlossName[];
extern const char kBluetoothUseFlossDescription[];

extern const char kBluetoothWifiQSPodRefreshName[];
extern const char kBluetoothWifiQSPodRefreshDescription[];

extern const char kBluetoothFlossIsAvailabilityCheckNeededName[];
extern const char kBluetoothFlossIsAvailabilityCheckNeededDescription[];

extern const char kBluetoothUseLLPrivacyName[];
extern const char kBluetoothUseLLPrivacyDescription[];

extern const char kCampbellGlyphName[];
extern const char kCampbellGlyphDescription[];

extern const char kCampbellKeyName[];
extern const char kCampbellKeyDescription[];

extern const char kCaptureModeEducationName[];
extern const char kCaptureModeEducationDescription[];

extern const char kCaptureModeEducationBypassLimitsName[];
extern const char kCaptureModeEducationBypassLimitsDescription[];

extern const char kCaptureModeGifRecordingName[];
extern const char kCaptureModeGifRecordingDescription[];

extern const char kCrosContentAdjustedRefreshRateName[];
extern const char kCrosContentAdjustedRefreshRateDescription[];

extern const char kCrosSoulName[];
extern const char kCrosSoulDescription[];

extern const char kCrosSoulGravediggerName[];
extern const char kCrosSoulGravediggerDescription[];

extern const char kDeskButtonName[];
extern const char kDeskButtonDescription[];

extern const char kDesksTemplatesName[];
extern const char kDesksTemplatesDescription[];

extern const char kPreferConstantFrameRateName[];
extern const char kPreferConstantFrameRateDescription[];

extern const char kForceControlFaceAeName[];
extern const char kForceControlFaceAeDescription[];

extern const char kCellularBypassESimInstallationConnectivityCheckName[];
extern const char kCellularBypassESimInstallationConnectivityCheckDescription[];

extern const char kCellularUseSecondEuiccName[];
extern const char kCellularUseSecondEuiccDescription[];

extern const char kClipboardHistoryLongpressName[];
extern const char kClipboardHistoryLongpressDescription[];

extern const char kClipboardHistoryRefreshName[];
extern const char kClipboardHistoryRefreshDescription[];

extern const char kClipboardHistoryUrlTitlesName[];
extern const char kClipboardHistoryUrlTitlesDescription[];

extern const char kCloudGamingDeviceName[];
extern const char kCloudGamingDeviceDescription[];

extern const char kComponentUpdaterTestRequestName[];
extern const char kComponentUpdaterTestRequestDescription[];

extern const char kCroshSWAName[];
extern const char kCroshSWADescription[];

extern const char kEnableServiceWorkersForChromeUntrustedName[];
extern const char kEnableServiceWorkersForChromeUntrustedDescription[];

extern const char kEnterpriseReportingUIName[];
extern const char kEnterpriseReportingUIDescription[];

extern const char kPermissiveUsbPassthroughName[];
extern const char kPermissiveUsbPassthroughDescription[];

extern const char kCameraAngleBackendName[];
extern const char kCameraAngleBackendDescription[];

extern const char kCameraSuperResOverrideName[];
extern const char kCameraSuperResOverrideDescription[];

extern const char kDisableBruschettaInstallChecksName[];
extern const char kDisableBruschettaInstallChecksDescription[];

extern const char kCrostiniContainerInstallName[];
extern const char kCrostiniContainerInstallDescription[];

extern const char kCrostiniGpuSupportName[];
extern const char kCrostiniGpuSupportDescription[];

extern const char kCrostiniResetLxdDbName[];
extern const char kCrostiniResetLxdDbDescription[];

extern const char kCrostiniMultiContainerName[];
extern const char kCrostiniMultiContainerDescription[];

extern const char kCrostiniQtImeSupportName[];
extern const char kCrostiniQtImeSupportDescription[];

extern const char kCrostiniVirtualKeyboardSupportName[];
extern const char kCrostiniVirtualKeyboardSupportDescription[];

extern const char kCameraAppDigitalZoomName[];
extern const char kCameraAppDigitalZoomDescription[];

extern const char kCameraAppPdfOcrName[];
extern const char kCameraAppPdfOcrDescription[];

extern const char kCameraAppPreviewOcrName[];
extern const char kCameraAppPreviewOcrDescription[];

extern const char kChromeboxUsbPassthroughRestrictionsName[];
extern const char kChromeboxUsbPassthroughRestrictionsDescription[];

extern const char kConchName[];
extern const char kConchDescription[];

extern const char kConchFeatureKeyName[];
extern const char kConchFeatureKeyDescription[];

extern const char kConchSystemAudioFromMicName[];
extern const char kConchSystemAudioFromMicDescription[];

extern const char kDeskProfilesName[];
extern const char kDeskProfilesDescription[];

extern const char kDisableCameraFrameRotationAtSourceName[];
extern const char kDisableCameraFrameRotationAtSourceDescription[];

extern const char kDisableCancelAllTouchesName[];
extern const char kDisableCancelAllTouchesDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisplayAlignmentAssistanceName[];
extern const char kDisplayAlignmentAssistanceDescription[];

extern const char kFaceRetouchOverrideName[];
extern const char kFaceRetouchOverrideDescription[];

extern const char kFastPairDebugMetadataName[];
extern const char kFastPairDebugMetadataDescription[];

extern const char kFastPairHandshakeLongTermRefactorName[];
extern const char kFastPairHandshakeLongTermRefactorDescription[];

extern const char kFastPairHIDName[];
extern const char kFastPairHIDDescription[];

extern const char kFastPairKeyboardsName[];
extern const char kFastPairKeyboardsDescription[];

extern const char kFastPairPwaCompanionName[];
extern const char kFastPairPwaCompanionDescription[];

extern const char kFastPairDevicesBluetoothSettingsName[];
extern const char kFastPairDevicesBluetoothSettingsDescription[];

extern const char kFrameSinkDesktopCapturerInCrdName[];
extern const char kFrameSinkDesktopCapturerInCrdDescription[];

extern const char kUseHDRTransferFunctionName[];
extern const char kUseHDRTransferFunctionDescription[];

extern const char kEnableExternalDisplayHdr10Name[];
extern const char kEnableExternalDisplayHdr10Description[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kQuickSettingsPWANotificationsName[];
extern const char kQuickSettingsPWANotificationsDescription[];

extern const char kDriveFsMirroringName[];
extern const char kDriveFsMirroringDescription[];

extern const char kDriveFsShowCSEFilesName[];
extern const char kDriveFsShowCSEFilesDescription[];

extern const char kEnableAssistantRoutinesName[];
extern const char kEnableAssistantRoutinesDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnableBrightnessControlInSettingsName[];
extern const char kEnableBrightnessControlInSettingsDescription[];

extern const char kEnableDisplayPerformanceModeName[];
extern const char kEnableDisplayPerformanceModeDescription[];

extern const char kDisableDnsProxyName[];
extern const char kDisableDnsProxyDescription[];

extern const char kDisconnectWiFiOnEthernetConnectedName[];
extern const char kDisconnectWiFiOnEthernetConnectedDescription[];

extern const char kEnableRFC8925Name[];
extern const char kEnableRFC8925Description[];

extern const char kEnableEdidBasedDisplayIdsName[];
extern const char kEnableEdidBasedDisplayIdsDescription[];

extern const char kTiledDisplaySupportName[];
extern const char kTiledDisplaySupportDescription[];

extern const char kEnableExternalKeyboardsInDiagnosticsAppName[];
extern const char kEnableExternalKeyboardsInDiagnosticsAppDescription[];

extern const char kEnableFastInkForSoftwareCursorName[];
extern const char kEnableFastInkForSoftwareCursorDescription[];

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

extern const char kEnableKeyboardBacklightControlInSettingsName[];
extern const char kEnableKeyboardBacklightControlInSettingsDescription[];

extern const char kEnableKeyboardBacklightToggleName[];
extern const char kEnableKeyboardBacklightToggleDescription[];

extern const char kEnableKeyboardRewriterFixName[];
extern const char kEnableKeyboardRewriterFixDescription[];

extern const char kEnableLibinputToHandleTouchpadName[];
extern const char kEnableLibinputToHandleTouchpadDescription[];

extern const char kEnableNeuralPalmAdaptiveHoldName[];
extern const char kEnableNeuralPalmAdaptiveHoldDescription[];

extern const char kEnableFakeKeyboardHeuristicName[];
extern const char kEnableFakeKeyboardHeuristicDescription[];

extern const char kEnableFakeMouseHeuristicName[];
extern const char kEnableFakeMouseHeuristicDescription[];

extern const char kEnableHeatmapPalmDetectionName[];
extern const char kEnableHeatmapPalmDetectionDescription[];

extern const char kEnableNeuralStylusPalmRejectionName[];
extern const char kEnableNeuralStylusPalmRejectionDescription[];

extern const char kEnableEdgeDetectionName[];
extern const char kEnableEdgeDetectionDescription[];

extern const char kEnableFastTouchpadClickName[];
extern const char kEnableFastTouchpadClickDescription[];

extern const char kEnablePalmSuppressionName[];
extern const char kEnablePalmSuppressionDescription[];

extern const char kEnableRemoveStalePolicyPinnedAppsFromShelfName[];
extern const char kEnableRemoveStalePolicyPinnedAppsFromShelfDescription[];

extern const char kEnableRuntimeCountersTelemetryName[];
extern const char kEnableRuntimeCountersTelemetryDescription[];

extern const char kEnableSeamlessRefreshRateSwitchingName[];
extern const char kEnableSeamlessRefreshRateSwitchingDescription[];

extern const char kEnableTouchpadsInDiagnosticsAppName[];
extern const char kEnableTouchpadsInDiagnosticsAppDescription[];

extern const char kEnableTouchscreensInDiagnosticsAppName[];
extern const char kEnableTouchscreensInDiagnosticsAppDescription[];

extern const char kEnableWifiQosName[];
extern const char kEnableWifiQosDescription[];

extern const char kEnableWifiQosEnterpriseName[];
extern const char kEnableWifiQosEnterpriseDescription[];

extern const char kEapGtcWifiAuthenticationName[];
extern const char kEapGtcWifiAuthenticationDescription[];

extern const char kEcheSWAName[];
extern const char kEcheSWADescription[];

extern const char kEcheLauncherName[];
extern const char kEcheLauncherDescription[];

extern const char kEcheLauncherListViewName[];
extern const char kEcheLauncherListViewDescription[];

extern const char kEcheLauncherIconsInMoreAppsButtonName[];
extern const char kEcheLauncherIconsInMoreAppsButtonDescription[];

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

extern const char kEnableOngoingProcessesName[];
extern const char kEnableOngoingProcessesDescription[];

extern const char kPanelSelfRefresh2Name[];
extern const char kPanelSelfRefresh2Description[];

extern const char kEnableVariableRefreshRateName[];
extern const char kEnableVariableRefreshRateDescription[];

extern const char kEnforceAshExtensionKeeplistName[];
extern const char kEnforceAshExtensionKeeplistDescription[];

extern const char kEnterOverviewFromWallpaperName[];
extern const char kEnterOverviewFromWallpaperDescription[];

extern const char kEolIncentiveName[];
extern const char kEolIncentiveDescription[];

extern const char kEolResetDismissedPrefsName[];
extern const char kEolResetDismissedPrefsDescription[];

extern const char kEventBasedLogUpload[];
extern const char kEventBasedLogUploadDescription[];

extern const char kExcludeDisplayInMirrorModeName[];
extern const char kExcludeDisplayInMirrorModeDescription[];

extern const char kExoGamepadVibrationName[];
extern const char kExoGamepadVibrationDescription[];

extern const char kExoOrdinalMotionName[];
extern const char kExoOrdinalMotionDescription[];

extern const char kExperimentalAccessibilityDictationContextCheckingName[];
extern const char
    kExperimentalAccessibilityDictationContextCheckingDescription[];

extern const char kExperimentalAccessibilityGoogleTtsHighQualityVoicesName[];
extern const char
    kExperimentalAccessibilityGoogleTtsHighQualityVoicesDescription[];

extern const char kExperimentalAccessibilityManifestV3Name[];
extern const char kExperimentalAccessibilityManifestV3Description[];

extern const char kExperimentalAccessibilitySwitchAccessTextName[];
extern const char kExperimentalAccessibilitySwitchAccessTextDescription[];

extern const char kExposeOutOfProcessVideoDecodingToLacrosName[];
extern const char kExposeOutOfProcessVideoDecodingToLacrosDescription[];

extern const char kFileTransferEnterpriseConnectorName[];
extern const char kFileTransferEnterpriseConnectorDescription[];

extern const char kFileTransferEnterpriseConnectorUIName[];
extern const char kFileTransferEnterpriseConnectorUIDescription[];

extern const char kFilesConflictDialogName[];
extern const char kFilesConflictDialogDescription[];

extern const char kFilesKernelDriversName[];
extern const char kFilesKernelDriversDescription[];

extern const char kFilesExtractArchiveName[];
extern const char kFilesExtractArchiveDescription[];

extern const char kFilesLocalImageSearchName[];
extern const char kFilesLocalImageSearchDescription[];

extern const char kFilesMaterializedViewsName[];
extern const char kFilesMaterializedViewsDescription[];

extern const char kFilesSinglePartitionFormatName[];
extern const char kFilesSinglePartitionFormatDescription[];

extern const char kFilesTrashDriveName[];
extern const char kFilesTrashDriveDescription[];

extern const char kFileSystemProviderCloudFileSystemName[];
extern const char kFileSystemProviderCloudFileSystemDescription[];

extern const char kFileSystemProviderContentCacheName[];
extern const char kFileSystemProviderContentCacheDescription[];

extern const char kFirmwareUpdateUIV2Name[];
extern const char kFirmwareUpdateUIV2Description[];

extern const char kFirstPartyVietnameseInputName[];
extern const char kFirstPartyVietnameseInputDescription[];

extern const char kFocusFollowsCursorName[];
extern const char kFocusFollowsCursorDescription[];

extern const char kFocusModeName[];
extern const char kFocusModeDescription[];

extern const char kFocusModeYTMName[];
extern const char kFocusModeYTMDescription[];

extern const char kForceReSyncDriveName[];
extern const char kForceReSyncDriveDescription[];

extern const char kFuseBoxDebugName[];
extern const char kFuseBoxDebugDescription[];

extern const char kGameDashboard[];
extern const char kGameDashboardDescription[];

extern const char kGameDashboardGamepadSupport[];
extern const char kGameDashboardGamepadSupportDescription[];

extern const char kGameDashboardGamePWAs[];
extern const char kGameDashboardGamePWAsDescription[];

extern const char kGameDashboardGamesInTest[];
extern const char kGameDashboardGamesInTestDescription[];

extern const char kGameDashboardUtilities[];
extern const char kGameDashboardUtilitiesDescription[];

extern const char kGlanceablesTimeManagementClassroomStudentViewName[];
extern const char kGlanceablesTimeManagementClassroomStudentViewDescription[];

extern const char kGlanceablesTimeManagementTasksViewName[];
extern const char kGlanceablesTimeManagementTasksViewDescription[];

extern const char kHelpAppAppDetailPageName[];
extern const char kHelpAppAppDetailPageDescription[];

extern const char kHelpAppAppsListName[];
extern const char kHelpAppAppsListDescription[];

extern const char kHelpAppAutoTriggerInstallDialogName[];
extern const char kHelpAppAutoTriggerInstallDialogDescription[];

extern const char kHelpAppHomePageAppArticlesName[];
extern const char kHelpAppHomePageAppArticlesDescription[];

extern const char kHelpAppLauncherSearchName[];
extern const char kHelpAppLauncherSearchDescription[];

extern const char kHelpAppOnboardingRevampName[];
extern const char kHelpAppOnboardingRevampDescription[];

extern const char kHelpAppOpensInsteadOfReleaseNotesNotificationName[];
extern const char kHelpAppOpensInsteadOfReleaseNotesNotificationDescription[];

extern const char kDiacriticsUseReplaceSurroundingTextName[];
extern const char kDiacriticsUseReplaceSurroundingTextDescription[];

extern const char kHoldingSpaceSuggestionsName[];
extern const char kHoldingSpaceSuggestionsDescription[];

extern const char kHotspotName[];
extern const char kHotspotDescription[];

extern const char kImeAssistMultiWordName[];
extern const char kImeAssistMultiWordDescription[];

extern const char kImeFstDecoderParamsUpdateName[];
extern const char kImeFstDecoderParamsUpdateDescription[];

extern const char kImeKoreanModeSwitchDebugName[];
extern const char kImeKoreanModeSwitchDebugDescription[];

extern const char kImeKoreanOnlyModeSwitchOnRightAltName[];
extern const char kImeKoreanOnlyModeSwitchOnRightAltDescription[];

extern const char kImeSwitchCheckConnectionStatusName[];
extern const char kImeSwitchCheckConnectionStatusDescription[];

extern const char kIppFirstSetupForUsbPrintersName[];
extern const char kIppFirstSetupForUsbPrintersDescription[];

extern const char kLacrosProfileMigrationForceOffName[];
extern const char kLacrosProfileMigrationForceOffDescription[];

extern const char kLacrosProfileBackwardMigrationName[];
extern const char kLacrosProfileBackwardMigrationDescription[];

extern const char kHindiInscriptLayoutName[];
extern const char kHindiInscriptLayoutDescription[];

extern const char kImeSystemEmojiPickerGIFSupportName[];
extern const char kImeSystemEmojiPickerGIFSupportDescription[];

extern const char kImeSystemEmojiPickerJellySupportName[];
extern const char kImeSystemEmojiPickerJellySupportDescription[];

extern const char kImeSystemEmojiPickerMojoSearchName[];
extern const char kImeSystemEmojiPickerMojoSearchDescription[];

extern const char kImeSystemEmojiPickerVariantGroupingName[];
extern const char kImeSystemEmojiPickerVariantGroupingDescription[];

extern const char kImeUsEnglishModelUpdateName[];
extern const char kImeUsEnglishModelUpdateDescription[];

extern const char kJellyColorsName[];
extern const char kJellyColorsDescription[];

extern const char kCrosComponentsName[];
extern const char kCrosComponentsDescription[];

extern const char kLacrosAvailabilityIgnoreName[];
extern const char kLacrosAvailabilityIgnoreDescription[];

extern const char kLacrosStabilityName[];
extern const char kLacrosStabilityDescription[];

extern const char kLacrosSelectionName[];
extern const char kLacrosSelectionDescription[];
extern const char kLacrosSelectionRootfsDescription[];
extern const char kLacrosSelectionStatefulDescription[];

extern const char kLacrosSelectionPolicyIgnoreName[];
extern const char kLacrosSelectionPolicyIgnoreDescription[];

extern const char kLacrosWaylandLoggingName[];
extern const char kLacrosWaylandLoggingDescription[];

extern const char kLanguagePacksInSettingsName[];
extern const char kLanguagePacksInSettingsDescription[];

extern const char kUseMlServiceForNonLongformHandwritingOnAllBoardsName[];
extern const char
    kUseMlServiceForNonLongformHandwritingOnAllBoardsDescription[];

extern const char kLauncherContinueSectionWithRecentsName[];
extern const char kLauncherContinueSectionWithRecentsDescription[];

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

extern const char kMahiName[];
extern const char kMahiDescription[];

extern const char kMahiDebuggingName[];
extern const char kMahiDebuggingDescription[];

extern const char kMahiName[];
extern const char kMahiDescription[];

extern const char kMahiFeatureKeyName[];
extern const char kMahiFeatureKeyDescription[];

extern const char kMediaAppPdfMahiName[];
extern const char kMediaAppPdfMahiDescription[];

extern const char kMeteredShowToggleName[];
extern const char kMeteredShowToggleDescription[];

extern const char kMicrophoneMuteSwitchDeviceName[];
extern const char kMicrophoneMuteSwitchDeviceDescription[];

extern const char kMultiCalendarSupportName[];
extern const char kMultiCalendarSupportDescription[];

extern const char kMultiZoneRgbKeyboardName[];
extern const char kMultiZoneRgbKeyboardDescription[];

extern const char kNotificationWidthIncreaseName[];
extern const char kNotificationWidthIncreaseDescription[];

extern const char kEnableNearbyBleV2Name[];
extern const char kEnableNearbyBleV2Description[];

extern const char kEnableNearbyBleV2ExtendedAdvertisingName[];
extern const char kEnableNearbyBleV2ExtendedAdvertisingDescription[];

extern const char kEnableNearbyBleV2GattServerName[];
extern const char kEnableNearbyBleV2GattServerDescription[];

extern const char kEnableNearbyBluetoothClassicAdvertisingName[];
extern const char kEnableNearbyBluetoothClassicAdvertisingDescription[];

extern const char kEnableNearbyMdnsName[];
extern const char kEnableNearbyMdnsDescription[];

extern const char kEnableNearbyWebRtcName[];
extern const char kEnableNearbyWebRtcDescription[];

extern const char kEnableNearbyWifiDirectName[];
extern const char kEnableNearbyWifiDirectDescription[];

extern const char kEnableNearbyWifiLanName[];
extern const char kEnableNearbyWifiLanDescription[];

extern const char kNearbyPresenceName[];
extern const char kNearbyPresenceDescription[];

extern const char kNotificationsIgnoreRequireInteractionName[];
extern const char kNotificationsIgnoreRequireInteractionDescription[];

extern const char kOnDeviceAppControlsName[];
extern const char kOnDeviceAppControlsDescription[];

extern const char kOsSettingsRevampWayfindingName[];
extern const char kOsSettingsRevampWayfindingDescription[];

extern const char kPcieBillboardNotificationName[];
extern const char kPcieBillboardNotificationDescription[];

extern const char kPerformantSplitViewResizing[];
extern const char kPerformantSplitViewResizingDescription[];

extern const char kPhoneHubCallNotificationName[];
extern const char kPhoneHubCallNotificationDescription[];

extern const char kPreferDcheckName[];
extern const char kPreferDcheckDescription[];

extern const char kPrintingPpdChannelName[];
extern const char kPrintingPpdChannelDescription[];

extern const char kPrintPreviewCrosAppName[];
extern const char kPrintPreviewCrosAppDescription[];

extern const char kProductivityLauncherName[];
extern const char kProductivityLauncherDescription[];

extern const char kProductivityLauncherImageSearchName[];
extern const char kProductivityLauncherImageSearchDescription[];

extern const char kProjectorAppDebugName[];
extern const char kProjectorAppDebugDescription[];

extern const char kProjectorGm3Name[];
extern const char kProjectorGm3Description[];

extern const char kProjectorServerSideSpeechRecognitionName[];
extern const char kProjectorServerSideSpeechRecognitionDescription[];

extern const char kProjectorServerSideUsmName[];
extern const char kProjectorServerSideUsmDescription[];

extern const char kReleaseNotesNotificationAllChannelsName[];
extern const char kReleaseNotesNotificationAllChannelsDescription[];

extern const char kReleaseNotesNotificationAlwaysEligibleName[];
extern const char kReleaseNotesNotificationAlwaysEligibleDescription[];

extern const char kRenderArcNotificationsByChromeName[];
extern const char kRenderArcNotificationsByChromeDescription[];

extern const char kRunVideoCaptureServiceInBrowserProcessName[];
extern const char kRunVideoCaptureServiceInBrowserProcessDescription[];

extern const char kArcWindowPredictorName[];
extern const char kArcWindowPredictorDescription[];

extern const char kScalableIphDebugName[];
extern const char kScalableIphDebugDescription[];

extern const char kSealKeyName[];
extern const char kSealKeyDescription[];

extern const char kSeaPenName[];
extern const char kSeaPenDescription[];

extern const char kSeaPenEnterpriseName[];
extern const char kSeaPenEnterpriseDescription[];

extern const char kShelfAutoHideSeparationName[];
extern const char kShelfAutoHideSeparationDescription[];

extern const char kShimlessRMAAproUpdateRootfsName[];
extern const char kShimlessRMAAproUpdateRootfsDescription[];

extern const char kShimlessRMAOsUpdateName[];
extern const char kShimlessRMAOsUpdateDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];

extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kSnapGroupsName[];
extern const char kSnapGroupsDescription[];

extern const char kMediaDynamicCgroupName[];
extern const char kMediaDynamicCgroupDescription[];

extern const char kMissiveStorageName[];
extern const char kMissiveStorageDescription[];

extern const char kShowBluetoothDebugLogToggleName[];
extern const char kShowBluetoothDebugLogToggleDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSnoopingProtectionName[];
extern const char kSnoopingProtectionDescription[];

extern const char kContinuousOverviewScrollAnimationName[];
extern const char kContinuousOverviewScrollAnimationDescription[];

extern const char kSparkyName[];
extern const char kSparkyDescription[];

extern const char kSparkyFeatureKeyName[];
extern const char kSparkyFeatureKeyDescription[];

extern const char kSparkyServerUrlName[];
extern const char kSparkyServerUrlDescription[];

extern const char kSpectreVariant2MitigationName[];
extern const char kSpectreVariant2MitigationDescription[];

extern const char kSystemJapanesePhysicalTypingName[];
extern const char kSystemJapanesePhysicalTypingDescription[];

extern const char kSupportF11AndF12ShortcutsName[];
extern const char kSupportF11AndF12ShortcutsDescription[];

extern const char kTerminalDevName[];
extern const char kTerminalDevDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTilingWindowResizeName[];
extern const char kTilingWindowResizeDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kTouchscreenMappingName[];
extern const char kTouchscreenMappingDescription[];

extern const char kTrafficCountersEnabledName[];
extern const char kTrafficCountersEnabledDescription[];

extern const char kTrafficCountersForWiFiTestingName[];
extern const char kTrafficCountersForWiFiTestingDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUploadOfficeToCloudName[];
extern const char kUploadOfficeToCloudDescription[];

extern const char kUpstreamTrustedReportsFirmwareName[];
extern const char kUpstreamTrustedReportsFirmwareDescription[];

extern const char kUseAnnotatedAccountIdName[];
extern const char kUseAnnotatedAccountIdDescription[];

extern const char kUseFakeDeviceForMediaStreamName[];
extern const char kUseFakeDeviceForMediaStreamDescription[];

extern const char kUseLegacyDHCPCDName[];
extern const char kUseLegacyDHCPCDDescription[];

extern const char kVcDlcUiName[];
extern const char kVcDlcUiDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardDisabledName[];
extern const char kVirtualKeyboardDisabledDescription[];

extern const char kVirtualKeyboardGlobalEmojiPreferencesName[];
extern const char kVirtualKeyboardGlobalEmojiPreferencesDescription[];

extern const char kWakeOnWifiAllowedName[];
extern const char kWakeOnWifiAllowedDescription[];

extern const char kWelcomeExperienceName[];
extern const char kWelcomeExperienceDescription[];

extern const char kWelcomeExperienceTestUnsupportedDevicesName[];
extern const char kWelcomeExperienceTestUnsupportedDevicesDescription[];

extern const char kWelcomeTourName[];
extern const char kWelcomeTourDescription[];

extern const char kWelcomeTourForceUserEligibilityName[];
extern const char kWelcomeTourForceUserEligibilityDescription[];

extern const char kWifiConnectMacAddressRandomizationName[];
extern const char kWifiConnectMacAddressRandomizationDescription[];

extern const char kWifiConcurrencyName[];
extern const char kWifiConcurrencyDescription[];

extern const char kWifiDirectName[];
extern const char kWifiDirectDescription[];

extern const char kWindowSplittingName[];
extern const char kWindowSplittingDescription[];

extern const char kLauncherGameSearchName[];
extern const char kLauncherGameSearchDescription[];

extern const char kLauncherKeyShortcutInBestMatchName[];
extern const char kLauncherKeyShortcutInBestMatchDescription[];

extern const char kLauncherKeywordExtractionScoring[];
extern const char kLauncherKeywordExtractionScoringDescription[];

extern const char kLauncherLocalImageSearchName[];
extern const char kLauncherLocalImageSearchDescription[];

extern const char kLauncherLocalImageSearchOnCoreName[];
extern const char kLauncherLocalImageSearchOnCoreDescription[];

extern const char kLauncherLocalImageSearchConfidenceName[];
extern const char kLauncherLocalImageSearchConfidenceDescription[];

extern const char kLauncherLocalImageSearchRelevanceName[];
extern const char kLauncherLocalImageSearchRelevanceDescription[];

extern const char kLauncherLocalImageSearchOcrName[];
extern const char kLauncherLocalImageSearchOcrDescription[];

extern const char kLauncherLocalImageSearchIcaName[];
extern const char kLauncherLocalImageSearchIcaDescription[];

extern const char kLauncherFuzzyMatchForOmniboxName[];
extern const char kLauncherFuzzyMatchForOmniboxDescription[];

extern const char kLauncherSearchControlName[];
extern const char kLauncherSearchControlDescription[];

extern const char kLauncherNudgeSessionResetName[];
extern const char kLauncherNudgeSessionResetDescription[];

extern const char kLauncherSystemInfoAnswerCardsName[];
extern const char kLauncherSystemInfoAnswerCardsDescription[];

extern const char kMacAddressRandomizationName[];
extern const char kMacAddressRandomizationDescription[];

extern const char kSysUiShouldHoldbackDriveIntegrationName[];
extern const char kSysUiShouldHoldbackDriveIntegrationDescription[];

extern const char kSysUiShouldHoldbackFocusModeName[];
extern const char kSysUiShouldHoldbackFocusModeDescription[];

extern const char kSysUiShouldHoldbackForestName[];
extern const char kSysUiShouldHoldbackForestDescription[];

extern const char kSysUiShouldHoldbackGifRecordingName[];
extern const char kSysUiShouldHoldbackGifRecordingDescription[];

extern const char kSysUiShouldHoldbackTaskManagementName[];
extern const char kSysUiShouldHoldbackTaskManagementDescription[];

extern const char kTetheringExperimentalFunctionalityName[];
extern const char kTetheringExperimentalFunctionalityDescription[];

// Prefer keeping this section sorted to adding new declarations down here.

#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kExperimentalWebAppStoragePartitionIsolationName[];
extern const char kExperimentalWebAppStoragePartitionIsolationDescription[];

extern const char kBlinkExtensionName[];
extern const char kBlinkExtensionDescription[];

extern const char kBlinkExtensionDiagnosticsName[];
extern const char kBlinkExtensionDiagnosticsDescription[];
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
extern const char kGetAllScreensMediaName[];
extern const char kGetAllScreensMediaDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kAddPrinterViaPrintscanmgrName[];
extern const char kAddPrinterViaPrintscanmgrDescription[];

extern const char kCrosAppsBackgroundEventHandlingName[];
extern const char kCrosAppsBackgroundEventHandlingDescription[];

extern const char kRunOnOsLoginName[];
extern const char kRunOnOsLoginDescription[];

extern const char kPreventCloseName[];
extern const char kPreventCloseDescription[];

extern const char kFileSystemAccessGetCloudIdentifiersName[];
extern const char kFileSystemAccessGetCloudIdentifiersDescription[];

extern const char kCrOSDspBasedAecAllowedName[];
extern const char kCrOSDspBasedAecAllowedDescription[];

extern const char kCrOSDspBasedNsAllowedName[];
extern const char kCrOSDspBasedNsAllowedDescription[];

extern const char kCrOSDspBasedAgcAllowedName[];
extern const char kCrOSDspBasedAgcAllowedDescription[];

extern const char kCrosMallName[];
extern const char kCrosMallDescription[];

extern const char kCrosMallSwaName[];
extern const char kCrosMallSwaDescription[];

extern const char kCrosMallUrlName[];
extern const char kCrosMallUrlDescription[];

extern const char kCrosPrivacyHubName[];
extern const char kCrosPrivacyHubDescription[];

extern const char kCrosPrivacyHubAppPermissionsName[];
extern const char kCrosPrivacyHubAppPermissionsDescription[];

extern const char kCrosPrivacyHubAppPermissionsV2Name[];
extern const char kCrosPrivacyHubAppPermissionsV2Description[];

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

extern const char kLacrosColorManagementName[];
extern const char kLacrosColorManagementDescription[];

extern const char kLockedModeName[];
extern const char kLockedModeDescription[];

extern const char kOneGroupPerRendererName[];
extern const char kOneGroupPerRendererDescription[];

extern const char kPlatformKeysAesEncryptionName[];
extern const char kPlatformKeysAesEncryptionDescription[];

extern const char kPrintPreviewCrosPrimaryName[];
extern const char kPrintPreviewCrosPrimaryDescription[];

extern const char kDisableQuickAnswersV2TranslationName[];
extern const char kDisableQuickAnswersV2TranslationDescription[];

extern const char kQuickAnswersRichCardName[];
extern const char kQuickAnswersRichCardDescription[];

extern const char kQuickAnswersMaterialNextUIName[];
extern const char kQuickAnswersMaterialNextUIDescription[];

extern const char kSyncChromeOSExplicitPassphraseSharingName[];
extern const char kSyncChromeOSExplicitPassphraseSharingDescription[];

extern const char kQuickOfficeForceFileDownloadName[];
extern const char kQuickOfficeForceFileDownloadDescription[];

extern const char kWebPrintingApiName[];
extern const char kWebPrintingApiDescription[];

extern const char kIgnoreUiGainsName[];
extern const char kIgnoreUiGainsDescription[];

extern const char kShowForceRespectUiGainsToggleName[];
extern const char kShowForceRespectUiGainsToggleDescription[];

extern const char kCrOSSystemVoiceIsolationOptionName[];
extern const char kCrOSSystemVoiceIsolationOptionDescription[];

extern const char kAudioFlexibleLoopbackForSystemLoopbackName[];
extern const char kAudioFlexibleLoopbackForSystemLoopbackDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
extern const char kChromeOSHWVBREncodingName[];
extern const char kChromeOSHWVBREncodingDescription[];
#if defined(ARCH_CPU_ARM_FAMILY)
extern const char kUseGLForScalingName[];
extern const char kUseGLForScalingDescription[];
extern const char kPreferGLImageProcessorName[];
extern const char kPreferGLImageProcessorDescription[];
extern const char kPreferSoftwareMT21Name[];
extern const char kPreferSoftwareMT21Description[];
extern const char kEnableProtectedVulkanDetilingName[];
extern const char kEnableProtectedVulkanDetilingDescription[];
extern const char kEnableArmHwdrm10bitOverlaysName[];
extern const char kEnableArmHwdrm10bitOverlaysDescription[];
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
extern const char kEnableArmHwdrmName[];
extern const char kEnableArmHwdrmDescription[];
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#endif  // defined(ARCH_CPU_ARM_FAMILY
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION) && defined(ARCH_CPU_ARM_FAMILY)
extern const char kV4L2FlatStatefulVideoDecoderName[];
extern const char kV4L2FlatStatefulVideoDecoderDescription[];
#endif

// Linux ---------------------------------------------------------------------

#if BUILDFLAG(IS_LINUX)
extern const char kOzonePlatformHintChoiceDefault[];
extern const char kOzonePlatformHintChoiceAuto[];
extern const char kOzonePlatformHintChoiceX11[];
extern const char kOzonePlatformHintChoiceWayland[];

extern const char kOzonePlatformHintName[];
extern const char kOzonePlatformHintDescription[];

extern const char kPulseaudioLoopbackForCastName[];
extern const char kPulseaudioLoopbackForCastDescription[];
extern const char kPulseaudioLoopbackForScreenShareName[];
extern const char kPulseaudioLoopbackForScreenShareDescription[];

extern const char kSimplifiedTabDragUIName[];
extern const char kSimplifiedTabDragUIDescription[];

extern const char kWaylandPerWindowScalingName[];
extern const char kWaylandPerWindowScalingDescription[];

extern const char kWaylandTextInputV3Name[];
extern const char kWaylandTextInputV3Description[];

extern const char kWaylandUiScalingName[];
extern const char kWaylandUiScalingDescription[];
#endif  // BUILDFLAG(IS_LINUX)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)
extern const char kSideSearchName[];
extern const char kSideSearchDescription[];

extern const char kSearchWebInSidePanelName[];
extern const char kSearchWebInSidePanelDescription[];
#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kZeroCopyVideoCaptureName[];
extern const char kZeroCopyVideoCaptureDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
extern const char kWebShareName[];
extern const char kWebShareDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
extern const char kWebBluetoothConfirmPairingSupportName[];
extern const char kWebBluetoothConfirmPairingSupportDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
extern const char kSkipUndecryptablePasswordsName[];
extern const char kSkipUndecryptablePasswordsDescription[];

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kCupsIppPrintingBackendName[];
extern const char kCupsIppPrintingBackendDescription[];
#endif  // BUILDFLAG(ENABLE_PRINTING)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kScreenlockReauthCardName[];
extern const char kScreenlockReauthCardDescription[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kScreenlockReauthPromoCardName[];
extern const char kScreenlockReauthPromoCardDescription[];
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
extern const char kFollowingFeedSidepanelName[];
extern const char kFollowingFeedSidepanelDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        //  BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
extern const char kTaskManagerClankName[];
extern const char kTaskManagerClankDescription[];
#else
extern const char kTaskManagerDesktopRefreshName[];
extern const char kTaskManagerDesktopRefreshDescription[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
extern const char kEnableNetworkServiceSandboxName[];
extern const char kEnableNetworkServiceSandboxDescription[];

extern const char kUseOutOfProcessVideoDecodingName[];
extern const char kUseOutOfProcessVideoDecodingDescription[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
extern const char kChromeWideEchoCancellationName[];
extern const char kChromeWideEchoCancellationDescription[];
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

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

#if BUILDFLAG(ENABLE_PDF)
extern const char kAccessiblePDFFormName[];
extern const char kAccessiblePDFFormDescription[];

extern const char kPdfCr23Name[];
extern const char kPdfCr23Description[];

#if BUILDFLAG(ENABLE_PDF_INK2)
extern const char kPdfInk2Name[];
extern const char kPdfInk2Description[];
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

extern const char kPdfOopifName[];
extern const char kPdfOopifDescription[];

extern const char kPdfPortfolioName[];
extern const char kPdfPortfolioDescription[];

extern const char kPdfUseSkiaRendererName[];
extern const char kPdfUseSkiaRendererDescription[];
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_VR)
extern const char kWebXrInternalsName[];
extern const char kWebXrInternalsDescription[];
#endif  // #if defined(ENABLE_VR)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kWebUITabStripFlagId[];
extern const char kWebUITabStripName[];
extern const char kWebUITabStripDescription[];

extern const char kWebUITabStripContextMenuAfterTapName[];
extern const char kWebUITabStripContextMenuAfterTapDescription[];
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
extern const char kElasticOverscrollName[];
extern const char kElasticOverscrollDescription[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
extern const char kElementCaptureName[];
extern const char kElementCaptureDescription[];
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC)
extern const char kUIDebugToolsName[];
extern const char kUIDebugToolsDescription[];
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
extern const char kWebrtcPipeWireCameraName[];
extern const char kWebrtcPipeWireCameraDescription[];
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

#if BUILDFLAG(ENABLE_COMPOSE)
extern const char kComposeId[];
extern const char kComposeName[];
extern const char kComposeDescription[];

extern const char kComposeTextSelectionName[];
extern const char kComposeTextSelectionDescription[];

extern const char kComposeProactiveNudgeName[];
extern const char kComposeProactiveNudgeDescription[];

extern const char kComposePoliteNudgeName[];
extern const char kComposePoliteNudgeDescription[];

extern const char kComposeSegmentationPromotionName[];
extern const char kComposeSegmentationPromotionDescription[];

extern const char kComposeSelectionNudgeName[];
extern const char kComposeSelectionNudgeDescription[];

extern const char kComposeUpfrontInputModesName[];
extern const char kComposeUpfrontInputModesDescription[];
#endif  // BUILDFLAG(ENABLE_COMPOSE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kThirdPartyProfileManagementName[];
extern const char kThirdPartyProfileManagementDescription[];

extern const char kOidcAuthProfileManagementName[];
extern const char kOidcAuthProfileManagementDescription[];

extern const char kUnoDesktopName[];
extern const char kUnoDesktopDescription[];

extern const char kExplicitBrowserSigninUIOnDesktopName[];
extern const char kExplicitBrowserSigninUIOnDesktopDescription[];

extern const char kDesktopPWAsUserLinkCapturingScopeExtensionsName[];
extern const char kDesktopPWAsUserLinkCapturingScopeExtensionsDescription[];

extern const char kSyncEnableBookmarksInTransportModeName[];
extern const char kSyncEnableBookmarksInTransportModeDescription[];

extern const char kSyncEnableContactInfoDataTypeInTransportModeName[];
extern const char kSyncEnableContactInfoDataTypeInTransportModeDescription[];

extern const char kEnableGenericOidcAuthProfileManagementName[];
extern const char kEnableGenericOidcAuthProfileManagementDescription[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
extern const char kEnableBuiltinHlsName[];
extern const char kEnableBuiltinHlsDescription[];
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kProfilesReorderingName[];
extern const char kProfilesReorderingDescription[];
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const char kBatchUploadDesktopName[];
extern const char kBatchUploadDesktopDescription[];
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
extern const char kEnableBoundSessionCredentialsName[];
extern const char kEnableBoundSessionCredentialsDescription[];

extern const char
    kEnableBoundSessionCredentialsSoftwareKeysForManualTestingName[];
extern const char
    kEnableBoundSessionCredentialsSoftwareKeysForManualTestingDescription[];
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
extern const char kEnableStandardBoundSessionCredentialsName[];
extern const char kEnableStandardBoundSessionCredentialsDescription[];
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
extern const char kEnableCertManagementV2UIName[];
extern const char kEnableCertManagementV2UIDescription[];
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kOverlayScrollbarsOSSettingsName[];
extern const char kOverlayScrollbarsOSSettingsDescription[];
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kSupervisedProfileHideGuestName[];
extern const char kSupervisedProfileHideGuestDescription[];

extern const char kSupervisedProfileSafeSearchName[];
extern const char kSupervisedProfileSafeSearchDescription[];

extern const char kSupervisedProfileReauthForYouTubeName[];
extern const char kSupervisedProfileReauthForYouTubeDescription[];

extern const char kSupervisedProfileReauthForBlockedSiteName[];
extern const char kSupervisedProfileReauthForBlockedSiteDescription[];

extern const char kSupervisedProfileSubframeReauthName[];
extern const char kSupervisedProfileSubframeReauthDescription[];

extern const char kSupervisedProfileFilteringFallbackName[];
extern const char kSupervisedProfileFilteringFallbackDescription[];

extern const char kSupervisedProfileCustomStringsName[];
extern const char kSupervisedProfileCustomStringsDescription[];

extern const char kSupervisedProfileSignInIphName[];
extern const char kSupervisedProfileSignInIphDescription[];

extern const char kSupervisedProfileShowKiteBadgeName[];
extern const char kSupervisedProfileShowKiteBadgeDescription[];
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
