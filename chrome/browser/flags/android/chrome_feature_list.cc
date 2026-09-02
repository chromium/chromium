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
#include "cc/base/features.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/finds/core/finds_features.h"
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
#include "components/enterprise/data_protection/features.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lens/lens_features.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/push_messaging/push_messaging_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safety_check/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/security_interstitials/core/features.h"
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
#include "components/tab_groups/features.h"
#include "components/themes/cross_device/features.h"
#include "components/touch_to_search/core/browser/contextual_search_field_trial.h"
#include "components/universal_optout/features.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/viz/common/features.h"
#include "components/webapps/browser/features.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "gpu/config/gpu_finch_features.h"
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
    &autofill::features::kAutofillAiAvailableByDefault,
    &autofill::features::kAutofillAiEditEntitiesFromSaveUpdatePrompt,
    &autofill::features::kAutofillAiLimitSuggestionWidth,
    &autofill::features::kAutofillAiOnlineModelToggleNewTitle,
    &autofill::features::kAutofillAiReauthRequired,
    &autofill::features::kAutofillAiShowDialogInSettingsWhenUpstreamingFails,
    &autofill::features::kAutofillAiShowWalletDisabledBanner,
    &autofill::features::kAutofillAiUseMaterialDatePickerInEntityEditor,
    &autofill::features::kAutofillAiUsePrivateAi,
    &autofill::features::kAutofillAiWalletPassBranding2026,
    &autofill::features::kAutofillAiWalletPrivatePassesDeepLink,
    &autofill::features::kAutofillAiWalletShopping,
    &autofill::features::kAutofillAiWithDataSchema,
    &autofill::features::kAutofillAmbientAutofill,
    &autofill::features::kAutofillAndroidDesktopKeyboardAccessoryRevamp,
    &autofill::features::kAutofillAndroidDesktopSuppressAccessoryOnEmpty,
    &autofill::features::kAutofillAndroidKeyboardAccessoryDynamicPositioning,
    &autofill::features::kAutofillAndroidKeyboardAccessoryHoverPreview,
    &autofill::features::kAutofillAtMemory,
    &autofill::features::kAutofillEnableAiBasedAmountExtraction,
    &autofill::features::kAutofillEnableBuyNowPayLater,
    &autofill::features::kAutofillEnableGradientGoogleLogos,
    &autofill::features::kAutofillEnableNewCardBenefitsToggleText,
    &autofill::features::kAutofillEnableNewFopDisplayAndroid,
    &autofill::features::kAutofillEnablePayNowPayLaterTabs,
    &autofill::features::kAutofillEnableScanCardOptionWhenNoCardsSaved,
    &autofill::features::kAutofillEnableSecurityTouchEventFilteringAndroid,
    &autofill::features::kAutofillEnableSeparatePixPreferenceItem,
    &autofill::features::kAutofillEnableVirtualCardJavaPaymentsDataManager,
    &autofill::features::kAutofillEnableWalletBranding,
    &autofill::features::kAutofillEnableWalletBrandingV2,
    &autofill::features::kAutofillEnableWalletDisclosureNoticePublicPass,
    &autofill::features::kAutofillEnableWalletReminderNotice,
    &autofill::features::kAutofillEnableWalletReminderNoticePublicPass,
    &autofill::features::kAutofillRetryImageFetchOnFailure,
    &autofill::features::kAutofillSyncEwalletAccounts,
    &autofill::features::kResetNativePointerInCreditCardAuthDialog,
    &base::features::kBackgroundThreadPoolFieldTrial,
    &base::features::kLowEndMemoryExperiment,
    &base::features::kShutdownPreNativeThreadPoolAfterStartup,
    &blink::features::kDocumentPictureInPictureAPI,
    &blink::features::kForceWebContentsDarkMode,
    &blink::features::kPrerender2,
    &browsing_data::features::kBrowsingDataModel,
    &browsing_data::features::kDbdPasswordRemovalOnAndroid,
    &commerce::kCommerceMerchantViewer,
    &commerce::kEnableDiscountInfoApi,
    &commerce::kPriceAnnotations,
    &commerce::kPriceInsights,
    &commerce::kShoppingList,
    &commerce::kShoppingPDPMetrics,
    &content_capture::features::kContentCaptureSendMetadataForDataShare,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &contextual_tasks::kContextualTasks,
    &contextual_tasks::kContextualTasksSidePanel,
    &credential_management::features::kCredentialManagementThirdPartyWebApiRequestForwarding,
    &data_controls::kDataControlsSearchWith,
    &data_sharing::features::kDataSharingEnableUpdateChromeUI,
    &data_sharing::features::kDataSharingFeature,
    &data_sharing::features::kDataSharingJoinOnly,
    &download::features::kDownloadNotificationServiceUnifiedAPI,
    &download::features::kEnableDownloadSaveAsContextMenu,
    &download::features::kEnableDownloadSaveAsSystemFileDialog,
    &download::features::kEnableSavePackageForOffTheRecord,
    &download::features::kOpenDownloadInFilesAppIfNoHandlerFound,
    &download::features::kOpenDownloadInNewTab,
    &download::features::kShowBlockedSensitiveDownload,
    &download::features::kShowDownloadScanningState,
    &download::features::kSmartSuggestionForLargeDownloads,
    &enterprise_data_protection::kEnableAndroidEnterpriseScreenshotProtection,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &features::kAAPMBlocksWebGPU,
    &features::kAbortNavigationsFromTabClosures,
    &features::kAiOverlayDialog,
    &features::kAndroidAnimatedProgressBarInBrowser,
    &features::kBackForwardCache,
    &features::kBrowserControlsScrollSnapAnimation,
    &features::kDisplayEdgeToEdgeFullscreen,
    &features::kElasticOverscroll,
    &features::kEmailVerificationProtocol,
    &features::kEnableExclusiveAccessManager,
    &features::kEnableFullscreenToAnyScreenAndroid,
    &features::kFluidResize,
    &features::kGenericSensorExtraClasses,
    &features::kGlic,
    &features::kGlicBackgroundActuation,
    &features::kGlicBackgroundTriggering,
    &features::kHttpsFirstBalancedMode,
    &features::kLoadingPredictorLimitPreconnectSocketCount,
    &features::kMigrateManagementPageToWebUIOnMobile,
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
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedLoadingPlaceholder,
    &feed::kFeedNoViewCache,
    &feed::kFeedPerformanceStudy,
    &feed::kFeedRecyclerBinderUnmountOnDetach,
    &feed::kFeedSignedOutViewDemotion,
    &feed::kInterestFeedV2,
    &feed::kWideScreenFeedForFoldables,
    &feed::kXsurfaceMetricsReporting,
    &finds::features::kChromeFinds,
    &history::kOrganicRepeatableQueries,
    &kAccountForSuppressedKeyboardInsets,
    &kAccountPickerDialog,
    &kActorLiveNotification,
    &kActorNotificationIntentRouting,
    &kActorStepProgressNotification,
    &kAllocInstanceIdIncreasedDefaultRange,
    &kAllowMultipleMediaNotifications,
    &kAlwaysDrawCompositedToolbarHairline,
    &kAndroidActorTaskTimeout,
    &kAndroidAppIntegrationMultiDataSource,
    &kAndroidAppRatingPrompt,
    &kAndroidAutofillPrefObserver,
    &kAndroidBottomBar,
    &kAndroidBottomBarAim,
    &kAndroidBricksNativePage,
    &kAndroidContextMenuDisabledMenuItems,
    &kAndroidDesktopBookmarkLayout,
    &kAndroidDesktopBookmarkPopup,
    &kAndroidDesktopHistoryLayout,
    &kAndroidDeviceSignalsDisclaimer,
    &kAndroidElegantTextHeight,
    &kAndroidFirstRunLaunchBounds,
    &kAndroidFreLayoutUpdate,
    &kAndroidHistoryClustering,
    &kAndroidKeyboardShortcutOpenFile,
    &kAndroidNoCaptureWhenScrollingDisabledOnDesktop,
    &kAndroidNoVisibleHintForDifferentTLD,
    &kAndroidOmniboxFocusedNewTabPage,
    &kAndroidOpenIncognitoAsWindowRestrictions,
    &kAndroidPageInfoAsAppMenuItem,
    &kAndroidProgressBarVisualUpdate,
    &kAndroidSaveCardNonBlockingDialog,
    &kAndroidSettingsContainment,
    &kAndroidSettingsUrl,
    &kAndroidSetupList,
    &kAndroidStartupImprovements,
    &kAndroidSurfaceColorUpdate,
    &kAndroidTabDeclutterArchiveOnDesktop,
    &kAndroidTabDeclutterDedupeTabIdsKillSwitch,
    &kAndroidTabSkipSaveTabsKillswitch,
    &kAndroidTabstripStartupCaptureBugFix,
    &kAndroidThemeModule,
    &kAndroidThemeResourceProvider,
    &kAndroidToolbarScrollAblation,
    &kAndroidVerticalTabs,
    &kAndroidXRUsesSurfaceControl,
    &kAndroidXrImmersivePlayer,
    &kAndroidZoomImmersive,
    &kAnimatedGifRefactor,
    &kAnimatedImageDragShadow,
    &kAnnotatedPageContentsVirtualStructure,
    &kApb144Patch1,
    &kApb144Patch2,
    &kApb144Patch3,
    &kApb144Patch4,
    &kApb144Patch6,
    &kApb144Patch7,
    &kApb144Patch8,
    &kApb144Patch9,
    &kAppSpecificHistory,
    &kAppSpecificHistoryViewIntent,
    &kArchivedTabsTeardown,
    &kAsyncNotificationManager,
    &kAsyncNotificationManagerForDownload,
    &kAutomotiveBackButtonBarStreamline,
    &kAuxiliarySearchDonation,
    &kAuxiliarySearchHistoryDonation,
    &kAvoidDoubleMultiwindowChanges,
    &kBackGestureReflectsDesktopBehavior,
    &kBlockIntentsWhileLocked,
    &kBookmarkPaneAndroid,
    &kBookmarksBarContextMenu,
    &kBookmarksBarNTP,
    &kBottomSheetAsBrowserControls,
    &kBottomSheetOnDesktopWindowing,
    &kBrowserControlsDebugging,
    &kBrowserControlsEarlyResize,
    &kBrowserControlsHidingToken,
    &kBrowserControlsPersistsOnCvh,
    &kBrowserControlsRenderDrivenShowConstraint,
    &kCCTAdaptiveButton,
    &kCCTAdaptiveButtonTestSwitch,
    &kCCTBlockTouchesDuringEnterAnimation,
    &kCCTClientDataHeader,
    &kCCTContextualMenuItems,
    &kCCTDestroyTabWhenModelIsEmpty,
    &kCCTDontOverrideIntentMimeType,
    &kCCTExtendTrustedCdnPublisher,
    &kCCTFreInSameTask,
    &kCCTGoogleBottomBar,
    &kCCTGoogleBottomBarVariantLayouts,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTMinimizedEnabledByDefault,
    &kCCTNavigationInfoScreenshot,
    &kCCTNavigationMetrics,
    &kCCTNavigationalPrefetch,
    &kCCTOpenInBrowserButtonIfAllowedByEmbedder,
    &kCCTOpenInBrowserButtonIfEnabledByEmbedder,
    &kCCTPageContentRequestAllowed,
    &kCCTPageContentRequestEnabled,
    &kCCTRealtimeEngagementEventsInBackground,
    &kCCTReportParallelRequestStatus,
    &kCCTReportPrerenderEvents,
    &kCCTResetTimeoutAllowed,
    &kCCTResizableForThirdParties,
    &kCCTTabModalDialog,
    &kCCTTabSwitcherEnabledForChromeExperiment,
    &kCCTTabSwitcherEnabledForEmbedderExperiment,
    &kCacheDeprecatedSystemLocationSetting,
    &kCacheIsGoogleSigned,
    &kCacheIsMultiInstanceApi31Enabled,
    &kCastDeviceFilter,
    &kCctTabResumption,
    &kChangeUnfocusedPriority,
    &kChromeNativeUrlOverriding,
    &kChromeSurveyNextAndroid,
    &kClampAutomotiveScaling,
    &kClankGlicContextMenu,
    &kClankStartupLatencyInjection,
    &kClankWhatsNew,
    &kClearIntentWhenRecreated,
    &kCommandLineOnNonRooted,
    &kCompositorViewRemeasureFix,
    &kContextualPanelCloseButton,
    &kContextualSearchDisableOnlineDetection,
    &kContextualSearchSuppressShortView,
    &kControlsInBrowserToolbarSwipeMock,
    &kControlsVisibilityFromNavigations,
    &kCopyLinkToHighlight,
    &kCrossDeviceTabPaneAndroid,
    &kCrossDeviceTaskHandoff,
    &kCrossWindowTabGroupOperations,
    &kDebugToolbarPositioning,
    &kDefaultBrowserPromoAndroid2,
    &kDefaultBrowserPromoEntryPoint,
    &kDefaultBrowserPromoFre,
    &kDeferNavigationStateChanged,
    &kDesktopAndroidLinkCapturing,
    &kDesktopAndroidTWADeleteBrowserData,
    &kDesktopAndroidTWADisclosures,
    &kDesktopAndroidTWADisclosuresHelpLink,
    &kDesktopUAOnConnectedDisplay,
    &kDisableGridTabSwitcher,
    &kDisablePartnerHomepageAndroid,
    &kDisableScrollbarOfFadingEdgeScrollView,
    &kDontAutoHideBrowserControls,
    &kEdgeToEdgeAutomotive,
    &kEdgeToEdgeBottomChin,
    &kEdgeToEdgeEverywhere,
    &kEdgeToEdgeExtraLogs,
    &kEdgeToEdgeMonitorConfigurations,
    &kEdgeToEdgeTablet,
    &kEdgeToEdgeUseBackupNavbarInsets,
    &kEdgelessTopInset,
    &kEnableAndroidSidePanel,
    &kEnableAndroidSidePanelDevFeature,
    &kEnableAndroidSidePanelLogs,
    &kEnableBrowserWindowInterfaceForCustomTabActivity,
    &kEnableEscapeHandlingForSecondaryActivities,
    &kEnableSwipeToSwitchPane,
    &kEnableToolbarPositioningInResizeMode,
    &kEnableXAxisActivityTransition,
    &kEnforceIncognitoIsolation,
    &kExperimentsForAgsa,
    &kFaviconDisableHostFallback,
    &kFlyoutInBookmarksBar,
    &kForceTranslucentNotificationTrampoline,
    &kFullscreenInsetsApiMigration,
    &kFullscreenInsetsApiMigrationOnAutomotive,
    &kGestureUserEducationBackSwipe,
    &kGmsCoreBindServiceOptimization,
    &kGridTabSwitcherSurfaceColorUpdate,
    &kHistoryPaneAndroid,
    &kHomeButtonRemoval,
    &kInAppUpdateFlow,
    &kInAppWindowManagerDeprecation,
    &kIncognitoAsWindowFullScreen,
    &kIncognitoModeForcedAndroid,
    &kIncognitoScreenshot,
    &kIncognitoThemeOverlayTesting,
    &kInlinePdfV2,
    &kInlinePdfV2Download,
    &kInlinePdfV2Incognito,
    &kKeyboardEscBackNavigation,
    &kLaunchCauseScreenOffFix,
    &kLensOnQuickActionSearchWidget,
    &kLinkHoverStatusBar,
    &kLoadAllTabsAtStartup,
    &kLockTopControlsOnLargeTabletsV2,
    &kLongScreenshotsLenientMemoryCheck,
    &kLongScreenshotsNoMemoryCheck,
    &kMayLaunchUrlUsesSeparateStoragePartition,
    &kMostVisitedTilesCustomization,
    &kMostVisitedTilesReselect,
    &kMoveToFrontInLaunchIntentDispatcher,
    &kMultiInstanceSharedPrefsMigration,
    &kNavigationListMenu,
    &kNotificationPermissionVariant,
    &kNotificationTrampoline,
    &kNotificationTrampolineNoNewTask,
    &kNtpAurora,
    &kNtpAuroraV2,
    &kNtpMvcRefactor,
    &kNtpVision,
    &kOmahaMinSdkVersionAndroid,
    &kOnDemandBackgroundTabContextCapture,
    &kOnDemandBackgroundTabContextCaptureOptimization,
    &kOnStartupWindowPolicy,
    &kOneStepAimAccess,
    &kOpenDownloadInPreferredApp,
    &kPCCTMinimumHeight,
    &kPageAnnotationsService,
    &kPageContentProvider,
    &kPartnerCustomizationsUma,
    &kPdfLauncherActivity,
    &kPdfReuseFragment,
    &kPersistAcrossReboots,
    &kPersistAcrossRebootsDebugLogs,
    &kPowerSavingModeBroadcastReceiverInBackground,
    &kPreconnectOnTabCreation,
    &kPriceChangeModule,
    &kPrintSelectionMenu,
    &kProtectRecentlyVisibleTab,
    &kPwaRestoreUi,
    &kPwaRestoreUiAtStartup,
    &kQueuedCompositorWebContentsUpdates,
    &kReadAloudAudioOverviews,
    &kReadAloudIPHMenuButtonHighlightCCT,
    &kRecordSuppressionMetrics,
    &kReengagementNotification,
    &kRelatedSearchesAllLanguage,
    &kRelatedSearchesSwitch,
    &kRemoveTabFocusOnShowingAndSelect,
    &kRobustWindowManagementExperimental,
    &kSafetyFrePromo,
    &kScheduleWindowCleaning,
    &kSearchInCCT,
    &kSearchInCCTAlternateTapHandling,
    &kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder,
    &kSearchInCCTIfEnabledByEmbedder,
    &kSessionRestoreAfterCrash,
    &kSettingsInTab,
    &kSettingsInTabUrlNav,
    &kSettingsMultiColumn,
    &kSettingsSingleActivity,
    &kShareCustomActionsInCCT,
    &kShortCircuitUnfocusAnimation,
    &kShowTabListAnimations,
    &kSidePanelTopHairlineRefactorAndroid,
    &kSmallerTabStripTitleLimit,
    &kStartSurfaceReturnTime,
    &kSubmenusInAppMenu,
    &kSubmenusInAppMenuLff,
    &kSyncRestoreOnStartupPref,
    &kTabAndroidGracefulShutdown,
    &kTabBottomSheet,
    &kTabBottomSheetFullHeight,
    &kTabBottomSheetHalfHeight,
    &kTabBottomSheetResizeWebview,
    &kTabClosureCommittedMethodRefactor,
    &kTabClosureMethodRefactor,
    &kTabOpenerTracking,
    &kTabSearchForDesktop,
    &kTabSharingToolbarAndroid,
    &kTabStorageSqlitePrototype,
    &kTabStripAutoSelectOnCloseChange,
    &kTabStripHeightTransitionGlitchFix,
    &kTabStripLayoutTransitionDebounceFix,
    &kTabStripStopSpinnerOnLoadStop,
    &kTabSwitcherDragDropAndroid,
    &kTabSwitcherGroupSuggestionsAndroid,
    &kTabSwitcherGroupSuggestionsTestModeAndroid,
    &kTabSwitcherMessagesOnDesktopWindowingKillSwitch,
    &kTabWindowManagerReportIndicesMismatch,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kTextHighlightFullLink,
    &kThreeDotMenuBackButton,
    &kTipsSelfService,
    &kToolbarCaptureFixForSPAs,
    &kToolbarPhoneAnimationRefactor,
    &kToolbarProgressBarRefactor,
    &kToolbarSnapshotRefactor,
    &kToolbarTabletResizeRefactor,
    &kTouchToSearchCallout,
    &kTrustedWebActivityContactsDelegation,
    &kTweakApplicationPreloadLoadNativeFirst,
    &kTweakApplicationPreloadMoveWarmUp,
    &kTweakApplicationPreloadSkipNewInstance,
    &kTweakApplicationPreloadSkipWarmUp,
    &kUmaBackgroundSessions,
    &kUmaSessionCorrectnessFixes,
    &kUniversalKeyboardHandling,
    &kUnparcelIntentFileDescriptors,
    &kUseActivityManagerForTabActivation,
    &kUseAppTaskForCustomTabActivation,
    &kUseLibunwindstackNativeUnwinderAndroid,
    &kUseWebUiNtpAndroid,
    &kVerifyStartupSigninState,
    &kVirtualKeyboardResizesContentTransientOvershootFix,
    &kVirtualKeyboardTransientInnerHeightFix,
    &kWebApkMinShellVersion,
    &kWebAppNavigationBarThemeColor,
    &kWebAppShortEdgesCutoutMode,
    &kWebOtpCrossDeviceSimpleString,
    &kWebUiAndroidTheming,
    &kXplatSyncedSetupThemes,
    &kYourSavedInfoSettingsPageAndroid,
    &language::kCctAutoTranslate,
    &language::kDetailedLanguageSettings,
    &language::kGmsCoreUlp,
    &lens::features::kLensBypassCompressionForC2pa,
    &lens::features::kLensOverlayAndroid,
    &lens::features::kLensSendRawFileMediaTypes,
    &media::kAutoDocPiPPermissionPromptAndroid,
    &media::kAutoPictureInPictureAndroid,
    &media::kContextMenuCopyVideoFrame,
    &media::kContextMenuPictureInPictureAndroid,
    &media::kContextMenuSaveVideoFrameAs,
    &media::kFullscreenVideoPictureInPicture,
    &net::features::kVerifyQWACs,
    &network::features::kLocalNetworkAccessChecks,
    &notifications::features::kUseChimeAndroidSdk,
    &ntp_features::kNtpCustomizeWebUiAndroid,
    &page_info::kPageInfoAboutThisSiteMoreLangs,
    &paint_preview::kPaintPreviewDemo,
    &password_manager::features::kActorLoginPermissionsUi,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::kPasswordFormGroupedAffiliations,
    &payments::facilitated::kEnablePixAccountLinkingNative,
    &payments::facilitated::kFacilitatedPaymentsEnableA2APayment,
    &permissions::features::kAndroidWindowManagementWebApi,
    &permissions::features::kPermissionDedicatedCpssSettingAndroid,
    &permissions::features::kPermissionsPromptSurvey,
    &privacy_sandbox::kPrivacySandboxAdPrivacyUxDeprecation,
    &privacy_sandbox::kRelatedWebsiteSetsUi,
    &safe_browsing::kAutoRevokeSuspiciousNotification,
    &safe_browsing::kExtendedReportingRemovePrefDependency,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &safe_browsing::kMaliciousApkDownloadCheck,
    &safe_browsing::kReportNotificationContentDetectionData,
    &safe_browsing::kShowWarningsForSuspiciousNotifications,
    &safety_check::features::kSafetyHub,
    &security_interstitials::features::kHttpsFirstDialogUi,
    &segmentation_platform::features::kAndroidAppIntegrationModule,
    &segmentation_platform::features::kAndroidTipsNotifications,
    &segmentation_platform::features::kAndroidTipsNotificationsV2,
    &segmentation_platform::features::kContextualPageActionTabGrouping,
    &segmentation_platform::features::kContextualPageActions,
    &segmentation_platform::features::kNewTabPageCustomizationV2,
    &segmentation_platform::features::kSegmentationPlatformAndroidHomeModuleRanker,
    &segmentation_platform::features::kSegmentationPlatformAndroidHomeModuleRankerV2,
    &segmentation_platform::features::kSegmentationPlatformEphemeralCardRanker,
    &send_tab_to_self::kSendTabToSelfAutoOpen,
    &send_tab_to_self::kSendTabToSelfDynamicShortcuts,
    &send_tab_to_self::kSendTabToSelfEnhancedBottomsheet,
    &send_tab_to_self::kSendTabToSelfExtraEntryPoints,
    &send_tab_to_self::kSendTabToSelfGesture,
    &send_tab_to_self::kSendTabToSelfOpenNativeApp,
    &send_tab_to_self::kSendTabToSelfPostSendToast,
    &send_tab_to_self::kSendTabToSelfPropagateFormFields,
    &send_tab_to_self::kSendTabToSelfPropagateScrollPosition,
    &send_tab_to_self::kSendTabToSelfRecordSnackbarActivation,
    &send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid,
    &sensitive_content::features::kSensitiveContent,
    &sensitive_content::features::kSensitiveContentWhileSwitchingTabs,
    &site_isolation::features::kSiteIsolationEnableMemoryThresholdAndroid,
    &switches::kApplyDeviceChoiceRenewal,
    &switches::kClankDefaultSearchApi,
    &switches::kSearchSettingsUpdateV2,
    &sync_preferences::features::kCrossDevicePrefTrackerExtraLogs,
    &sync_sessions::kOptimizeAssociateWindowsAndroid,
    &syncer::kNewTabPageCustomizationThemeSync,
    &syncer::kSyncEnableNewSyncDashboardUrl,
    &syncer::kSyncEnablePasswordsSyncErrorMessageAlternative,
    &syncer::kSyncTrustedVaultErrorMessageDuration,
    &syncer::kWebApkBackupAndRestoreBackend,
    &tab_groups::kUpdateTabGroupColors,
    &tab_groups::kUseAlternateHistorySyncIllustration,
    &themes::kCrossDeviceThemeTracker,
    &universal_optout::features::kUniversalOptOutSettings,
    &visited_url_ranking::features::kGroupSuggestionService,
    &webapps::features::kAndroidAutoMintedTWA,
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
BASE_FEATURE(kAccountPickerDialog, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kActorLiveNotification, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kActorNotificationIntentRouting, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kActorStepProgressNotification, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAllocInstanceIdIncreasedDefaultRange, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAllowMultipleMediaNotifications, base::FEATURE_DISABLED_BY_DEFAULT);
// Don't clean up this flag yet, BCIV is launched, so this needs to be enabled by
// default, but some render tests need to disable this so that the hairline isn't
// included in the screenshot. See crbug.com/394842006 for more details.
BASE_FEATURE(kAlwaysDrawCompositedToolbarHairline, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidActorTaskTimeout, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAppIntegrationMultiDataSource, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAppRatingPrompt, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAtomsLogging, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidAutofillPrefObserver, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBottomBar, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBottomBarAim, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidBricksNativePage, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidContextMenuDisabledMenuItems, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidDesktopBookmarkLayout, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidDesktopBookmarkPopup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidDesktopHistoryLayout, base::FEATURE_DISABLED_BY_DEFAULT);
// Enables additional text shown during profile creation for managed users,
// informing them about device signal collection for security purposes.
BASE_FEATURE(kAndroidDeviceSignalsDisclaimer, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidElegantTextHeight, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidFirstRunLaunchBounds, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidFreLayoutUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidHistoryClustering, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidKeyboardShortcutOpenFile, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidNoCaptureWhenScrollingDisabledOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidNoVisibleHintForDifferentTLD, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidOmniboxFocusedNewTabPage, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidOpenIncognitoAsWindowRestrictions, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidPageInfoAsAppMenuItem, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidProgressBarVisualUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSaveCardNonBlockingDialog, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSettingsContainment, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSettingsUrl, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSetupList, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidStartupImprovements, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSurfaceColorUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabDeclutterArchiveOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabDeclutterDedupeTabIdsKillSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabSkipSaveTabsKillswitch,"AndroidTabSkipSaveTabsTaskKillswitch", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidTabstripStartupCaptureBugFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidThemeModule, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidThemeResourceProvider, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidToolbarScrollAblation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidXRUsesSurfaceControl, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidXrImmersivePlayer, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidZoomImmersive, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAnimatedGifRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAnimatedImageDragShadow, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAnnotatedPageContentsVirtualStructure, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch1, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch3, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch4, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch6, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch7, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch8, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kApb144Patch9, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAppSpecificHistory, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAppSpecificHistoryViewIntent, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kArchivedTabsTeardown, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAsyncNotificationManager, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAsyncNotificationManagerForDownload, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutomotiveBackButtonBarStreamline, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAuxiliarySearchDonation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAuxiliarySearchHistoryDonation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAvoidDoubleMultiwindowChanges, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBackGestureReflectsDesktopBehavior, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBlockIntentsWhileLocked, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBookmarkPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBookmarksBarContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBookmarksBarNTP, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBottomSheetAsBrowserControls, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBottomSheetOnDesktopWindowing, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsDebugging, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsEarlyResize, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsHidingToken, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsPersistsOnCvh, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserControlsRenderDrivenShowConstraint, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAdaptiveButton, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTAdaptiveButtonTestSwitch, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTBlockTouchesDuringEnterAnimation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTClientDataHeader, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTContextualMenuItems, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTDestroyTabWhenModelIsEmpty, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTDontOverrideIntentMimeType, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTExtendTrustedCdnPublisher, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTFreInSameTask, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTGoogleBottomBar, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTGoogleBottomBarVariantLayouts, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTIncognitoAvailableToThirdParty, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTMinimizedEnabledByDefault, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNavigationInfoScreenshot, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNavigationMetrics, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTNavigationalPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTOpenInBrowserButtonIfAllowedByEmbedder, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTOpenInBrowserButtonIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTPageContentRequestAllowed, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTPageContentRequestEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTRealtimeEngagementEventsInBackground, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTReportParallelRequestStatus, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTReportPrerenderEvents, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTResetTimeoutAllowed, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTResizableForThirdParties, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTTabModalDialog, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCCTTabSwitcherEnabledForChromeExperiment, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCCTTabSwitcherEnabledForEmbedderExperiment, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheDeprecatedSystemLocationSetting, base::FEATURE_ENABLED_BY_DEFAULT);
// Used in downstream code.
BASE_FEATURE(kCacheIsGoogleSigned, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheIsMultiInstanceApi31Enabled, base::FEATURE_ENABLED_BY_DEFAULT);
// Used in downstream code.
BASE_FEATURE(kCastDeviceFilter, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCctTabResumption, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChangeUnfocusedPriority, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeNativeUrlOverriding, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeSurveyNextAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kClampAutomotiveScaling, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kClankGlicContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kClankStartupLatencyInjection, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kClankWhatsNew, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kClearIntentWhenRecreated, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCommandLineOnNonRooted, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCompositorViewRemeasureFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kContextualPanelCloseButton, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kContextualSearchDisableOnlineDetection, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualSearchSuppressShortView, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kControlsInBrowserToolbarSwipeMock, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kControlsVisibilityFromNavigations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCopyLinkToHighlight, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCrossDeviceTabPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCrossDeviceTaskHandoff, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCrossWindowTabGroupOperations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDebugToolbarPositioning, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoAndroid2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoEntryPoint, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserPromoFre, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDeferNavigationStateChanged, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidBackgroundTabLoading, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidLinkCapturing, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidTWADeleteBrowserData, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidTWADisclosures, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopAndroidTWADisclosuresHelpLink, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopUAOnConnectedDisplay, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDisableGridTabSwitcher, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDisablePartnerHomepageAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDisableScrollbarOfFadingEdgeScrollView, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDiscardPageWithCrashedSubframePolicy, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDontAutoHideBrowserControls, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeAutomotive, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeBottomChin, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeEverywhere, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeExtraLogs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeMonitorConfigurations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeTablet, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgeToEdgeUseBackupNavbarInsets, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEdgelessTopInset, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAndroidSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAndroidSidePanelDevFeature, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAndroidSidePanelLogs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableBrowserWindowInterfaceForCustomTabActivity, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableEscapeHandlingForSecondaryActivities, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableSwipeToSwitchPane, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableToolbarPositioningInResizeMode, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableXAxisActivityTransition, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnforceIncognitoIsolation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExperimentsForAgsa, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFaviconDisableHostFallback, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFlyoutInBookmarksBar, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kForceTranslucentNotificationTrampoline, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFullscreenInsetsApiMigration, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGestureUserEducationBackSwipe, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGmsCoreBindServiceOptimization, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGridTabSwitcherSurfaceColorUpdate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHistoryPaneAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHomeButtonRemoval, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInAppUpdateFlow, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInAppWindowManagerDeprecation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoAsWindowFullScreen, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoModeForcedAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoScreenshot, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIncognitoThemeOverlayTesting, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInlinePdfV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInlinePdfV2Download, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInlinePdfV2Incognito, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kKeyboardEscBackNavigation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLaunchCauseScreenOffFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLensOnQuickActionSearchWidget, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLinkHoverStatusBar, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLoadAllTabsAtStartup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLockTopControlsOnLargeTabletsV2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLongScreenshotsLenientMemoryCheck, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLongScreenshotsNoMemoryCheck, base::FEATURE_DISABLED_BY_DEFAULT);
// Enables an experimental feature which forces mayLaunchUrl to use a different
// storage partition. This may reduce performance. This should not be enabled by
// default.
BASE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMostVisitedTilesCustomization, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kMostVisitedTilesReselect, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMoveToFrontInLaunchIntentDispatcher, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kMultiInstanceSharedPrefsMigration, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNavigationListMenu, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationPermissionVariant, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationTrampoline, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNotificationTrampolineNoNewTask, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNtpAurora, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNtpAuroraV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNtpMvcRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNtpVision, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOmahaMinSdkVersionAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOnDemandBackgroundTabContextCapture, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOnDemandBackgroundTabContextCaptureOptimization, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOnStartupWindowPolicy, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOneStepAimAccess, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOpenDownloadInPreferredApp, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPCCTMinimumHeight, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageAnnotationsService, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPageContentProvider, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPartnerCustomizationsUma, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPdfLauncherActivity, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPdfReuseFragment, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPersistAcrossReboots, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPersistAcrossRebootsDebugLogs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPowerSavingModeBroadcastReceiverInBackground, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPreconnectOnTabCreation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPriceChangeModule, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPrintSelectionMenu, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kProtectRecentlyVisibleTab, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPwaRestoreUi, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPwaRestoreUiAtStartup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kQueuedCompositorWebContentsUpdates, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudAudioOverviews, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReadAloudServerExperiments, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRecordSuppressionMetrics, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kReengagementNotification, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kRelatedSearchesAllLanguage, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRelatedSearchesSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRemoveTabFocusOnShowingAndSelect, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRobustWindowManagementExperimental, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSafetyFrePromo, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kScheduleWindowCleaning, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTAlternateTapHandling, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSearchInCCTIfEnabledByEmbedder, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSessionRestoreAfterCrash, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsInTab, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsInTabUrlNav, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsMultiColumn, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSettingsSingleActivity, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kShareCustomActionsInCCT, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShortCircuitUnfocusAnimation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShowTabListAnimations, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSidePanelTopHairlineRefactorAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSmallerTabStripTitleLimit, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kStartSurfaceReturnTime, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSubmenusInAppMenu, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSubmenusInAppMenuLff, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSyncRestoreOnStartupPref, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabAndroidGracefulShutdown, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabBottomSheet, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabBottomSheetFullHeight, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabBottomSheetHalfHeight, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabBottomSheetResizeWebview, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabClosureCommittedMethodRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabClosureMethodRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabOpenerTracking, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabSearchForDesktop, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSharingToolbarAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabStorageSqlitePrototype, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripAutoSelectOnCloseChange, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripHeightTransitionGlitchFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripLayoutTransitionDebounceFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabStripStopSpinnerOnLoadStop, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherDragDropAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherGroupSuggestionsAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherGroupSuggestionsTestModeAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabSwitcherMessagesOnDesktopWindowingKillSwitch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabWindowManagerReportIndicesMismatch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestDefaultDisabled, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestDefaultEnabled, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTextHighlightFullLink, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kThreeDotMenuBackButton, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTipsSelfService, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarCaptureFixForSPAs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarPhoneAnimationRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarProgressBarRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarSnapshotRefactor, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kToolbarTabletResizeRefactor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTouchToSearchCallout, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrustedWebActivityContactsDelegation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTweakApplicationPreloadLoadNativeFirst, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTweakApplicationPreloadMoveWarmUp, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTweakApplicationPreloadSkipNewInstance, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTweakApplicationPreloadSkipWarmUp, base::FEATURE_DISABLED_BY_DEFAULT);
// If enabled, keep logging and reporting UMA while chrome is backgrounded.
BASE_FEATURE(kUmaBackgroundSessions,"UMABackgroundSessions", base::FEATURE_ENABLED_BY_DEFAULT);
// Correctness fixes to Activity tagging for UMA sessions.
BASE_FEATURE(kUmaSessionCorrectnessFixes, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUniversalKeyboardHandling, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUnparcelIntentFileDescriptors, base::FEATURE_ENABLED_BY_DEFAULT);
// Activate tab with moveTaskToFront() which works in multi-window mode.
BASE_FEATURE(kUseActivityManagerForTabActivation, base::FEATURE_ENABLED_BY_DEFAULT);
// Use AppTask.startActivity to bypass Background Activity Launch (BAL) restrictions.
BASE_FEATURE(kUseAppTaskForCustomTabActivation, base::FEATURE_ENABLED_BY_DEFAULT);
// Use the LibunwindstackNativeUnwinderAndroid for only browser main thread, and
// only on Android.
BASE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUseWebUiNtpAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
// Verify sign-in state on startup.
BASE_FEATURE(kVerifyStartupSigninState, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kVirtualKeyboardResizesContentTransientOvershootFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kVirtualKeyboardTransientInnerHeightFix, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWebAppNavigationBarThemeColor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWebAppShortEdgesCutoutMode, base::FEATURE_DISABLED_BY_DEFAULT);
// Shows only the remote device name on the Android notification instead of
// a descriptive text.
BASE_FEATURE(kWebOtpCrossDeviceSimpleString, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWebUiAndroidTheming, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kXplatSyncedSetupThemes, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kYourSavedInfoSettingsPageAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
// go/keep-sorted end
// BASE_FEATURE_END

// clang-format on

}  // namespace android
}  // namespace chrome

DEFINE_JNI(ChromeFeatureMap)
