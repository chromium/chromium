// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/flags/jni_headers/ChromeFeatureList_jni.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/supervised_user/supervised_user_features/supervised_user_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/reactions/core/reactions_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/contextual_search_field_trial.h"
#include "components/download/public/common/download_features.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/messages/android/messages_feature.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/query_tiles/switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/base/features.h"
#include "components/webapps/browser/features.h"
#include "content/public/common/content_features.h"
#include "device/fido/features.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &autofill::features::kAutofillAddressProfileSavePromptNicknameSupport,
    &autofill::features::kAutofillCreditCardAuthentication,
    &autofill::features::kAutofillEnableRankingFormula,
    &autofill::features::kAutofillEnableManualFallbackForVirtualCards,
    &autofill::features::kAutofillKeyboardAccessory,
    &autofill::features::kAutofillManualFallbackAndroid,
    &autofill::features::kAutofillRefreshStyleAndroid,
    &autofill::features::kAutofillEnableSupportForHonorificPrefixes,
    &autofill::features::kAutofillEnableUpdateVirtualCardEnrollment,
    &autofill::features::kAutofillEnableVirtualCardMetadata,
    &autofill::features::kAutofillEnableCardProductName,
    &blink::features::kForceWebContentsDarkMode,
    &blink::features::kOSKResizesVisualViewportByDefault,
    &blink::features::kPrerender2,
    &commerce::kCommerceMerchantViewer,
    &commerce::kCommercePriceTracking,
    &commerce::kShoppingList,
    &commerce::kShoppingListEnableDesyncResolution,
    &commerce::kShoppingPDPMetrics,
    &content_creation::kLightweightReactions,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &download::features::kDownloadAutoResumptionNative,
    &download::features::kSmartSuggestionForLargeDownloads,
    &download::features::kUseDownloadOfflineContentProvider,
    &embedder_support::kShowTrustedPublisherURL,
    &features::kPWAsDefaultOfflinePage,
    &features::kAnonymousUpdateChecks,
    &features::kEarlyLibraryLoad,
    &features::kGenericSensorExtraClasses,
    &features::kAsyncSensorCalls,
    &features::kBackForwardCache,
    &features::kHttpsOnlyMode,
    &features::kMetricsSettingsAndroid,
    &features::kNetworkServiceInProcess,
    &shared_highlighting::kPreemptiveLinkToTextGeneration,
    &shared_highlighting::kSharedHighlightingAmp,
    &features::kElasticOverscroll,
    &features::kPrivacyGuideAndroid,
    &features::kPushMessagingDisallowSenderIDs,
    &features::kPwaUpdateDialogForIcon,
    &features::kPwaUpdateDialogForName,
    &features::kQuietNotificationPrompts,
    &features::kRequestDesktopSiteForTablets,
    &features::kToolbarUseHardwareBitmapDraw,
    &features::kWebNfc,
    &features::kIncognitoDownloadsWarning,
    &features::kIncognitoNtpRevamp,
    &feature_engagement::kEnableIPH,
    &feature_engagement::kEnableAutomaticSnooze,
    &feature_engagement::kIPHNewTabPageHomeButtonFeature,
    &feature_engagement::kIPHSnooze,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &feature_engagement::kUseClientConfigIPH,
    &feature_guide::features::kFeatureNotificationGuide,
    &feature_guide::features::kSkipCheckForLowEngagedUsers,
    &feed::kClientGoodVisits,
    &feed::kFeedBackToTop,
    &feed::kFeedClearImageMemoryCache,
    &feed::kFeedHeaderStickToTop,
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedInteractiveRefresh,
    &feed::kFeedLoadingPlaceholder,
    &feed::kFeedNoViewCache,
    &feed::kFeedPerformanceStudy,
    &feed::kFeedReplaceAll,
    &feed::kFeedShowSignInCommand,
    &feed::kInterestFeedContentSuggestions,
    &feed::kInterestFeedV1ClicksAndViewsConditionalUpload,
    &feed::kInterestFeedV2,
    &feed::kInterestFeedV2Autoplay,
    &feed::kInterestFeedV2Hearts,
    &feed::kShareCrowButton,
    &feed::kReliabilityLogging,
    &feed::kWebFeed,
    &feed::kWebFeedAwareness,
    &feed::kWebFeedOnboarding,
    &feed::kWebFeedSort,
    &feed::kXsurfaceMetricsReporting,
    &history::kOrganicRepeatableQueries,
    &history_clusters::internal::kJourneys,
    &kAdaptiveButtonInTopToolbar,
    &kAdaptiveButtonInTopToolbarCustomizationV2,
    &kAddEduAccountFromAccountSettingsForSupervisedUsers,
    &kAddToHomescreenIPH,
    &kAllowNewIncognitoTabIntents,
    &kAndroidScrollOptimizations,
    &kAndroidSearchEngineChoiceNotification,
    &kAssistantConsentSimplifiedText,
    &kAssistantConsentV2,
    &kAssistantIntentExperimentId,
    &kAssistantIntentPageUrl,
    &kAssistantIntentTranslateInfo,
    &kAssistantNonPersonalizedVoiceSearch,
    &kAppMenuMobileSiteOption,
    &kAppToWebAttribution,
    &kBackgroundThreadPool,
    &kCastDeviceFilter,
    &kClearOmniboxFocusAfterNavigation,
    &kCloseTabSuggestions,
    &kCriticalPersistedTabData,
    &kCommerceCoupons,
    &kCCTBackgroundTab,
    &kCCTBrandTransparency,
    &kCCTClientDataHeader,
    &kCCTFeatureUsage,
    &kCCTIncognito,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTIntentFeatureOverrides,
    &kCCTNewDownloadTab,
    &kCCTPackageNameRecording,
    &kCCTPostMessageAPI,
    &kCCTPrefetchDelayShowOnStart,
    &kCCTRealTimeEngagementSignals,
    &kCCTRedirectPreconnect,
    &kCCTRemoveRemoteViewIds,
    &kCCTReportParallelRequestStatus,
    &kCCTResizable90MaximumHeight,
    &kCCTResizableAllowResizeByUserGesture,
    &kCCTResizableAlwaysShowNavBarButtons,
    &kCCTResizableForFirstParties,
    &kCCTResizableForThirdParties,
    &kCCTResizableSideSheet,
    &kCCTRetainingStateInMemory,
    &kCCTResourcePrefetch,
    &kCCTShowAboutBlankUrl,
    &kCCTToolbarCustomizations,
    &kDiscardOccludedBitmaps,
    &kDontAutoHideBrowserControls,
    &kCacheDeprecatedSystemLocationSetting,
    &kChromeNewDownloadTab,
    &kChromeShareLongScreenshot,
    &kChromeSharingHub,
    &kChromeSharingHubLaunchAdjacent,
    &kChromeSurveyNextAndroid,
    &kCommandLineOnNonRooted,
    &kConditionalTabStripAndroid,
    &kContextMenuEnableLensShoppingAllowlist,
    &kContextMenuGoogleLensChip,
    &kContextMenuSearchWithGoogleLens,
    &kContextMenuShopWithGoogleLens,
    &kContextMenuSearchAndShopWithGoogleLens,
    &kContextMenuTranslateWithGoogleLens,
    &kContextMenuPopupForAllScreenSizes,
    &kContextualSearchDebug,
    &kContextualSearchDelayedIntelligence,
    &kContextualSearchDisableOnlineDetection,
    &kContextualSearchForceCaption,
    &kContextualSearchSuppressShortView,
    &kContextualSearchThinWebViewImplementation,
    &kDeferKeepScreenOnDuringGesture,
    &kDirectActions,
    &kDuetTabStripIntegrationAndroid,
    &kExperimentsForAgsa,
    &kExploreSites,
    &kFocusOmniboxInIncognitoTabIntents,
    &kFoldableJankFix,
    &kGridTabSwitcherForTablets,
    &kHandleMediaIntents,
    &kHideNonDisplayableAccountEmail,
    &kImmersiveUiMode,
    &kIncognitoReauthenticationForAndroid,
    &kIncognitoScreenshot,
    &kInfobarScrollOptimization,
    &kInstanceSwitcher,
    &kInstantStart,
    &kIsVoiceSearchEnabledCache,
    &kLensCameraAssistedSearch,
    &kLensOnQuickActionSearchWidget,
    &kNewTabPageTilesTitleWrapAround,
    &kNewWindowAppMenu,
    &kNotificationPermissionVariant,
    &kPageAnnotationsService,
    &kBookmarksImprovedSaveFlow,
    &kBookmarksRefresh,
    &kBackGestureRefactorAndroid,
    &kOmahaMinSdkVersionAndroid,
    &kOmniboxModernizeVisualUpdate,
    &kOpaqueOriginForIncomingIntents,
    &kOptimizeGeolocationHeaderGeneration,
    &kOptimizeLayoutsForPullRefresh,
    &kPostTaskFocusTab,
    &kProbabilisticCryptidRenderer,
    &kReachedCodeProfiler,
    &kReaderModeInCCT,
    &kRecordSuppressionMetrics,
    &kReengagementNotification,
    &kRelatedSearches,
    &kRelatedSearchesInBar,
    &kRelatedSearchesUi,
    &kReportParentalControlSitesChild,
    &kRequestDesktopSiteDefaults,
    &kRequestDesktopSiteDefaultsControl,
    &kRequestDesktopSiteDefaultsControlSynthetic,
    &kRequestDesktopSiteDefaultsSynthetic,
    &kRequestDesktopSiteOptInControlSynthetic,
    &kRequestDesktopSiteOptInSynthetic,
    &kRequestDesktopSiteDefaultsDowngrade,
    &kSafeModeForCachedFlags,
    &kSearchEnginePromoExistingDevice,
    &kSearchEnginePromoExistingDeviceV2,
    &kSearchEnginePromoNewDevice,
    &kSearchEnginePromoNewDeviceV2,
    &kShowScrollableMVTOnNTPAndroid,
    &kFeedPositionAndroid,
    &kSearchResumptionModuleAndroid,
    &kSpannableInlineAutocomplete,
    &kSpecialLocaleWrapper,
    &kSpecialUserDecision,
    &kSuppressToolbarCaptures,
    &kSplitCompositorTask,
    &kStoreHoursAndroid,
    &kSwapPixelFormatToFixConvertFromTranslucent,
    &kTabEngagementReportingAndroid,
    &kTabGroupsAndroid,
    &kTabGroupsContinuationAndroid,
    &kTabGroupsUiImprovementsAndroid,
    &kTabGroupsForTablets,
    &kDiscoverFeedMultiColumn,
    &kTabStripRedesign,
    &kTabGridLayoutAndroid,
    &kTabReparenting,
    &kTabSelectionEditorV2,
    &kTabStripImprovements,
    &kTabSwitcherOnReturn,
    &kTabToGTSAnimation,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kToolbarMicIphAndroid,
    &kToolbarPhoneOptimizations,
    &kToolbarScrollAblationAndroid,
    &kTrustedWebActivityPostMessage,
    &kTrustedWebActivityQualityEnforcement,
    &kTrustedWebActivityQualityEnforcementForced,
    &kTrustedWebActivityQualityEnforcementWarning,
    &kStartSurfaceAndroid,
    &kStartSurfaceReturnTime,
    &kStartSurfaceRefactor,
    &kStartSurfaceDisabledFeedImprovement,
    &kUmaBackgroundSessions,
    &kUpdateHistoryEntryPointsInIncognito,
    &kUpdateNotificationScheduleServiceImmediateShowOption,
    &kUseLibunwindstackNativeUnwinderAndroid,
    &kVoiceSearchAudioCapturePolicy,
    &kVoiceButtonInTopToolbar,
    &kVrBrowsingFeedback,
    &kWebOtpCrossDeviceSimpleString,
    &content_creation::kWebNotesDynamicTemplates,
    &content_creation::kWebNotesStylizeEnabled,
    &kWebApkAllowIconUpdate,
    &kWebApkInstallService,
    &kWebApkTrampolineOnInitialIntent,
    &features::kDnsOverHttps,
    &notifications::features::kUseChimeAndroidSdk,
    &paint_preview::kPaintPreviewDemo,
    &language::kAppLanguagePrompt,
    &language::kAppLanguagePromptULP,
    &language::kCctAutoTranslate,
    &language::kDetailedLanguageSettings,
    &language::kExplicitLanguageAsk,
    &language::kForceAppLanguagePrompt,
    &language::kTranslateAssistContent,
    &language::kTranslateIntent,
    &media_router::kCafMRPDeferredDiscovery,
    &messages::kMessagesForAndroidChromeSurvey,
    &messages::kMessagesForAndroidInfrastructure,
    &messages::kMessagesForAndroidReaderMode,
    &messages::kMessagesForAndroidSaveCard,
    &offline_pages::kOfflineIndicatorFeature,
    &offline_pages::kOfflinePagesCTFeature,  // See crbug.com/620421.
    &offline_pages::kOfflinePagesDescriptiveFailStatusFeature,
    &offline_pages::kOfflinePagesDescriptivePendingStatusFeature,
    &offline_pages::kOfflinePagesLivePageSharingFeature,
    &offline_pages::kPrefetchingOfflinePagesFeature,
    &omnibox::kAdaptiveSuggestionsCount,
    &omnibox::kAndroidAuxiliarySearch,
    &omnibox::kMostVisitedTiles,
    &omnibox::kMostVisitedTilesTitleWrapAround,
    &omnibox::kOmniboxAssistantVoiceSearch,
    &omnibox::kOmniboxMatchToolbarAndStatusBarColor,
    &omnibox::kOmniboxRemoveExcessiveRecycledViewClearCalls,
    &omnibox::kOmniboxMostVisitedTilesAddRecycledViewPool,
    &omnibox::kOmniboxOnClobberFocusTypeOnContent,
    &omnibox::kSuggestionAnswersColorReverse,
    &omnibox::kUpdatedConnectionSecurityIndicators,
    &optimization_guide::features::kPushNotifications,
    &page_info::kPageInfoAboutThisSiteEn,
    &page_info::kPageInfoAboutThisSiteMoreInfo,
    &page_info::kPageInfoAboutThisSiteNonEn,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::kEnablePasswordsAccountStorage,
    &password_manager::features::kLeakDetectionUnauthenticated,
    &password_manager::features::kRecoverFromNeverSaveAndroid,
    &password_manager::features::kUnifiedCredentialManagerDryRun,
    &password_manager::features::kUnifiedPasswordManagerAndroid,
    &password_manager::features::kUnifiedPasswordManagerAndroidBranding,
    &password_manager::features::kUnifiedPasswordManagerErrorMessages,
    &password_manager::features::kPasswordEditDialogWithDetails,
    &privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
    &privacy_sandbox::kPrivacySandboxSettings3,
    &privacy_sandbox::kPrivacySandboxSettings4,
    &query_tiles::features::kQueryTiles,
    &query_tiles::features::kQueryTilesInNTP,
    &query_tiles::features::kQueryTilesOnStart,
    &query_tiles::features::kQueryTilesSegmentation,
    &reading_list::switches::kReadLater,
    &segmentation_platform::features::kContextualPageActions,
    &segmentation_platform::features::kContextualPageActionPriceTracking,
    &segmentation_platform::features::kContextualPageActionReaderMode,
    &send_tab_to_self::kSendTabToSelfSigninPromo,
    &send_tab_to_self::kSendTabToSelfV2,
    &share::kCormorant,
    &share::kCrowLaunchTab,
    &share::kPersistShareHubOnAppSwitch,
    &share::kScreenshotsForAndroidV2,
    &share::kUpcomingSharingFeatures,
    &supervised_users::kLocalWebApprovals,
    &supervised_users::kWebFilterInterstitialRefresh,
    &switches::kForceStartupSigninPromo,
    &switches::kForceDisableExtendedSyncPromos,
    &switches::kTangibleSync,
    &syncer::kSyncEnableHistoryDataType,
    &syncer::kSyncTrustedVaultPassphraseRecovery,
    &syncer::kSyncAndroidLimitNTPPromoImpressions,
    &syncer::kSyncAndroidPromosWithAlternativeTitle,
    &syncer::kSyncAndroidPromosWithIllustration,
    &syncer::kSyncAndroidPromosWithSingleButton,
    &syncer::kSyncAndroidPromosWithTitle,
    &subresource_filter::kSafeBrowsingSubresourceFilter,
    &video_tutorials::features::kVideoTutorials,
    &webapps::features::kInstallableAmbientBadgeInfoBar,
    &webapps::features::kInstallableAmbientBadgeMessage,
    &webapps::features::kWebApkUniqueId,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  static auto kFeaturesExposedToJavaMap = base::NoDestructor(
      base::MakeFlatMap<base::StringPiece, const base::Feature*>(
          kFeaturesExposedToJava, {},
          [](const base::Feature* a)
              -> std::pair<base::StringPiece, const base::Feature*> {
            return std::make_pair(a->name, a);
          }));

  auto it = kFeaturesExposedToJavaMap->find(base::StringPiece(feature_name));
  if (it != kFeaturesExposedToJavaMap->end()) {
    return it->second;
  }

  NOTREACHED() << "Queried feature cannot be found in ChromeFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

