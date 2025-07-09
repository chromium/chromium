// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "build/android_buildflags.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/contextmenu/context_menu_features.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/task_manager/common/task_manager_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browsing_data/core/features.h"
#include "components/collaboration/public/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/contextual_search_field_trial.h"
#include "components/credential_management/android/features.h"
#include "components/data_sharing/public/features.h"
#include "components/download/public/common/download_features.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/plus_addresses/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/sensitive_content/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/base/features.h"
#include "components/sync_sessions/features.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/viz/common/features.h"
#include "components/webapps/browser/features.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/flags/jni_headers/ChromeFeatureMap_jni.h"

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &autofill::features::kAutofillEnableRankingFormulaAddressProfiles,
    &credential_management::features::
        kCredentialManagementThirdPartyWebApiRequestForwarding,
    &autofill::features::kAutofillEnableRankingFormulaCreditCards,
    &autofill::features::kAutofillEnableCardBenefitsForAmericanExpress,
    &autofill::features::kAutofillEnableCardBenefitsForBmo,
    &autofill::features::kAutofillEnablePaymentSettingsCardPromoAndScanCard,
    &autofill::features::kAutofillEnablePaymentSettingsCardPromoAndScanCard,
    &autofill::features::kAutofillThirdPartyModeContentProvider,
    &autofill::features::kAutofillEnableSecurityTouchEventFilteringAndroid,
    &autofill::features::kAutofillVirtualViewStructureAndroid,
    &autofill::features::kAutofillDeepLinkAutofillOptions,
    &autofill::features::kAutofillEnableCvcStorageAndFilling,
    &autofill::features::kAutofillSyncEwalletAccounts,
    &autofill::features::kAutofillEnableSyncingOfPixBankAccounts,
    &autofill::features::kAutofillEnableVirtualCardJavaPaymentsDataManager,
    &autofill::features::kAutofillEnableSupportForHomeAndWork,
    &autofill::features::kAutofillRetryImageFetchOnFailure,
    &blink::features::kBackForwardTransitions,
    &blink::features::kDynamicSafeAreaInsets,
    &blink::features::kForceWebContentsDarkMode,
    &blink::features::kPrerender2,
    &browsing_data::features::kBrowsingDataModel,
    &commerce::kCommerceMerchantViewer,
    &commerce::kEnableDiscountInfoApi,
    &commerce::kPriceAnnotations,
    &commerce::kPriceInsights,
    &commerce::kShoppingList,
    &commerce::kShoppingPDPMetrics,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &content_settings::features::kTrackingProtection3pcd,
    &content_settings::features::kUserBypassUI,
    &data_controls::kEnableClipboardDataControlsAndroid,
    &data_sharing::features::kDataSharingFeature,
    &data_sharing::features::kDataSharingJoinOnly,
    &data_sharing::features::kDataSharingNonProductionEnvironment,
    &download::features::kEnableSavePackageForOffTheRecord,
    &download::features::kSmartSuggestionForLargeDownloads,
    &base::features::kCollectAndroidFrameTimelineMetrics,
    &download::features::kDownloadNotificationServiceUnifiedAPI,
    &features::kAndroidBcivBottomControls,
    &features::kAndroidBrowserControlsInViz,
    &features::kAndroidWebAppLaunchHandler,
    &features::kGenericSensorExtraClasses,
    &features::kBackForwardCache,
    &features::kBoardingPassDetector,
    &features::kContextMenuEmptySpace,
    &features::kDisplayEdgeToEdgeFullscreen,
    &features::kHttpsFirstBalancedMode,
    &features::kNetworkServiceInProcess,
    &features::kElasticOverscroll,
    &features::kLinkedServicesSetting,
    &features::kLoadingPredictorLimitPreconnectSocketCount,
    &features::kNotificationOneTapUnsubscribe,
    &features::kPrefetchBrowserInitiatedTriggers,
    &features::kPushMessagingDisallowSenderIDs,
    &features::kPwaUpdateDialogForIcon,
    &features::kSafetyHub,
    &features::kSafetyHubAndroidOrganicSurvey,
    &features::kSafetyHubAndroidSurvey,
    &features::kSafetyHubAndroidSurveyV2,
    &features::kSafetyHubDisruptiveNotificationRevocation,
    &features::kSafetyHubFollowup,
    &features::kSafetyHubLocalPasswordsModule,
    &features::kSafetyHubMagicStack,
    &features::kSafetyHubUnifiedPasswordsModule,
    &features::kSafetyHubWeakAndReusedPasswords,
    &features::kTaskManagerClank,
    &features::kQuietNotificationPrompts,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &feature_engagement::kIPHRtlGestureNavigationFeature,
    &feed::kFeedContainment,
    &feed::kFeedFollowUiUpdate,
    &feed::kFeedHeaderRemoval,
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedLoadingPlaceholder,
    &feed::kFeedNoViewCache,
    &feed::kFeedPerformanceStudy,
    &feed::kFeedShowSignInCommand,
    &feed::kFeedSignedOutViewDemotion,
    &feed::kFeedRecyclerBinderUnmountOnDetach,
    &feed::kInterestFeedV2,
    &feed::kWebFeedAwareness,
    &feed::kWebFeedOnboarding,
    &feed::kWebFeedSort,
    &feed::kXsurfaceMetricsReporting,
    &history::kOrganicRepeatableQueries,
    &history_clusters::internal::kJourneys,
    &history_clusters::internal::kOmniboxAction,
    &kAccountForSuppressedKeyboardInsets,
    &kAdaptiveButtonInTopToolbarCustomizationV2,
    &kAdaptiveButtonInTopToolbarPageSummary,
    &kAllowTabClosingUponMinimization,
    &kAndroidAppIntegration,
    &kAndroidAppIntegrationV2,
    &kNewTabPageCustomization,
    &kNewTabPageCustomizationToolbarButton,
    &kAndroidAppIntegrationWithFavicon,
    &kAndroidAppIntegrationMultiDataSource,
    &kAndroidAppearanceSettings,
    &kAndroidBookmarkBar,
    &kAndroidBottomToolbar,
    &kAndroidDumpOnScrollWithoutResource,
    &kAndroidElegantTextHeight,
    &kAndroidKeyboardA11y,
    &kAndroidMetaClickHistoryNavigation,
    &kAndroidNativePagesInNewTab,
    &kAndroidProgressBarVisualUpdate,
    &kAndroidNoVisibleHintForDifferentTLD,
    &kAndroidOmniboxFocusedNewTabPage,
    &kAndroidOpenPdfInlineBackport,
    &kAndroidPdfAssistContent,
    &kAndroidSurfaceColorUpdate,
    &kAndroidTabDeclutterArchiveAllButActiveTab,
    &kAndroidTabDeclutterArchiveDuplicateTabs,
    &kAndroidTabDeclutterArchiveTabGroups,
    &kAndroidTabDeclutterAutoDelete,
    &kAndroidTabDeclutterAutoDeleteKillSwitch,
    &kAndroidTabDeclutterDedupeTabIdsKillSwitch,
    &kAndroidTabDeclutterRescueKillswitch,
    &kAndroidTabDeclutterPerformanceImprovements,
    &kAndroidTabSkipSaveTabsKillswitch,
    &kAndroidThemeModule,
    &kAndroidToolbarScrollAblation,
    &kAndroidWindowPopupLargeScreen,
    &kAnimatedImageDragShadow,
    &kAppSpecificHistory,
    &kAsyncNotificationManager,
    &kAsyncNotificationManagerForDownload,
    &kAuxiliarySearchDonation,
    &kBackgroundThreadPool,
    &kBatchTabRestore,
    &kBlockIntentsWhileLocked,
    &kBookmarkPaneAndroid,
    &kBottomBrowserControlsRefactor,
    &kTabClosureMethodRefactor,
    &kBrowserControlsDebugging,
    &kBrowserControlsEarlyResize,
    &kCacheActivityTaskID,
    &kCacheIsMultiInstanceApi31Enabled,
    &kCastDeviceFilter,
    &kCCTAdaptiveButton,
    &kCCTAuthTab,
    &kCCTAuthTabDisableAllExternalIntents,
    &kCCTAuthTabEnableHttpsRedirects,
    &kCCTBlockTouchesDuringEnterAnimation,
    &kCCTClientDataHeader,
    &kCCTEarlyNav,
    &kCCTExtendTrustedCdnPublisher,
    &kCCTEphemeralMediaViewerExperiment,
    &kCCTEphemeralMode,
    &kCCTFreInSameTask,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTIntentFeatureOverrides,
    &kCCTMinimized,
    &kCCTMinimizedEnabledByDefault,
    &kCCTNavigationalPrefetch,
    &kCCTNestedSecurityIcon,
    &kCCTGoogleBottomBar,
    &kCCTGoogleBottomBarVariantLayouts,
    &kCCTOpenInBrowserButtonIfAllowedByEmbedder,
    &kCCTOpenInBrowserButtonIfEnabledByEmbedder,
    &kCCTPredictiveBackGesture,
    &kCCTPrewarmTab,
    &kCCTReportParallelRequestStatus,
    &kCCTReportPrerenderEvents,
    &kCCTResizableForThirdParties,
    &kCCTRevampedBranding,
    &kCCTShowTabFix,
    &kCCTTabModalDialog,
    &kCCTToolbarRefactor,
    &kChangeUnfocusedPriority,
    &kDefaultBrowserPromoAndroid2,
    &kDisableInstanceLimit,
    &kDontAutoHideBrowserControls,
    &kCacheDeprecatedSystemLocationSetting,
    &kChromeSurveyNextAndroid,
    &kClampAutomotiveScaling,
    &kClankStartupLatencyInjection,
    &kClankWhatsNew,
    &kClearBrowsingDataAndroidSurvey,
    &kClearInstanceInfoWhenClosedIntentionally,
    &kCommandLineOnNonRooted,
    &kContextMenuTranslateWithGoogleLens,
    &kContextMenuSysUiMatchesActivity,
    &kContextualSearchDisableOnlineDetection,
    &kContextualSearchSuppressShortView,
    &kControlsVisibilityFromNavigations,
    &kCpaSpecUpdate,
    &kCrossDeviceTabPaneAndroid,
    &kDeviceAuthenticatorAndroidx,
    &kDisableListTabSwitcher,
    &kDrawKeyNativeEdgeToEdge,
    &kEdgeToEdgeBottomChin,
    &kEdgeToEdgeDebugging,
    &kEdgeToEdgeEverywhere,
    &kEdgeToEdgeMonitorConfigurations,
    &kEdgeToEdgeSafeAreaConstraint,
    &kEdgeToEdgeTablet,
    &kEdgeToEdgeWebOptIn,
    &kEducationalTipDefaultBrowserPromoCard,
    &kEmptyTabListAnimationKillSwitch,
    &kEnableXAxisActivityTransition,
    &kExperimentsForAgsa,
    &kFloatingSnackbar,
    &kForceBrowserControlsUponExitingFullscreen,
    &kForceListTabSwitcher,
    &kForceTranslucentNotificationTrampoline,
    &kFullscreenInsetsApiMigration,
    &kFullscreenInsetsApiMigrationOnAutomotive,
    &kGridTabSwitcherSurfaceColorUpdate,
    &kGroupNewTabWithParent,
    &kHeadlessTabModel,
    &kHideTabletToolbarDownloadButton,
    &kHistoryPaneAndroid,
    &kHomepageIsNewTabPagePolicyAndroid,
    &kLegacyTabStateDeprecation,
    &kLockBackPressHandlerAtStart,
    &kIncognitoScreenshot,
    &kInstanceSwitcherV2,
    &kKeyboardEscBackNavigation,
    &kLensOnQuickActionSearchWidget,
    &kMagicStackAndroid,
    &kMayLaunchUrlUsesSeparateStoragePartition,
    &kMiniOriginBar,
    &kMostVisitedTilesCustomization,
    &kMostVisitedTilesReselect,
    &kMultiInstanceApplicationStatusCleanup,
    &kMvcUpdateViewWhenModelChanged,
    &kNativePageTransitionHardwareCapture,
    &kNavBarColorAnimation,
    &kNavBarColorMatchesTabBackground,
    &kNewTabPageAndroidTriggerForPrerender2,
    &kNotificationPermissionVariant,
    &kNotificationPermissionBottomSheet,
    &kNotificationTrampoline,
    &kTinkerTankBottomSheet,
    &kPageAnnotationsService,
    &kPageContentProvider,
    &kPowerSavingModeBroadcastReceiverInBackground,
    &kPreconnectOnTabCreation,
    &kPriceChangeModule,
    &kProcessRankPolicyAndroid,
    &kProtectedTabsAndroid,
    &kPwaRestoreUi,
    &kPwaRestoreUiAtStartup,
    &kOmahaMinSdkVersionAndroid,
    &kShortCircuitUnfocusAnimation,
    &kShowHomeButtonPolicyAndroid,
    &kShowNewTabAnimations,
    &kPartnerCustomizationsUma,
    &kQuickDeleteAndroidSurvey,
    &kReadAloud,
    &kReadAloudAudioOverviews,
    &kReadAloudAudioOverviewsFeedback,
    &kReadAloudInOverflowMenuInCCT,
    &kReadAloudInMultiWindow,
    &kReadAloudBackgroundPlayback,
    &kReadAloudPlayback,
    &kReadAloudTapToSeek,
    &kReadAloudIPHMenuButtonHighlightCCT,
    &kRecordSuppressionMetrics,
    &kReengagementNotification,
    &kRelatedSearchesAllLanguage,
    &kRelatedSearchesSwitch,
    &kReloadTabUiResourcesIfChanged,
    &kRemoveTabFocusOnShowingAndSelect,
    &kRightEdgeGoesForwardGestureNav,
    &kSearchInCCT,
    &kSearchInCCTAlternateTapHandling,
    &kSearchResumptionModuleAndroid,
    &kSettingsSingleActivity,
    &kShareCustomActionsInCCT,
    &kSkipIsolatedSplitPreload,
    &kSmallerTabStripTitleLimit,
    &kSuppressToolbarCapturesAtGestureEnd,
    &kSwapNewTabAndNewTabInGroupAndroid,
    &kTabGroupEntryPointsAndroid,
    &kTabGroupParityBottomSheetAndroid,
    &kTabletTabStripAnimation,
    &kToolbarPhoneCleanup,
    &kTabStateFlatBuffer,
    &kTabStripContextMenuAndroid,
    &kTabStripDensityChangeAndroid,
    &kTabStripGroupDragDropAndroid,
    &kTabStripGroupReorderAndroid,
    &kTabStripIncognitoMigration,
    &kTabStripLayoutOptimization,
    &kTabStripTransitionInDesktopWindow,
    &kTabSwitcherColorBlendAnimate,
    &kTabSwitcherForeignFaviconSupport,
    &kTabWindowManagerReportIndicesMismatch,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kTileContextMenuRefactor,
    &kTopControlsRefactor,
    &kTraceBinderIpc,
    &kStartSurfaceReturnTime,
    &kUmaBackgroundSessions,
    &kUpdateCompositorForSurfaceControl,
    &kUseActivityManagerForTabActivation,
    &kUseLibunwindstackNativeUnwinderAndroid,
    &kWebOtpCrossDeviceSimpleString,
    &kWebApkMinShellVersion,
    &kGridTabSwitcherUpdate,
    &notifications::features::kUseChimeAndroidSdk,
    &paint_preview::kPaintPreviewDemo,
    &language::kCctAutoTranslate,
    &language::kDetailedLanguageSettings,
    &optimization_guide::features::kPushNotifications,
    &page_info::kPageInfoAboutThisSiteMoreLangs,
    &password_manager::features::kBiometricAuthIdentityCheck,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::kLoginDbDeprecationAndroid,
    &password_manager::features::kPasswordFormGroupedAffiliations,
    &permissions::features::kPermissionsPromptSurvey,
    &permissions::features::kPermissionDedicatedCpssSettingAndroid,
    &permissions::features::kPermissionSiteSettingsRadioButton,
    &plus_addresses::features::kPlusAddressesEnabled,
    &plus_addresses::features::kPlusAddressAndroidOpenGmsCoreManagementPage,
    &privacy_sandbox::kAlwaysBlock3pcsIncognito,
    &privacy_sandbox::kDisplayWildcardInContentSettings,
    &privacy_sandbox::kFingerprintingProtectionUx,
    &privacy_sandbox::kIpProtectionUx,
    &privacy_sandbox::kPrivacySandboxActivityTypeStorage,
    &privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements,
    &privacy_sandbox::kPrivacySandboxAdsNoticeCCT,
    &privacy_sandbox::kPrivacySandboxRelatedWebsiteSetsUi,
    &privacy_sandbox::kPrivacySandboxSettings4,
    &privacy_sandbox::kPrivacySandboxAdTopicsContentParity,
    &privacy_sandbox::kPrivacySandboxSentimentSurvey,
    &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
    &privacy_sandbox::kTrackingProtectionUserBypassPwa,
    &privacy_sandbox::kTrackingProtectionUserBypassPwaTrigger,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &safe_browsing::kReportNotificationContentDetectionData,
    &safe_browsing::kShowWarningsForSuspiciousNotifications,
    &segmentation_platform::features::kAndroidAppIntegrationModule,
    &segmentation_platform::features::kContextualPageActions,
    &segmentation_platform::features::kContextualPageActionShareModel,
    &segmentation_platform::features::kEducationalTipModule,
    &segmentation_platform::features::kSegmentationPlatformEphemeralCardRanker,
    &segmentation_platform::features::
        kSegmentationPlatformAndroidHomeModuleRanker,
    &segmentation_platform::features::
        kSegmentationPlatformAndroidHomeModuleRankerV2,
    &sensitive_content::features::kSensitiveContent,
    &sensitive_content::features::kSensitiveContentWhileSwitchingTabs,
    &switches::kForceStartupSigninPromo,
    &sync_sessions::kOptimizeAssociateWindowsAndroid,
    &syncer::kWebApkBackupAndRestoreBackend,
    &syncer::kUnoPhase2FollowUp,
    &syncer::kSyncEnablePasswordsSyncErrorMessageAlternative,
    &tab_groups::kTabGroupSyncAndroid,
    &tab_groups::kTabGroupSyncAutoOpenKillSwitch,
    &tab_groups::kUseAlternateHistorySyncIllustration,
    &visited_url_ranking::features::kGroupSuggestionService,
    &visited_url_ranking::features::kVisitedURLRankingService,
    &webapps::features::kWebApkInstallFailureNotification,
    &webapps::features::kAndroidMinimalUiLargeScreen,
    &base::features::kPostGetMyMemoryStateToBackground,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_ChromeFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

