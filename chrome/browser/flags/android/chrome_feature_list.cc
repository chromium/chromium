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
#include "chrome/browser/task_manager/common/task_manager_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome_feature_list.h"
#include "components/android_autofill/browser/android_autofill_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/accessibility/android/features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browsing_data/core/features.h"
#include "components/collaboration/public/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/credential_management/android/features.h"
#include "components/data_sharing/public/features.h"
#include "components/download/public/common/download_features.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
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
#include "components/plus_addresses/core/common/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/push_messaging/push_messaging_features.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safety_check/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/sensitive_content/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_isolation/features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/features.h"
#include "components/sync_sessions/features.h"
#include "components/touch_to_search/core/browser/contextual_search_field_trial.h"
#include "components/variations/net/omnibox_autofocus_http_headers.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/viz/common/features.h"
#include "components/webapps/browser/features.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
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
// in other locations in the code base (e.g. chrome/, components/, etc). Clang
// formatting is turned off so that long names don't extend to two lines, which
// makes it easier to have scripts that automatically add new flags correctly.

// clang-format off

// Alphabetical:
// LINT.IfChange(FeaturesExposedToJava)
const base::Feature* const kFeaturesExposedToJava[] = {
// FEATURE_EXPORT_LIST_START
// go/keep-sorted start
    &autofill::features::kAndroidAutofillSupportForHttpAuth,
    &autofill::features::kAutofillAiCreateEntityDataManager,
    &autofill::features::kAutofillAiWithDataSchema,
    &autofill::features::kAutofillAndroidDesktopKeyboardAccessoryRevamp,
    &autofill::features::kAutofillAndroidDesktopSuppressAccessoryOnEmpty,
    &autofill::features::kAutofillAndroidKeyboardAccessoryDynamicPositioning,
    &autofill::features::kAutofillDeepLinkAutofillOptions,
    &autofill::features::kAutofillEnableAiBasedAmountExtraction,
    &autofill::features::kAutofillEnableBuyNowPayLater,
    &autofill::features::kAutofillEnableCardBenefitsForAmericanExpress,
    &autofill::features::kAutofillEnableCardBenefitsForBmo,
    &autofill::features::kAutofillEnableCvcStorageAndFilling,
    &autofill::features::kAutofillEnableFlatRateCardBenefitsFromCurinos,
    &autofill::features::kAutofillEnableKeyboardAccessoryChipRedesign,
    &autofill::features::kAutofillEnableKeyboardAccessoryChipWidthAdjustment,
    &autofill::features::kAutofillEnableLoyaltyCardsFilling,
    &autofill::features::kAutofillEnableNewCardBenefitsToggleText,
    &autofill::features::kAutofillEnableNewFopDisplayAndroid,
    &autofill::features::kAutofillEnableSecurityTouchEventFilteringAndroid,
    &autofill::features::kAutofillEnableSeparatePixPreferenceItem,
    &autofill::features::kAutofillEnableSupportForHomeAndWork,
    &autofill::features::kAutofillEnableVirtualCardJavaPaymentsDataManager,
    &autofill::features::kAutofillEnableWalletBranding,
    &autofill::features::kAutofillRetryImageFetchOnFailure,
    &autofill::features::kAutofillSyncEwalletAccounts,
    &autofill::features::kAutofillThirdPartyModeContentProvider,
    &autofill::features::kAutofillThirdPartyModeRestoredOnStart,
    &base::features::kBackgroundThreadPoolFieldTrial,
    &base::features::kLowEndMemoryExperiment,
    &base::features::kPostGetMyMemoryStateToBackground,
    &blink::features::kAndroidWindowControlsOverlay,
    &blink::features::kDocumentPictureInPictureAPI,
    &blink::features::kForceWebContentsDarkMode,
    &blink::features::kPrerender2,
    &browser_ui::kAndroidZoomIndicator,
    &browsing_data::features::kBrowsingDataModel,
    &commerce::kCommerceMerchantViewer,
    &commerce::kEnableDiscountInfoApi,
    &commerce::kPriceAnnotations,
    &commerce::kPriceInsights,
    &commerce::kShoppingList,
    &commerce::kShoppingPDPMetrics,
    &content_capture::features::kContentCaptureSendMetadataForDataShare,
    &content_settings::features::kUserBypassUI,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &contextual_tasks::kContextualTasks,
    &credential_management::features::kCredentialManagementThirdPartyWebApiRequestForwarding,
    &data_controls::kEnableClipboardDataControlsAndroid,
    &data_sharing::features::kDataSharingEnableUpdateChromeUI,
    &data_sharing::features::kDataSharingFeature,
    &data_sharing::features::kDataSharingJoinOnly,
    &data_sharing::features::kDataSharingNonProductionEnvironment,
    &data_sharing::features::kSharedDataTypesKillSwitch,
    &download::features::kDownloadNotificationServiceUnifiedAPI,
    &download::features::kEnableSavePackageForOffTheRecord,
    &download::features::kShowBlockedSensitiveDownload,
    &download::features::kShowDownloadScanningState,
    &download::features::kSmartSuggestionForLargeDownloads,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &features::kAbortNavigationsFromTabClosures,
    &features::kAndroidAnimatedProgressBarInBrowser,
    &features::kBackForwardCache,
    &features::kContextMenuEmptySpace,
    &features::kDisplayEdgeToEdgeFullscreen,
    &features::kElasticOverscroll,
    &features::kEnableExclusiveAccessManager,
    &features::kEnableFullscreenToAnyScreenAndroid,
    &features::kFluidResize,
    &features::kGenericSensorExtraClasses,
    &features::kGlic,
    &features::kGlicActorUi,
    &features::kHttpsFirstBalancedMode,
    &features::kLoadingPredictorLimitPreconnectSocketCount,
    &features::kNetworkServiceInProcess,
    &features::kPushMessagingDisallowSenderIDs,
    &features::kPwaUpdateDialogForIcon,
    &features::kQuietNotificationPrompts,
    &features::kSafetyHubDisruptiveNotificationRevocation,
    &features::kSafetyHubLocalPasswordsModule,
    &features::kSafetyHubUnifiedPasswordsModule,
    &features::kSafetyHubWeakAndReusedPasswords,
    &features::kTaskManagerClank,
    &feed::kAndroidOpenIncognitoAsWindow,
    &feed::kFeedAudioOverviews,
    &feed::kFeedContainment,
    &feed::kFeedFollowUiUpdate,
    &feed::kFeedHeaderRemoval,
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedLoadingPlaceholder,
    &feed::kFeedNoViewCache,
    &feed::kFeedPerformanceStudy,
    &feed::kFeedRecyclerBinderUnmountOnDetach,
    &feed::kFeedSignedOutViewDemotion,
    &feed::kInterestFeedV2,
    &feed::kWebFeedAwareness,
    &feed::kWebFeedOnboarding,
    &feed::kWebFeedSort,
    &feed::kXsurfaceMetricsReporting,
    &history::kOrganicRepeatableQueries,
    &history_clusters::internal::kJourneys,
    &kAccountForSuppressedKeyboardInsets,
    &kAdaptiveButtonInTopToolbarCustomizationV2,
    &kAdaptiveButtonInTopToolbarPageSummary,
    &kAlwaysDrawCompositedToolbarHairline,
    &kAndroidAppIntegrationMultiDataSource,
    &kAndroidAppearanceSettings,
    &kAndroidBookmarkBar,
    &kAndroidBookmarkBarFastFollow,
    &kAndroidBottomToolbarV2,
    &kAndroidComposeplate,
    &kAndroidComposeplateLFF,
    &kAndroidContextMenuDuplicateTabs,
    &kAndroidDataImporterService,
    &kAndroidDesktopDensity,
    &kAndroidElegantTextHeight,
    &kAndroidFirstRunLaunchBounds,
    &kAndroidNewMediaPicker,
    &kAndroidNoVisibleHintForDifferentTLD,
    &kAndroidOmniboxFocusedNewTabPage,
    &kAndroidPinnedTabs,
    &kAndroidPinnedTabsTabletTabStrip,
    &kAndroidProgressBarVisualUpdate,
    &kAndroidSearchInSettings,
    &kAndroidSettingsContainment,
    &kAndroidSetupList,
    &kAndroidSurfaceColorUpdate,
    &kAndroidTabDeclutterDedupeTabIdsKillSwitch,
    &kAndroidTabHighlighting,
    &kAndroidTabSkipSaveTabsKillswitch,
    &kAndroidThemeModule,
    &kAndroidThemeResourceProvider,
    &kAndroidToolbarScrollAblation,
    &kAndroidUseAdminsForEnterpriseInfo,
    &kAndroidWindowPopupCustomTabUi,
    &kAndroidWindowPopupLargeScreen,
    &kAndroidWindowPopupPredictFinalBounds,
    &kAndroidWindowPopupResizeAfterSpawn,
    &kAndroidXRUsesSurfaceControl,
    &kAnimatedGifRefactor,
    &kAnimatedImageDragShadow,
    &kAnnotatedPageContentsVirtualStructure,
    &kAppSpecificHistory,
    &kAppSpecificHistoryViewIntent,
    &kAsyncNotificationManager,
    &kAsyncNotificationManagerForDownload,
    &kAutomotiveBackButtonBarStreamline,
    &kAuxiliarySearchDonation,
    &kAuxiliarySearchHistoryDonation,
    &kAvoidDoubleMultiwindowChanges,
    &kBlockIntentsWhileLocked,
    &kBookmarkPaneAndroid,
    &kBottomSheetAsBrowserControls,
    &kBrowserControlsDebugging,
    &kBrowserControlsEarlyResize,
    &kBrowserControlsPersistsOnCvh,
    &kBrowserControlsRenderDrivenShowConstraint,
    &kBrowserWindowInterfaceMobile,
    &kCCTAdaptiveButton,
    &kCCTAdaptiveButtonTestSwitch,
    &kCCTAuthTab,
    &kCCTAuthTabDisableAllExternalIntents,
    &kCCTAuthTabEnableHttpsRedirects,
    &kCCTBlockTouchesDuringEnterAnimation,
    &kCCTClientDataHeader,
    &kCCTContextualMenuItems,
    &kCCTDestroyTabWhenModelIsEmpty,
    &kCCTExtendTrustedCdnPublisher,
    &kCCTFixWarmup,
    &kCCTFreInSameTask,
    &kCCTGoogleBottomBar,
    &kCCTGoogleBottomBarVariantLayouts,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTMinimizedEnabledByDefault,
    &kCCTMultipleParallelRequests,
    &kCCTNavigationMetrics,
    &kCCTNavigationalPrefetch,
    &kCCTNestedSecurityIcon,
    &kCCTOpenInBrowserButtonIfAllowedByEmbedder,
    &kCCTOpenInBrowserButtonIfEnabledByEmbedder,
    &kCCTRealtimeEngagementEventsInBackground,
    &kCCTReportParallelRequestStatus,
    &kCCTReportPrerenderEvents,
    &kCCTResetTimeoutAllowed,
    &kCCTResetTimeoutEnabled,
    &kCCTResizableForThirdParties,
    &kCCTShowTabFix,
    &kCCTTabModalDialog,
    &kCCTToolbarRefactor,
    &kCacheActivityTaskID,
    &kCacheDeprecatedSystemLocationSetting,
    &kCacheIsMultiInstanceApi31Enabled,
    &kCastDeviceFilter,
    &kChangeUnfocusedPriority,
    &kChromeFinds,
    &kChromeItemPickerUi,
    &kChromeNativeUrlOverriding,
    &kChromeSurveyNextAndroid,
    &kClampAutomotiveScaling,
    &kClankStartupLatencyInjection,
    &kClankWhatsNew,
    &kClearIntentWhenRecreated,
    &kCommandLineOnNonRooted,
    &kCompositorViewHolderObscuring,
    &kContextMenuTranslateWithGoogleLens,
    &kContextualSearchDisableOnlineDetection,
    &kContextualSearchSuppressShortView,
    &kControlsVisibilityFromNavigations,
    &kCrossDeviceTabPaneAndroid,
    &kDefaultBrowserPromoAndroid2,
    &kDefaultBrowserPromoEntryPoint,
    &kDefaultBrowserPromoFre,
    &kDesktopAndroidLinkCapturing,
    &kDesktopUAOnConnectedDisplay,
    &kDontAutoHideBrowserControls,
    &kDrawChromePagesEdgeToEdge,
    &kEdgeToEdgeBottomChin,
    &kEdgeToEdgeEverywhere,
    &kEdgeToEdgeExtraLogs,
    &kEdgeToEdgeMonitorConfigurations,
    &kEdgeToEdgeTablet,
    &kEdgeToEdgeUseBackupNavbarInsets,
    &kEducationalTipDefaultBrowserPromoCard,
    &kEmptyTabListAnimationKillSwitch,
    &kEnableAndroidSidePanel,
    &kEnableContextMenuForPreviewTab,
    &kEnableEscapeHandlingForSecondaryActivities,
    &kEnableSwipeToSwitchPane,
    &kEnableToolbarPositioningInResizeMode,
    &kEnableXAxisActivityTransition,
    &kEscCancelDrag,
    &kExperimentsForAgsa,
    &kForceTranslucentNotificationTrampoline,
    &kFullscreenInsetsApiMigration,
    &kFullscreenInsetsApiMigrationOnAutomotive,
    &kGridTabSwitcherSurfaceColorUpdate,
    &kGroupNewTabWithParent,
    &kHistoryPaneAndroid,
    &kHomeModulePrefRefactor,
    &kIncognitoNtpSmallIcon,
    &kIncognitoScreenshot,
    &kIncognitoThemeOverlayTesting,
    &kInlinePdfV2,
    &kKeyboardEscBackNavigation,
    &kLaunchCauseScreenOffFix,
    &kLensOnQuickActionSearchWidget,
    &kLinkHoverStatusBar,
    &kLoadAllTabsAtStartup,
    &kLoadNativeEarly,
    &kLockBackPressHandlerAtStart,
    &kLockTopControlsOnLargeTablets,
    &kLockTopControlsOnLargeTabletsV2,
    &kMagicStackAndroid,
    &kMayLaunchUrlUsesSeparateStoragePartition,
    &kMediaIndicatorsAndroid,
    &kMostVisitedTilesCustomization,
    &kMostVisitedTilesReselect,
    &kMoveToFrontInLaunchIntentDispatcher,
    &kMultiInstanceSharedPrefsMigration,
    &kMvcUpdateViewWhenModelChanged,
    &kNavBarColorAnimation,
    &kNewTabPageCustomizationForMvt,
    &kNewTabPageCustomizationV2,
    &kNotificationPermissionVariant,
    &kNotificationTrampoline,
    &kNotificationTrampolineNoNewTask,
    &kNtpMvcRefactor,
    &kNtpSimplification,
    &kOmahaMinSdkVersionAndroid,
    &kOnDemandBackgroundTabContextCapture,
    &kPCCTMinimumHeight,
    &kPageAnnotationsService,
    &kPageContentProvider,
    &kPartnerCustomizationsUma,
    &kPersistAcrossReboots,
    &kPersistAcrossRebootsDebugLogs,
    &kPowerSavingModeBroadcastReceiverInBackground,
    &kPreconnectOnTabCreation,
    &kPriceChangeModule,
    &kProcessRankPolicyAndroid,
    &kProtectRecentlyVisibleTab,
    &kProtectedTabsAndroid,
    &kPwaRestoreUi,
    &kPwaRestoreUiAtStartup,
    &kReadAloud,
    &kReadAloudAudioOverviews,
    &kReadAloudAudioOverviewsFeedback,
    &kReadAloudIPHMenuButtonHighlightCCT,
    &kReadAloudPlayback,
    &kRecentlyClosedTabsAndWindows,
    &kRecordIncognitoNtpTimeToFirstNavigationMetric,
    &kRecordSuppressionMetrics,
    &kReengagementNotification,
    &kRelatedSearchesAllLanguage,
    &kRelatedSearchesSwitch,
    &kReloadTabUiResourcesIfChanged,
    &kRemoveTabFocusOnShowingAndSelect,
    &kRightEdgeGoesForwardGestureNav,
    &kRobustWindowManagement,
    &kRobustWindowManagementExperimental,
    &kSearchInCCT,
    &kSearchInCCTAlternateTapHandling,
    &kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder,
    &kSearchInCCTIfEnabledByEmbedder,
    &kSearchResumptionModuleAndroid,
    &kSettingsMultiColumn,
    &kSettingsSingleActivity,
    &kShareCustomActionsInCCT,
    &kShortCircuitUnfocusAnimation,
    &kShowCloseAllIncognitoTabsButton,
    &kShowTabListAnimations,
    &kSmallerTabStripTitleLimit,
    &kStartSurfaceReturnTime,
    &kSubmenusInAppMenu,
    &kSubmenusTabContextMenuLffTabStrip,
    &kSuppressToolbarCapturesAtGestureEnd,
    &kTabBottomSheet,
    &kTabClosureMethodRefactor,
    &kTabModelInitFixes,
    &kTabStorageSqlitePrototype,
    &kTabStripAutoSelectOnCloseChange,
    &kTabStripCloseRefactorAndroid,
    &kTabStripDensityChangeAndroid,
    &kTabStripEmptySpaceContextMenuAndroid,
    &kTabSwitcherDragDropAndroid,
    &kTabSwitcherGroupSuggestionsAndroid,
    &kTabSwitcherGroupSuggestionsTestModeAndroid,
    &kTabWindowManagerReportIndicesMismatch,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kTinkerTankBottomSheet,
    &kToolbarPhoneAnimationRefactor,
    &kToolbarSnapshotRefactor,
    &kToolbarStaleCaptureBugFix,
    &kToolbarTabletResizeRefactor,
    &kTopControlsRefactor,
    &kTopControlsRefactorV2,
    &kTouchToSearchCallout,
    &kTrustedWebActivityContactsDelegation,
    &kUmaBackgroundSessions,
    &kUmaSessionCorrectnessFixes,
    &kUnparcelIntentFileDescriptors,
    &kUpdateCompositorForSurfaceControl,
    &kUseActivityManagerForTabActivation,
    &kUseInitialNetworkStateAtStartup,
    &kUseLibunwindstackNativeUnwinderAndroid,
    &kWebApkMinShellVersion,
    &kWebOtpCrossDeviceSimpleString,
    &kXplatSyncedSetup,
    &language::kCctAutoTranslate,
    &language::kDetailedLanguageSettings,
    &media::kAutoDocPiPPermissionPromptAndroid,
    &media::kAutoPictureInPictureAndroid,
    &media::kContextMenuPictureInPictureAndroid,
    &net::features::kVerifyQWACs,
    &network::features::kLocalNetworkAccessChecks,
    &network::features::kLocalNetworkAccessChecksSplitPermissions,
    &notifications::features::kUseChimeAndroidSdk,
    &page_info::kPageInfoAboutThisSiteMoreLangs,
    &paint_preview::kPaintPreviewDemo,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::kPasswordFormGroupedAffiliations,
    &payments::facilitated::kFacilitatedPaymentsEnableA2APayment,
    &permissions::features::kAndroidWindowManagementWebApi,
    &permissions::features::kPermissionDedicatedCpssSettingAndroid,
    &permissions::features::kPermissionsPromptSurvey,
    &plus_addresses::features::kPlusAddressAndroidOpenGmsCoreManagementPage,
    &plus_addresses::features::kPlusAddressesEnabled,
    &privacy_sandbox::kPrivacySandboxActivityTypeStorage,
    &privacy_sandbox::kPrivacySandboxAdTopicsContentParity,
    &privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements,
    &privacy_sandbox::kPrivacySandboxSettings4,
    &privacy_sandbox::kRelatedWebsiteSetsUi,
    &safe_browsing::kAutoRevokeSuspiciousNotification,
    &safe_browsing::kExtendedReportingRemovePrefDependency,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &safe_browsing::kMaliciousApkDownloadCheck,
    &safe_browsing::kReportNotificationContentDetectionData,
    &safe_browsing::kShowWarningsForSuspiciousNotifications,
    &safety_check::features::kSafetyHub,
    &segmentation_platform::features::kAndroidAppIntegrationModule,
    &segmentation_platform::features::kAndroidTipsNotifications,
    &segmentation_platform::features::kAndroidTipsNotificationsV2,
    &segmentation_platform::features::kContextualPageActionTabGrouping,
    &segmentation_platform::features::kContextualPageActions,
    &segmentation_platform::features::kSegmentationPlatformAndroidHomeModuleRanker,
    &segmentation_platform::features::kSegmentationPlatformAndroidHomeModuleRankerV2,
    &segmentation_platform::features::kSegmentationPlatformEphemeralCardRanker,
    &sensitive_content::features::kSensitiveContent,
    &sensitive_content::features::kSensitiveContentWhileSwitchingTabs,
    &site_isolation::features::kSiteIsolationEnableMemoryThresholdAndroid,
    &switches::kClankDefaultSearchApi,
    &switches::kRestrictLegacySearchEnginePromoOnFormFactors,
    &sync_preferences::features::kCrossDevicePrefTrackerExtraLogs,
    &sync_sessions::kOptimizeAssociateWindowsAndroid,
    &syncer::kSyncEnableNewSyncDashboardUrl,
    &syncer::kSyncEnablePasswordsSyncErrorMessageAlternative,
    &syncer::kSyncTrustedVaultErrorMessageDuration,
    &syncer::kUnoPhase2FollowUp,
    &syncer::kWebApkBackupAndRestoreBackend,
    &tab_groups::kUseAlternateHistorySyncIllustration,
    &variations::kOmniboxAutofocusOnIncognitoNtp,
    &visited_url_ranking::features::kGroupSuggestionService,
    &visited_url_ranking::features::kVisitedURLRankingService,
    &webapps::features::kAndroidAutoMintedTWA,
    &webapps::features::kAndroidTWAOriginDisplay,
    &webapps::features::kWebApkInstallFailureNotification,
// go/keep-sorted end
// FEATURE_EXPORT_LIST_END
};
// LINT.ThenChange(//chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java:FeaturesExposedToJava)

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_ChromeFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