// Alphabetical:

BASE_FEATURE(kAdaptiveButtonInTopToolbar,
             "AdaptiveButtonInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2,
             "AdaptiveButtonInTopToolbarCustomizationV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAddEduAccountFromAccountSettingsForSupervisedUsers,
             "AddEduAccountFromAccountSettingsForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddToHomescreenIPH,
             "AddToHomescreenIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowNewIncognitoTabIntents,
             "AllowNewIncognitoTabIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFocusOmniboxInIncognitoTabIntents,
             "FocusOmniboxInIncognitoTabIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidScrollOptimizations,
             "AndroidScrollOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidSearchEngineChoiceNotification,
             "AndroidSearchEngineChoiceNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantConsentSimplifiedText,
             "AssistantConsentSimplifiedText",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantConsentV2,
             "AssistantConsentV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantIntentExperimentId,
             "AssistantIntentExperimentId",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantIntentPageUrl,
             "AssistantIntentPageUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantIntentTranslateInfo,
             "AssistantIntentTranslateInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantNonPersonalizedVoiceSearch,
             "AssistantNonPersonalizedVoiceSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppMenuMobileSiteOption,
             "AppMenuMobileSiteOption",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppToWebAttribution,
             "AppToWebAttribution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackgroundThreadPool,
             "BackgroundThreadPool",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConditionalTabStripAndroid,
             "ConditionalTabStripAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used in downstream code.
BASE_FEATURE(kCastDeviceFilter,
             "CastDeviceFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearOmniboxFocusAfterNavigation,
             "ClearOmniboxFocusAfterNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCloseTabSuggestions,
             "CloseTabSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCriticalPersistedTabData,
             "CriticalPersistedTabData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceCoupons,
             "CommerceCoupons",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTBackgroundTab,
             "CCTBackgroundTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTBrandTransparency,
             "CCTBrandTransparency",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTClientDataHeader,
             "CCTClientDataHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTNewDownloadTab,
             "CCTNewDownloadTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTFeatureUsage,
             "CCTFeatureUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIncognito, "CCTIncognito", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIncognitoAvailableToThirdParty,
             "CCTIncognitoAvailableToThirdParty",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIntentFeatureOverrides,
             "CCTIntentFeatureOverrides",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPackageNameRecording,
             "CCTPackageNameRecording",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPostMessageAPI,
             "CCTPostMessageAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPrefetchDelayShowOnStart,
             "CCTPrefetchDelayShowOnStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRealTimeEngagementSignals,
             "CCTRealTimeEngagementSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRedirectPreconnect,
             "CCTRedirectPreconnect",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRemoveRemoteViewIds,
             "CCTRemoveRemoteViewIds",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTReportParallelRequestStatus,
             "CCTReportParallelRequestStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizable90MaximumHeight,
             "CCTResizable90MaximumHeight",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableAllowResizeByUserGesture,
             "CCTResizableAllowResizeByUserGesture",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableAlwaysShowNavBarButtons,
             "CCTResizableAlwaysShowNavBarButtons",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableForFirstParties,
             "CCTResizableForFirstParties",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableForThirdParties,
             "CCTResizableForThirdParties",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableSideSheet,
             "CCTResizableSideSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResourcePrefetch,
             "CCTResourcePrefetch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRetainingStateInMemory,
             "CCTRetainingStateInMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTShowAboutBlankUrl,
             "CCTShowAboutBlankUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTToolbarCustomizations,
             "CCTToolbarCustomizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardOccludedBitmaps,
             "DiscardOccludedBitmaps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDontAutoHideBrowserControls,
             "DontAutoHideBrowserControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCacheDeprecatedSystemLocationSetting,
             "CacheDeprecatedSystemLocationSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeNewDownloadTab,
             "ChromeNewDownloadTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeShareLongScreenshot,
             "ChromeShareLongScreenshot",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSharingHub,
             "ChromeSharingHub",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSharingHubLaunchAdjacent,
             "ChromeSharingHubLaunchAdjacent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSurveyNextAndroid,
             "ChromeSurveyNextAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommandLineOnNonRooted,
             "CommandLineOnNonRooted",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuEnableLensShoppingAllowlist,
             "ContextMenuEnableLensShoppingAllowlist",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuGoogleLensChip,
             "ContextMenuGoogleLensChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuPopupForAllScreenSizes,
             "ContextMenuPopupForAllScreenSizes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuSearchWithGoogleLens,
             "ContextMenuSearchWithGoogleLens",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuShopWithGoogleLens,
             "ContextMenuShopWithGoogleLens",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuSearchAndShopWithGoogleLens,
             "ContextMenuSearchAndShopWithGoogleLens",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuTranslateWithGoogleLens,
             "ContextMenuTranslateWithGoogleLens",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensCameraAssistedSearch,
             "LensCameraAssistedSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOnQuickActionSearchWidget,
             "LensOnQuickActionSearchWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchDelayedIntelligence,
             "ContextualSearchDelayedIntelligence",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchDisableOnlineDetection,
             "ContextualSearchDisableOnlineDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchForceCaption,
             "ContextualSearchForceCaption",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchSuppressShortView,
             "ContextualSearchSuppressShortView",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchThinWebViewImplementation,
             "ContextualSearchThinWebViewImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeferKeepScreenOnDuringGesture,
             "DeferKeepScreenOnDuringGesture",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDirectActions, "DirectActions", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadAutoResumptionThrottling,
             "DownloadAutoResumptionThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadHomeForExternalApp,
             "DownloadHomeForExternalApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDuetTabStripIntegrationAndroid,
             "DuetTabStripIntegrationAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentsForAgsa,
             "ExperimentsForAgsa",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExploreSites, "ExploreSites", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFoldableJankFix,
             "FoldableJankFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGridTabSwitcherForTablets,
             "GridTabSwitcherForTablets",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHandleMediaIntents,
             "HandleMediaIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHideNonDisplayableAccountEmail,
             "HideNonDisplayableAccountEmail",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kImmersiveUiMode,
             "ImmersiveUiMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoReauthenticationForAndroid,
             "IncognitoReauthenticationForAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoScreenshot,
             "IncognitoScreenshot",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInfobarScrollOptimization,
             "InfobarScrollOptimization",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInstantStart, "InstantStart", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIsVoiceSearchEnabledCache,
             "IsVoiceSearchEnabledCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePromoExistingDevice,
             "SearchEnginePromo.ExistingDevice",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePromoExistingDeviceV2,
             "SearchEnginePromo.ExistingDeviceVer2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePromoNewDevice,
             "SearchEnginePromo.NewDevice",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePromoNewDeviceV2,
             "SearchEnginePromo.NewDeviceVer2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageTilesTitleWrapAround,
             "NewTabPageTilesTitleWrapAround",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewWindowAppMenu,
             "NewWindowAppMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationPermissionVariant,
             "NotificationPermissionVariant",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInstanceSwitcher,
             "InstanceSwitcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageAnnotationsService,
             "PageAnnotationsService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBookmarksImprovedSaveFlow,
             "BookmarksImprovedSaveFlow",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBookmarksRefresh,
             "BookmarksRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackGestureRefactorAndroid,
             "BackGestureRefactorAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaMinSdkVersionAndroid,
             "OmahaMinSdkVersionAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxModernizeVisualUpdate,
             "OmniboxModernizeVisualUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOpaqueOriginForIncomingIntents,
             "OpaqueOriginForIncomingIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeGeolocationHeaderGeneration,
             "OptimizeGeolocationHeaderGeneration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeLayoutsForPullRefresh,
             "OptimizeLayoutsForPullRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPostTaskFocusTab,
             "PostTaskFocusTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProbabilisticCryptidRenderer,
             "ProbabilisticCryptidRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReachedCodeProfiler,
             "ReachedCodeProfiler",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeInCCT,
             "ReaderModeInCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRecordSuppressionMetrics,
             "RecordSuppressionMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReengagementNotification,
             "ReengagementNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearches,
             "RelatedSearches",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearchesInBar,
             "RelatedSearchesInBar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearchesUi,
             "RelatedSearchesUi",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportParentalControlSitesChild,
             "ReportParentalControlSitesChild",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteDefaults,
             "RequestDesktopSiteDefaults",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteDefaultsControl,
             "RequestDesktopSiteDefaultsControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteDefaultsControlSynthetic,
             "RequestDesktopSiteDefaultsControlSynthetic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteDefaultsSynthetic,
             "RequestDesktopSiteDefaultsSynthetic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteOptInControlSynthetic,
             "RequestDesktopSiteOptInControlSynthetic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteOptInSynthetic,
             "RequestDesktopSiteOptInSynthetic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRequestDesktopSiteDefaultsDowngrade,
             "RequestDesktopSiteDefaultsDowngrade",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeModeForCachedFlags,
             "SafeModeForCachedFlags",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowScrollableMVTOnNTPAndroid,
             "ShowScrollableMVTOnNTPAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpannableInlineAutocomplete,
             "SpannableInlineAutocomplete",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpecialLocaleWrapper,
             "SpecialLocaleWrapper",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpecialUserDecision,
             "SpecialUserDecision",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCompositorTask,
             "SplitCompositorTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStoreHoursAndroid,
             "StoreHoursAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSuppressToolbarCaptures,
             "SuppressToolbarCaptures",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSwapPixelFormatToFixConvertFromTranslucent,
             "SwapPixelFormatToFixConvertFromTranslucent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabEngagementReportingAndroid,
             "TabEngagementReportingAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsAndroid,
             "TabGroupsAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsContinuationAndroid,
             "TabGroupsContinuationAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsUiImprovementsAndroid,
             "TabGroupsUiImprovementsAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsForTablets,
             "TabGroupsForTablets",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridLayoutAndroid,
             "TabGridLayoutAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabReparenting,
             "TabReparenting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabSelectionEditorV2,
             "TabSelectionEditorV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripImprovements,
             "TabStripImprovements",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoverFeedMultiColumn,
             "DiscoverFeedMultiColumn",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripRedesign,
             "TabStripRedesign",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSwitcherOnReturn,
             "TabSwitcherOnReturn",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabToGTSAnimation,
             "TabToGTSAnimation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultDisabled,
             "TestDefaultDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultEnabled,
             "TestDefaultEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kToolbarMicIphAndroid,
             "ToolbarMicIphAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kToolbarPhoneOptimizations,
             "ToolbarPhoneOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kToolbarScrollAblationAndroid,
             "ToolbarScrollAblationAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedWebActivityPostMessage,
             "TrustedWebActivityPostMessage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedWebActivityQualityEnforcement,
             "TrustedWebActivityQualityEnforcement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedWebActivityQualityEnforcementForced,
             "TrustedWebActivityQualityEnforcementForced",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedWebActivityQualityEnforcementWarning,
             "TrustedWebActivityQualityEnforcementWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceAndroid,
             "StartSurfaceAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedPositionAndroid,
             "FeedPositionAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchResumptionModuleAndroid,
             "SearchResumptionModuleAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceReturnTime,
             "StartSurfaceReturnTime",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceRefactor,
             "StartSurfaceRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceDisabledFeedImprovement,
             "StartSurfaceDisabledFeedImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, keep logging and reporting UMA while chrome is backgrounded.
BASE_FEATURE(kUmaBackgroundSessions,
             "UMABackgroundSessions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUpdateHistoryEntryPointsInIncognito,
             "UpdateHistoryEntryPointsInIncognito",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUpdateNotificationScheduleServiceImmediateShowOption,
             "UpdateNotificationScheduleServiceImmediateShowOption",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the LibunwindstackNativeUnwinderAndroid for only browser main thread, and
// only on Android.
//
// Enable by default to collect stack java samples for scroll jank effort as
// soon as possible.
BASE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid,
             "UseLibunwindstackNativeUnwinderAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUserMediaScreenCapturing,
             "UserMediaScreenCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVoiceSearchAudioCapturePolicy,
             "VoiceSearchAudioCapturePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVoiceButtonInTopToolbar,
             "VoiceButtonInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVrBrowsingFeedback,
             "VrBrowsingFeedback",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows only the remote device name on the Android notification instead of
// a descriptive text.
BASE_FEATURE(kWebOtpCrossDeviceSimpleString,
             "WebOtpCrossDeviceSimpleString",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebApkAllowIconUpdate,
             "WebApkAllowIconUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Chrome Android WebAPK-install service.
BASE_FEATURE(kWebApkInstallService,
             "WebApkInstallService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebApkTrampolineOnInitialIntent,
             "WebApkTrampolineOnInitialIntent",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jboolean JNI_ChromeFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

static ScopedJavaLocalRef<jstring>
JNI_ChromeFeatureList_GetFieldTrialParamByFeature(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  const std::string& param_value =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

static jint JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jint jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static jdouble JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsDouble(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jdouble jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsDouble(*feature, param_name,
                                                   jdefault_value);
}

static jboolean JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_ChromeFeatureList_GetFlattedFieldTrialParamsForFeature(
    JNIEnv* env,
    const JavaParamRef<jstring>& feature_name) {
  base::FieldTrialParams params;
  std::vector<std::string> keys_and_values;
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, feature_name));
  if (feature && base::GetFieldTrialParamsByFeature(*feature, &params)) {
    for (const auto& param_pair : params) {
      keys_and_values.push_back(param_pair.first);
      keys_and_values.push_back(param_pair.second);
    }
  }
  return base::android::ToJavaArrayOfStrings(env, keys_and_values);
}

}  // namespace android
}  // namespace chrome