// Alphabetical:

BASE_FEATURE(kAccountForSuppressedKeyboardInsets,
             "AccountForSuppressedKeyboardInsets",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2,
             "AdaptiveButtonInTopToolbarCustomizationV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdaptiveButtonInTopToolbarPageSummary,
             "AdaptiveButtonInTopToolbarPageSummary",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowTabClosingUponMinimization,
             "AllowTabClosingUponMinimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableListTabSwitcher,
             "DisableListTabSwitcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Long-term flag for debugging only.
BASE_FEATURE(kForceListTabSwitcher,
             "ForceListTabSwitcher",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegration,
             "AndroidAppIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegrationV2,
             "AndroidAppIntegrationV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegrationMultiDataSource,
             "AndroidAppIntegrationMultiDataSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageCustomization,
             "NewTabPageCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageCustomizationToolbarButton,
             "NewTabPageCustomizationToolbarButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegrationWithFavicon,
             "AndroidAppIntegrationWithFavicon",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppearanceSettings,
             "AndroidAppearanceSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidBookmarkBar,
             "AndroidBookmarkBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidBottomToolbar,
             "AndroidBottomToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidDumpOnScrollWithoutResource,
             "AndroidDumpOnScrollWithoutResource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidElegantTextHeight,
             "AndroidElegantTextHeight",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidKeyboardA11y,
             "AndroidKeyboardA11y",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidMetaClickHistoryNavigation,
             "AndroidMetaClickHistoryNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidNativePagesInNewTab,
             "AndroidNativePagesInNewTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidProgressBarVisualUpdate,
             "AndroidProgressBarVisualUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidNoVisibleHintForDifferentTLD,
             "AndroidNoVisibleHintForDifferentTLD",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidOmniboxFocusedNewTabPage,
             "AndroidOmniboxFocusedNewTabPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidOpenPdfInlineBackport,
             "AndroidOpenPdfInlineBackport",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidPdfAssistContent,
             "AndroidPdfAssistContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidSurfaceColorUpdate,
             "AndroidSurfaceColorUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterArchiveAllButActiveTab,
             "AndroidTabDeclutterArchiveAllButActiveTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterArchiveDuplicateTabs,
             "AndroidTabDeclutterArchiveDuplicateTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterArchiveTabGroups,
             "AndroidTabDeclutterArchiveTabGroups",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterAutoDelete,
             "AndroidTabDeclutterAutoDelete",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterAutoDeleteKillSwitch,
             "AndroidTabDeclutterAutoDeleteKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterDedupeTabIdsKillSwitch,
             "AndroidTabDeclutterDedupeTabIdsKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterPerformanceImprovements,
             "AndroidTabDeclutterPerformanceImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterRescueKillswitch,
             "AndroidTabDeclutterRescueKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabSkipSaveTabsKillswitch,
             "AndroidTabSkipSaveTabsTaskKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidThemeModule,
             "AndroidThemeModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidToolbarScrollAblation,
             "AndroidToolbarScrollAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidWindowPopupLargeScreen,
             "AndroidWindowPopupLargeScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAnimatedImageDragShadow,
             "AnimatedImageDragShadow",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppSpecificHistory,
             "AppSpecificHistory",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAsyncNotificationManager,
             "AsyncNotificationManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAsyncNotificationManagerForDownload,
             "AsyncNotificationManagerForDownload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAuxiliarySearchDonation,
             "AuxiliarySearchDonation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTinkerTankBottomSheet,
             "TinkerTankBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackgroundThreadPool,
             "BackgroundThreadPool",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBatchTabRestore,
             "BatchTabRestore",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBlockIntentsWhileLocked,
             "BlockIntentsWhileLocked",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBookmarkPaneAndroid,
             "BookmarkPaneAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBottomBrowserControlsRefactor,
             "BottomBrowserControlsRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserControlsDebugging,
             "BrowserControlsDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserControlsEarlyResize,
             "BrowserControlsEarlyResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCacheActivityTaskID,
             "CacheActivityTaskID",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCacheIsMultiInstanceApi31Enabled,
             "CacheIsMultiInstanceApi31Enabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used in downstream code.
BASE_FEATURE(kCastDeviceFilter,
             "CastDeviceFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTAdaptiveButton,
             "CCTAdaptiveButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTAuthTab, "CCTAuthTab", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAuthTabDisableAllExternalIntents,
             "CCTAuthTabDisableAllExternalIntents",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAuthTabEnableHttpsRedirects,
             "CCTAuthTabEnableHttpsRedirects",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTBlockTouchesDuringEnterAnimation,
             "CCTBlockTouchesDuringEnterAnimation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTClientDataHeader,
             "CCTClientDataHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTEarlyNav, "CCTEarlyNav", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTEphemeralMediaViewerExperiment,
             "CCTEphemeralMediaViewerExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTEphemeralMode,
             "CCTEphemeralMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTExtendTrustedCdnPublisher,
             "CCTExtendTrustedCdnPublisher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTFreInSameTask,
             "CCTFreInSameTask",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIncognitoAvailableToThirdParty,
             "CCTIncognitoAvailableToThirdParty",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIntentFeatureOverrides,
             "CCTIntentFeatureOverrides",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTMinimized, "CCTMinimized", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTMinimizedEnabledByDefault,
             "CCTMinimizedEnabledByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTNavigationalPrefetch,
             "CCTNavigationalPrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTNestedSecurityIcon,
             "CCTNestedSecurityIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTGoogleBottomBar,
             "CCTGoogleBottomBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTGoogleBottomBarVariantLayouts,
             "CCTGoogleBottomBarVariantLayouts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTOpenInBrowserButtonIfAllowedByEmbedder,
             "CCTOpenInBrowserButtonIfAllowedByEmbedder",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTOpenInBrowserButtonIfEnabledByEmbedder,
             "CCTOpenInBrowserButtonIfEnabledByEmbedder",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPredictiveBackGesture,
             "CCTPredictiveBackGesture",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabClosureMethodRefactor,
             "TabClosureMethodRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGridTabSwitcherUpdate,
             "GridTabSwitcherUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPrewarmTab, "CCTPrewarmTab", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTReportParallelRequestStatus,
             "CCTReportParallelRequestStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTReportPrerenderEvents,
             "CCTReportPrerenderEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableForThirdParties,
             "CCTResizableForThirdParties",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRevampedBranding,
             "CCTRevampedBranding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTShowTabFix, "CCTShowTabFix", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTTabModalDialog,
             "CCTTabModalDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTToolbarRefactor,
             "CCTToolbarRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, render processes associated only with tabs in unfocused windows
// will be downgraded to "vis" priority, rather than remaining at "fg". This
// will allow tabs in unfocused windows to be prioritized for OOM kill in
// low-memory scenarios.
BASE_FEATURE(kChangeUnfocusedPriority,
             "ChangeUnfocusedPriority",
#if BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable by default for desktop platforms, pending a tablet rollout using the
// same flag.
// TODO(crbug.com/368058472): Remove when tablet rollout is complete.
BASE_FEATURE(kDisableInstanceLimit,
             "DisableInstanceLimit",
#if BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDontAutoHideBrowserControls,
             "DontAutoHideBrowserControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCacheDeprecatedSystemLocationSetting,
             "CacheDeprecatedSystemLocationSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSurveyNextAndroid,
             "ChromeSurveyNextAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClampAutomotiveScaling,
             "ClampAutomotiveScaling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClankStartupLatencyInjection,
             "ClankStartupLatencyInjection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClankWhatsNew,
             "ClankWhatsNew",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearBrowsingDataAndroidSurvey,
             "ClearBrowsingDataAndroidSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearInstanceInfoWhenClosedIntentionally,
             "ClearInstanceInfoWhenClosedIntentionally",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCommandLineOnNonRooted,
             "CommandLineOnNonRooted",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuSysUiMatchesActivity,
             "ContextMenuSysUiMatchesActivity",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuTranslateWithGoogleLens,
             "ContextMenuTranslateWithGoogleLens",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOnQuickActionSearchWidget,
             "LensOnQuickActionSearchWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchDisableOnlineDetection,
             "ContextualSearchDisableOnlineDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchSuppressShortView,
             "ContextualSearchSuppressShortView",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kControlsVisibilityFromNavigations,
             "ControlsVisibilityFromNavigations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCpaSpecUpdate,
             "CpaSpecUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCrossDeviceTabPaneAndroid,
             "CrossDeviceTabPaneAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPromoAndroid2,
             "DefaultBrowserPromoAndroid2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The feature is a no-op, it replaces android.hardware.biometrics library on
// Android with androidx.biometric.
BASE_FEATURE(kDeviceAuthenticatorAndroidx,
             "DeviceAuthenticatorAndroidx",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDrawKeyNativeEdgeToEdge,
             "DrawKeyNativeEdgeToEdge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeBottomChin,
             "EdgeToEdgeBottomChin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeDebugging,
             "EdgeToEdgeDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeMonitorConfigurations,
             "EdgeToEdgeMonitorConfigurations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeEverywhere,
             "EdgeToEdgeEverywhere",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeSafeAreaConstraint,
             "EdgeToEdgeSafeAreaConstraint",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeTablet,
             "EdgeToEdgeTablet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeWebOptIn,
             "EdgeToEdgeWebOptIn",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEducationalTipDefaultBrowserPromoCard,
             "EducationalTipDefaultBrowserPromoCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEmptyTabListAnimationKillSwitch,
             "EmptyTabListAnimationKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableXAxisActivityTransition,
             "EnableXAxisActivityTransition",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentsForAgsa,
             "ExperimentsForAgsa",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFloatingSnackbar,
             "FloatingSnackbar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kForceBrowserControlsUponExitingFullscreen,
             "ForceBrowserControlsUponExitingFullscreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kForceTranslucentNotificationTrampoline,
             "ForceTranslucentNotificationTrampoline",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenInsetsApiMigration,
             "FullscreenInsetsApiMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive,
             "FullscreenInsetsApiMigrationOnAutomotive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGridTabSwitcherSurfaceColorUpdate,
             "GridTabSwitcherSurfaceColorUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGroupNewTabWithParent,
             "GroupNewTabWithParent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHeadlessTabModel,
             "HeadlessTabModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryPaneAndroid,
             "HistoryPaneAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHomepageIsNewTabPagePolicyAndroid,
             "HomepageIsNewTabPagePolicyAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLegacyTabStateDeprecation,
             "LegacyTabStateDeprecation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLockBackPressHandlerAtStart,
             "LockBackPressHandlerAtStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoScreenshot,
             "IncognitoScreenshot",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInstanceSwitcherV2,
             "InstanceSwitcherV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeyboardEscBackNavigation,
             "KeyboardEscBackNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMagicStackAndroid,
             "MagicStackAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables an experimental feature which forces mayLaunchUrl to use a different
// storage partition. This may reduce performance. This should not be enabled by
// default.
BASE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition,
             "MayLaunchUrlUsesSeparateStoragePartition",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMiniOriginBar,
             "MiniOriginBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMostVisitedTilesCustomization,
             "MostVisitedTilesCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMostVisitedTilesReselect,
             "MostVisitedTilesReselect",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMultiInstanceApplicationStatusCleanup,
             "MultiInstanceApplicationStatusCleanup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMvcUpdateViewWhenModelChanged,
             "MvcUpdateViewWhenModelChanged",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNativePageTransitionHardwareCapture,
             "NativePageTransitionHardwareCapture",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNavBarColorAnimation,
             "NavBarColorAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNavBarColorMatchesTabBackground,
             "NavBarColorMatchesTabBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageAndroidTriggerForPrerender2,
             "NewTabPageAndroidTriggerForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationPermissionVariant,
             "NotificationPermissionVariant",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationPermissionBottomSheet,
             "NotificationPermissionBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationTrampoline,
             "NotificationTrampoline",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageAnnotationsService,
             "PageAnnotationsService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentProvider,
             "PageContentProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPowerSavingModeBroadcastReceiverInBackground,
             "PowerSavingModeBroadcastReceiverInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreconnectOnTabCreation,
             "PreconnectOnTabCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPriceChangeModule,
             "PriceChangeModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProcessRankPolicyAndroid,
             "ProcessRankPolicyAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Put a higher memory priority to protected background tabs (e.g. tabs with
// user edits in forms) to prevent them from being killed by LMKD before any
// other non-protected tabs.
BASE_FEATURE(kProtectedTabsAndroid,
             "ProtectedTabsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPwaRestoreUi, "PwaRestoreUi", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPwaRestoreUiAtStartup,
             "PwaRestoreUiAtStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHideTabletToolbarDownloadButton,
             "HideTabletToolbarDownloadButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaMinSdkVersionAndroid,
             "OmahaMinSdkVersionAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShortCircuitUnfocusAnimation,
             "ShortCircuitUnfocusAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowHomeButtonPolicyAndroid,
             "ShowHomeButtonPolicyAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowNewTabAnimations,
             "ShowNewTabAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartnerCustomizationsUma,
             "PartnerCustomizationsUma",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuickDeleteAndroidSurvey,
             "QuickDeleteAndroidSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloud, "ReadAloud", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudAudioOverviews,
             "ReadAloudAudioOverviews",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudAudioOverviewsFeedback,
              "ReadAloudAudioOverviewsFeedback",
              base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudInOverflowMenuInCCT,
             "ReadAloudInOverflowMenuInCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudInMultiWindow,
             "ReadAloudInMultiWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudBackgroundPlayback,
             "ReadAloudBackgroundPlayback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudPlayback,
             "ReadAloudPlayback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudTapToSeek,
             "ReadAloudTapToSeek",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudServerExperiments,
             "ReadAloudServerExperiments",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT,
             "ReadAloudIPHMenuButtonHighlightCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRecordSuppressionMetrics,
             "RecordSuppressionMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReengagementNotification,
             "ReengagementNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearchesAllLanguage,
             "RelatedSearchesAllLanguage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearchesSwitch,
             "RelatedSearchesSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReloadTabUiResourcesIfChanged,
             "ReloadTabUiResourcesIfChanged",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveTabFocusOnShowingAndSelect,
             "RemoveTabFocusOnShowingAndSelect",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRightEdgeGoesForwardGestureNav,
             "RightEdgeGoesForwardGestureNav",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSettingsSingleActivity,
             "SettingsSingleActivity",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShareCustomActionsInCCT,
             "ShareCustomActionsInCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipIsolatedSplitPreload,
             "SkipIsolatedSplitPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmallerTabStripTitleLimit,
             "SmallerTabStripTitleLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuppressToolbarCapturesAtGestureEnd,
             "SuppressToolbarCapturesAtGestureEnd",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSwapNewTabAndNewTabInGroupAndroid,
             "SwapNewTabAndNewTabInGroupAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupEntryPointsAndroid,
             "TabGroupEntryPointsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupParityBottomSheetAndroid,
             "TabGroupParityBottomSheetAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabletTabStripAnimation,
             "TabletTabStripAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kToolbarPhoneCleanup,
             "ToolbarPhoneCleanup",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStateFlatBuffer,
             "TabStateFlatBuffer",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripContextMenuAndroid,
             "TabStripContextMenuAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripDensityChangeAndroid,
             "TabStripDensityChangeAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripGroupDragDropAndroid,
             "TabStripGroupDragDropAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripGroupReorderAndroid,
             "TabStripGroupReorderAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripLayoutOptimization,
             "TabStripLayoutOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripTransitionInDesktopWindow,
             "TabStripTransitionInDesktopWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripIncognitoMigration,
             "TabStripIncognitoMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSwitcherColorBlendAnimate,
             "TabSwitcherColorBlendAnimate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabSwitcherForeignFaviconSupport,
             "TabSwitcherForeignFaviconSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabWindowManagerReportIndicesMismatch,
             "TabWindowManagerReportIndicesMismatch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultDisabled,
             "TestDefaultDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultEnabled,
             "TestDefaultEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTileContextMenuRefactor,
             "TileContextMenuRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of the refactored Top Controls approach on Android.
BASE_FEATURE(kTopControlsRefactor,
             "TopControlsRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTraceBinderIpc,
             "TraceBinderIpc",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchInCCT, "SearchInCCT", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchInCCTAlternateTapHandling,
             "SearchInCCTAlternateTapHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchResumptionModuleAndroid,
             "SearchResumptionModuleAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceReturnTime,
             "StartSurfaceReturnTime",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, keep logging and reporting UMA while chrome is backgrounded.
BASE_FEATURE(kUmaBackgroundSessions,
             "UMABackgroundSessions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Actively update the compositor surface when surface control is enabled.
BASE_FEATURE(kUpdateCompositorForSurfaceControl,
             "UpdateCompositorForSurfaceControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Activate tab with moveTaskToFront() which works in multi-window mode.
BASE_FEATURE(kUseActivityManagerForTabActivation,
             "UseActivityManagerForTabActivation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the LibunwindstackNativeUnwinderAndroid for only browser main thread, and
// only on Android.
BASE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid,
             "UseLibunwindstackNativeUnwinderAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shows only the remote device name on the Android notification instead of
// a descriptive text.
BASE_FEATURE(kWebOtpCrossDeviceSimpleString,
             "WebOtpCrossDeviceSimpleString",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace android
}  // namespace chrome