// Alphabetical:
// BASE_FEATURE_START
// go/keep-sorted start sticky_comments=yes

BASE_FEATURE(kAccountForSuppressedKeyboardInsets, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAdaptiveButtonInTopToolbarPageSummary, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAlwaysDrawCompositedToolbarHairline, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAppIntegrationMultiDataSource, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAppearanceSettings, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBookmarkBar, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBookmarkBarFastFollow, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBottomToolbarV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidComposeplate, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidComposeplateAllLocales, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidComposeplateLFF, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidComposeplateLFFAllLocales, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidContextMenuDuplicateTabs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidDataImporterService, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidDesktopDensity, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidElegantTextHeight, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidFirstRunLaunchBounds, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidNewMediaPicker, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidNoVisibleHintForDifferentTLD, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidOmniboxFocusedNewTabPage, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidPinnedTabs, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidPinnedTabsTabletTabStrip, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidProgressBarVisualUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSearchInSettings,"SearchInSettings", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSettingsContainment, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSetupList, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSurfaceColorUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabDeclutterDedupeTabIdsKillSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabHighlighting, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabSkipSaveTabsKillswitch,"AndroidTabSkipSaveTabsTaskKillswitch", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidThemeModule, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidThemeResourceProvider, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidToolbarScrollAblation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidUseAdminsForEnterpriseInfo, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidWindowPopupCustomTabUi, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidWindowPopupLargeScreen, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidWindowPopupPredictFinalBounds, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidWindowPopupResizeAfterSpawn, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidXRUsesSurfaceControl, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAnimatedGifRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAnimatedImageDragShadow, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAnnotatedPageContentsVirtualStructure, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAppSpecificHistory, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAppSpecificHistoryViewIntent, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAsyncNotificationManager, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAsyncNotificationManagerForDownload, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutomotiveBackButtonBarStreamline, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAuxiliarySearchDonation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAuxiliarySearchHistoryDonation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAvoidDoubleMultiwindowChanges, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBlockIntentsWhileLocked, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBookmarkPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBottomSheetAsBrowserControls, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsDebugging, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsEarlyResize, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsPersistsOnCvh, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsRenderDrivenShowConstraint, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserWindowInterfaceMobile, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAdaptiveButton, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAdaptiveButtonTestSwitch, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAuthTab, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAuthTabDisableAllExternalIntents, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAuthTabEnableHttpsRedirects, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTBlockTouchesDuringEnterAnimation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTClientDataHeader, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTContextualMenuItems, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTDestroyTabWhenModelIsEmpty, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTExtendTrustedCdnPublisher, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTFixWarmup, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTFreInSameTask, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTGoogleBottomBar, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTGoogleBottomBarVariantLayouts, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTIncognitoAvailableToThirdParty, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTMinimizedEnabledByDefault, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTMultipleParallelRequests, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNavigationMetrics, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNavigationalPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNestedSecurityIcon, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTOpenInBrowserButtonIfAllowedByEmbedder, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTOpenInBrowserButtonIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTRealtimeEngagementEventsInBackground, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTReportParallelRequestStatus, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTReportPrerenderEvents, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTResetTimeoutAllowed, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTResetTimeoutEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTResizableForThirdParties, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTShowTabFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTTabModalDialog, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTToolbarRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheActivityTaskID, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheDeprecatedSystemLocationSetting, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheIsMultiInstanceApi31Enabled, base::FEATURE_ENABLED_BY_DEFAULT);
// Used in downstream code.
BASE_FEATURE(kCastDeviceFilter, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChangeUnfocusedPriority, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeFinds, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeItemPickerUi, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeNativeUrlOverriding, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeSurveyNextAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kClampAutomotiveScaling, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kClankStartupLatencyInjection, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kClankWhatsNew, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kClearIntentWhenRecreated, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCommandLineOnNonRooted, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCompositorViewHolderObscuring, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kContextMenuTranslateWithGoogleLens, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualSearchDisableOnlineDetection, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualSearchSuppressShortView, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kControlsVisibilityFromNavigations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCrossDeviceTabPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoAndroid2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoEntryPoint, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoFre, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidLinkCapturing, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopUAOnConnectedDisplay, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDiscardPageWithCrashedSubframePolicy, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDiscardPageWithCrashedSubframeRelaxedEligibility, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDontAutoHideBrowserControls, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDrawChromePagesEdgeToEdge, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeBottomChin, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeEverywhere, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeExtraLogs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeMonitorConfigurations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeTablet, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeUseBackupNavbarInsets, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEducationalTipDefaultBrowserPromoCard, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEmptyTabListAnimationKillSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAndroidSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableContextMenuForPreviewTab, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableEscapeHandlingForSecondaryActivities, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableSwipeToSwitchPane, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableToolbarPositioningInResizeMode, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableXAxisActivityTransition, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEscCancelDrag, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExperimentsForAgsa, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kForceTranslucentNotificationTrampoline, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFullscreenInsetsApiMigration, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGridTabSwitcherSurfaceColorUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGroupNewTabWithParent, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kHistoryPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHomeModulePrefRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoNtpSmallIcon, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoScreenshot, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoThemeOverlayTesting, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInlinePdfV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kKeyboardEscBackNavigation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLaunchCauseScreenOffFix, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLensOnQuickActionSearchWidget, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLinkHoverStatusBar, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLoadAllTabsAtStartup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLoadNativeEarly, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLockBackPressHandlerAtStart, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLockTopControlsOnLargeTablets, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLockTopControlsOnLargeTabletsV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMagicStackAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
// Enables an experimental feature which forces mayLaunchUrl to use a different
// storage partition. This may reduce performance. This should not be enabled by
// default.
BASE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMediaIndicatorsAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMostVisitedTilesCustomization, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kMostVisitedTilesReselect, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMoveToFrontInLaunchIntentDispatcher, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMultiInstanceSharedPrefsMigration, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMvcUpdateViewWhenModelChanged, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNavBarColorAnimation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNewTabPageCustomizationForMvt, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNewTabPageCustomizationV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationPermissionVariant, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationTrampoline, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationTrampolineNoNewTask, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNtpMvcRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNtpSimplification, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOmahaMinSdkVersionAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOnDemandBackgroundTabContextCapture, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPCCTMinimumHeight, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageAnnotationsService, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPageContentProvider, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPartnerCustomizationsUma, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPersistAcrossReboots, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPersistAcrossRebootsDebugLogs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPowerSavingModeBroadcastReceiverInBackground, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPreconnectOnTabCreation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPriceChangeModule, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kProcessRankPolicyAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kProtectRecentlyVisibleTab, base::FEATURE_DISABLED_BY_DEFAULT);
// Put a higher memory priority to protected background tabs (e.g. tabs with
// user edits in forms) to prevent them from being killed by LMKD before any
// other non-protected tabs.
BASE_FEATURE(kProtectedTabsAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPwaRestoreUi, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPwaRestoreUiAtStartup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloud, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudAudioOverviews, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudAudioOverviewsFeedback, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudPlayback, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudServerExperiments, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRecentlyClosedTabsAndWindows, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRecordIncognitoNtpTimeToFirstNavigationMetric, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRecordSuppressionMetrics, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReengagementNotification, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRelatedSearchesAllLanguage, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRelatedSearchesSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReloadTabUiResourcesIfChanged, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRemoveTabFocusOnShowingAndSelect, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRightEdgeGoesForwardGestureNav, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRobustWindowManagement, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRobustWindowManagementExperimental, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTAlternateTapHandling, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSearchResumptionModuleAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsMultiColumn, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsSingleActivity, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShareCustomActionsInCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShortCircuitUnfocusAnimation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShowCloseAllIncognitoTabsButton, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShowTabListAnimations, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSmallerTabStripTitleLimit, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kStartSurfaceReturnTime, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSubmenusInAppMenu, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSubmenusTabContextMenuLffTabStrip, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSuppressToolbarCapturesAtGestureEnd, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabBottomSheet, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabClosureMethodRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabModelInitFixes, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStorageSqlitePrototype, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripAutoSelectOnCloseChange, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripCloseRefactorAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripDensityChangeAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripEmptySpaceContextMenuAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherDragDropAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherGroupSuggestionsAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherGroupSuggestionsTestModeAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabWindowManagerReportIndicesMismatch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestDefaultDisabled, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestDefaultEnabled, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTinkerTankBottomSheet, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarPhoneAnimationRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarSnapshotRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarStaleCaptureBugFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarTabletResizeRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTopControlsRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTopControlsRefactorV2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTouchToSearchCallout, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrustedWebActivityContactsDelegation, base::FEATURE_DISABLED_BY_DEFAULT);
// If enabled, keep logging and reporting UMA while chrome is backgrounded.
BASE_FEATURE(kUmaBackgroundSessions,"UMABackgroundSessions", base::FEATURE_ENABLED_BY_DEFAULT);
// Correctness fixes to Activity tagging for UMA sessions.
BASE_FEATURE(kUmaSessionCorrectnessFixes, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUnparcelIntentFileDescriptors, base::FEATURE_ENABLED_BY_DEFAULT);
// Actively update the compositor surface when surface control is enabled.
BASE_FEATURE(kUpdateCompositorForSurfaceControl, base::FEATURE_ENABLED_BY_DEFAULT);
// Activate tab with moveTaskToFront() which works in multi-window mode.
BASE_FEATURE(kUseActivityManagerForTabActivation, base::FEATURE_ENABLED_BY_DEFAULT);
// Whether to use initial network state during initialization to speed up
// startup.
BASE_FEATURE(kUseInitialNetworkStateAtStartup, base::FEATURE_ENABLED_BY_DEFAULT);
// Use the LibunwindstackNativeUnwinderAndroid for only browser main thread, and
// only on Android.
BASE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
// Shows only the remote device name on the Android notification instead of
// a descriptive text.
BASE_FEATURE(kWebOtpCrossDeviceSimpleString, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kXplatSyncedSetup, base::FEATURE_ENABLED_BY_DEFAULT);
// go/keep-sorted end
// BASE_FEATURE_END

// clang-format on

}  // namespace android
}  // namespace chrome

DEFINE_JNI(ChromeFeatureMap)
