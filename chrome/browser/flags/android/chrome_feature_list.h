// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome::android {

// Clang formatting is turned off so that long names don't extend to two lines,
// which makes it easier to have scripts that automatically add new flags
// correctly.

// clang-format off

// Alphabetical:
// BASE_DECLARE_FEATURE_START
// go/keep-sorted start
BASE_DECLARE_FEATURE(kAccountForSuppressedKeyboardInsets);
BASE_DECLARE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2);
BASE_DECLARE_FEATURE(kAdaptiveButtonInTopToolbarPageSummary);
BASE_DECLARE_FEATURE(kAndroidAnimatedProgressBarInViz);
BASE_DECLARE_FEATURE(kAndroidAppIntegrationMultiDataSource);
BASE_DECLARE_FEATURE(kAndroidAppearanceSettings);
BASE_DECLARE_FEATURE(kAndroidBookmarkBar);
BASE_DECLARE_FEATURE(kAndroidBookmarkBarFastFollow);
BASE_DECLARE_FEATURE(kAndroidBottomToolbar);
BASE_DECLARE_FEATURE(kAndroidBottomToolbarV2);
BASE_DECLARE_FEATURE(kAndroidComposeplate);
BASE_DECLARE_FEATURE(kAndroidComposeplateAllLocales);
BASE_DECLARE_FEATURE(kAndroidComposeplateLFF);
BASE_DECLARE_FEATURE(kAndroidComposeplateLFFAllLocales);
BASE_DECLARE_FEATURE(kAndroidContextMenuDuplicateTabs);
BASE_DECLARE_FEATURE(kAndroidDataImporterService);
BASE_DECLARE_FEATURE(kAndroidDesktopDensity);
BASE_DECLARE_FEATURE(kAndroidElegantTextHeight);
BASE_DECLARE_FEATURE(kAndroidFirstRunLaunchBounds);
BASE_DECLARE_FEATURE(kAndroidLogoViewRefactor);
BASE_DECLARE_FEATURE(kAndroidNewMediaPicker);
BASE_DECLARE_FEATURE(kAndroidNoVisibleHintForDifferentTLD);
BASE_DECLARE_FEATURE(kAndroidOmniboxFocusedNewTabPage);
BASE_DECLARE_FEATURE(kAndroidOpenPdfInlineBackport);
BASE_DECLARE_FEATURE(kAndroidPbDisablePulseAnimation);
BASE_DECLARE_FEATURE(kAndroidPbDisableSmoothAnimation);
BASE_DECLARE_FEATURE(kAndroidPdfAssistContent);
BASE_DECLARE_FEATURE(kAndroidPinnedTabs);
BASE_DECLARE_FEATURE(kAndroidPinnedTabsTabletTabStrip);
BASE_DECLARE_FEATURE(kAndroidProgressBarVisualUpdate);
BASE_DECLARE_FEATURE(kAndroidSearchInSettings);
BASE_DECLARE_FEATURE(kAndroidSettingsContainment);
BASE_DECLARE_FEATURE(kAndroidShowRestoreTabsPromoOnFREBypassedKillSwitch);
BASE_DECLARE_FEATURE(kAndroidSurfaceColorUpdate);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterArchiveAllButActiveTab);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterArchiveTabGroups);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterDedupeTabIdsKillSwitch);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterPerformanceImprovements);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterRescueKillswitch);
BASE_DECLARE_FEATURE(kAndroidTabGroupsColorUpdateGM3);
BASE_DECLARE_FEATURE(kAndroidTabHighlighting);
BASE_DECLARE_FEATURE(kAndroidTabSkipSaveTabsKillswitch);
BASE_DECLARE_FEATURE(kAndroidThemeModule);
BASE_DECLARE_FEATURE(kAndroidThemeResourceProvider);
BASE_DECLARE_FEATURE(kAndroidToolbarScrollAblation);
BASE_DECLARE_FEATURE(kAndroidUseAdminsForEnterpriseInfo);
BASE_DECLARE_FEATURE(kAndroidWindowPopupCustomTabUi);
BASE_DECLARE_FEATURE(kAndroidWindowPopupLargeScreen);
BASE_DECLARE_FEATURE(kAndroidWindowPopupPredictFinalBounds);
BASE_DECLARE_FEATURE(kAndroidWindowPopupResizeAfterSpawn);
BASE_DECLARE_FEATURE(kAndroidXRUsesSurfaceControl);
BASE_DECLARE_FEATURE(kAnimatedGifRefactor);
BASE_DECLARE_FEATURE(kAnimatedImageDragShadow);
BASE_DECLARE_FEATURE(kAnnotatedPageContentsVirtualStructure);
BASE_DECLARE_FEATURE(kAppSpecificHistory);
BASE_DECLARE_FEATURE(kAppSpecificHistoryViewIntent);
BASE_DECLARE_FEATURE(kAsyncNotificationManager);
BASE_DECLARE_FEATURE(kAsyncNotificationManagerForDownload);
BASE_DECLARE_FEATURE(kAutomotiveBackButtonBarStreamline);
BASE_DECLARE_FEATURE(kAuxiliarySearchDonation);
BASE_DECLARE_FEATURE(kAuxiliarySearchHistoryDonation);
BASE_DECLARE_FEATURE(kAvoidRelayoutDuringFocusAnimation);
BASE_DECLARE_FEATURE(kBackgroundThreadPool);
BASE_DECLARE_FEATURE(kBlockIntentsWhileLocked);
BASE_DECLARE_FEATURE(kBookmarkPaneAndroid);
BASE_DECLARE_FEATURE(kBrowserControlsDebugging);
BASE_DECLARE_FEATURE(kBrowserControlsEarlyResize);
BASE_DECLARE_FEATURE(kBrowserControlsRenderDrivenShowConstraint);
BASE_DECLARE_FEATURE(kCCTAdaptiveButton);
BASE_DECLARE_FEATURE(kCCTAdaptiveButtonTestSwitch);
BASE_DECLARE_FEATURE(kCCTAuthTab);
BASE_DECLARE_FEATURE(kCCTAuthTabDisableAllExternalIntents);
BASE_DECLARE_FEATURE(kCCTAuthTabEnableHttpsRedirects);
BASE_DECLARE_FEATURE(kCCTBlockTouchesDuringEnterAnimation);
BASE_DECLARE_FEATURE(kCCTClientDataHeader);
BASE_DECLARE_FEATURE(kCCTContextualMenuItems);
BASE_DECLARE_FEATURE(kCCTDestroyTabWhenModelIsEmpty);
BASE_DECLARE_FEATURE(kCCTExtendTrustedCdnPublisher);
BASE_DECLARE_FEATURE(kCCTFixWarmup);
BASE_DECLARE_FEATURE(kCCTFreInSameTask);
BASE_DECLARE_FEATURE(kCCTGoogleBottomBar);
BASE_DECLARE_FEATURE(kCCTGoogleBottomBarVariantLayouts);
BASE_DECLARE_FEATURE(kCCTIncognitoAvailableToThirdParty);
BASE_DECLARE_FEATURE(kCCTMinimized);
BASE_DECLARE_FEATURE(kCCTMinimizedEnabledByDefault);
BASE_DECLARE_FEATURE(kCCTMultipleParallelRequests);
BASE_DECLARE_FEATURE(kCCTNavigationMetrics);
BASE_DECLARE_FEATURE(kCCTNavigationalPrefetch);
BASE_DECLARE_FEATURE(kCCTNestedSecurityIcon);
BASE_DECLARE_FEATURE(kCCTOpenInBrowserButtonIfAllowedByEmbedder);
BASE_DECLARE_FEATURE(kCCTOpenInBrowserButtonIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kCCTRealtimeEngagementEventsInBackground);
BASE_DECLARE_FEATURE(kCCTReportParallelRequestStatus);
BASE_DECLARE_FEATURE(kCCTReportPrerenderEvents);
BASE_DECLARE_FEATURE(kCCTResetTimeoutEnabled);
BASE_DECLARE_FEATURE(kCCTResizableForThirdParties);
BASE_DECLARE_FEATURE(kCCTRetainingStateInMemory);
BASE_DECLARE_FEATURE(kCCTShowTabFix);
BASE_DECLARE_FEATURE(kCCTTabModalDialog);
BASE_DECLARE_FEATURE(kCCTToolbarRefactor);
BASE_DECLARE_FEATURE(kCacheActivityTaskID);
BASE_DECLARE_FEATURE(kCacheDeprecatedSystemLocationSetting);
BASE_DECLARE_FEATURE(kCacheIsMultiInstanceApi31Enabled);
BASE_DECLARE_FEATURE(kCastDeviceFilter);
BASE_DECLARE_FEATURE(kChangeUnfocusedPriority);
BASE_DECLARE_FEATURE(kChromeItemPickerUi);
BASE_DECLARE_FEATURE(kChromeNativeUrlOverriding);
BASE_DECLARE_FEATURE(kChromeShareScreenshot);
BASE_DECLARE_FEATURE(kChromeSharingHubLaunchAdjacent);
BASE_DECLARE_FEATURE(kChromeSurveyNextAndroid);
BASE_DECLARE_FEATURE(kClampAutomotiveScaling);
BASE_DECLARE_FEATURE(kClankStartupLatencyInjection);
BASE_DECLARE_FEATURE(kClankWhatsNew);
BASE_DECLARE_FEATURE(kClearInstanceInfoWhenClosedIntentionally);
BASE_DECLARE_FEATURE(kClearIntentWhenRecreated);
BASE_DECLARE_FEATURE(kCommandLineOnNonRooted);
BASE_DECLARE_FEATURE(kContextMenuTranslateWithGoogleLens);
BASE_DECLARE_FEATURE(kContextualSearchDisableOnlineDetection);
BASE_DECLARE_FEATURE(kContextualSearchSuppressShortView);
BASE_DECLARE_FEATURE(kControlsVisibilityFromNavigations);
BASE_DECLARE_FEATURE(kCpaSpecUpdate);
BASE_DECLARE_FEATURE(kCrossDeviceTabPaneAndroid);
BASE_DECLARE_FEATURE(kDefaultBrowserPromoAndroid2);
BASE_DECLARE_FEATURE(kDesktopUAOnConnectedDisplay);
BASE_DECLARE_FEATURE(kDeviceAuthenticatorAndroidx);
BASE_DECLARE_FEATURE(kDisableInstanceLimit);
BASE_DECLARE_FEATURE(kDiscardPageWithCrashedSubframePolicy);
BASE_DECLARE_FEATURE(kDontAutoHideBrowserControls);
BASE_DECLARE_FEATURE(kDontPrefetchLibraries);
BASE_DECLARE_FEATURE(kDrawChromePagesEdgeToEdge);
BASE_DECLARE_FEATURE(kEdgeToEdgeBottomChin);
BASE_DECLARE_FEATURE(kEdgeToEdgeEverywhere);
BASE_DECLARE_FEATURE(kEdgeToEdgeMonitorConfigurations);
BASE_DECLARE_FEATURE(kEdgeToEdgeTablet);
BASE_DECLARE_FEATURE(kEdgeToEdgeUseBackupNavbarInsets);
BASE_DECLARE_FEATURE(kEducationalTipDefaultBrowserPromoCard);
BASE_DECLARE_FEATURE(kEmptyTabListAnimationKillSwitch);
BASE_DECLARE_FEATURE(kEnableEscapeHandlingForSecondaryActivities);
BASE_DECLARE_FEATURE(kEnableXAxisActivityTransition);
BASE_DECLARE_FEATURE(kExperimentsForAgsa);
BASE_DECLARE_FEATURE(kFloatingSnackbar);
BASE_DECLARE_FEATURE(kForceTranslucentNotificationTrampoline);
BASE_DECLARE_FEATURE(kFullscreenInsetsApiMigration);
BASE_DECLARE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive);
BASE_DECLARE_FEATURE(kGridTabSwitcherSurfaceColorUpdate);
BASE_DECLARE_FEATURE(kGridTabSwitcherUpdate);
BASE_DECLARE_FEATURE(kGroupNewTabWithParent);
BASE_DECLARE_FEATURE(kHeadlessTabModel);
BASE_DECLARE_FEATURE(kHistoryPaneAndroid);
BASE_DECLARE_FEATURE(kHomeModulePrefRefactor);
BASE_DECLARE_FEATURE(kHomepageIsNewTabPagePolicyAndroid);
BASE_DECLARE_FEATURE(kHubBackButton);
BASE_DECLARE_FEATURE(kHubSlideAnimation);
BASE_DECLARE_FEATURE(kImprovedA2HS);
BASE_DECLARE_FEATURE(kIncognitoNtpSmallIcon);
BASE_DECLARE_FEATURE(kIncognitoScreenshot);
BASE_DECLARE_FEATURE(kIncognitoThemeOverlayTesting);
BASE_DECLARE_FEATURE(kInstanceSwitcherV2);
BASE_DECLARE_FEATURE(kKeyboardEscBackNavigation);
BASE_DECLARE_FEATURE(kLanguagesPreference);
BASE_DECLARE_FEATURE(kLensOnQuickActionSearchWidget);
BASE_DECLARE_FEATURE(kLinkHoverStatusBar);
BASE_DECLARE_FEATURE(kLoadAllTabsAtStartup);
BASE_DECLARE_FEATURE(kLoadNativeEarly);
BASE_DECLARE_FEATURE(kLocationBarModelOptimizations);
BASE_DECLARE_FEATURE(kLockBackPressHandlerAtStart);
BASE_DECLARE_FEATURE(kLockTopControlsOnLargeTablets);
BASE_DECLARE_FEATURE(kLockTopControlsOnLargeTabletsV2);
BASE_DECLARE_FEATURE(kMagicStackAndroid);
BASE_DECLARE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition);
BASE_DECLARE_FEATURE(kMediaIndicatorsAndroid);
BASE_DECLARE_FEATURE(kMiniOriginBar);
BASE_DECLARE_FEATURE(kMostVisitedTilesCustomization);
BASE_DECLARE_FEATURE(kMostVisitedTilesReselect);
BASE_DECLARE_FEATURE(kMultiInstanceApplicationStatusCleanup);
BASE_DECLARE_FEATURE(kMvcUpdateViewWhenModelChanged);
BASE_DECLARE_FEATURE(kNavBarColorAnimation);
BASE_DECLARE_FEATURE(kNewTabPageCustomization);
BASE_DECLARE_FEATURE(kNewTabPageCustomizationForMvt);
BASE_DECLARE_FEATURE(kNewTabPageCustomizationToolbarButton);
BASE_DECLARE_FEATURE(kNewTabPageCustomizationV2);
BASE_DECLARE_FEATURE(kNotificationPermissionBottomSheet);
BASE_DECLARE_FEATURE(kNotificationPermissionVariant);
BASE_DECLARE_FEATURE(kNotificationTrampoline);
BASE_DECLARE_FEATURE(kOmahaMinSdkVersionAndroid);
BASE_DECLARE_FEATURE(kOptimizeGeolocationHeaderGeneration);
BASE_DECLARE_FEATURE(kPCCTMinimumHeight);
BASE_DECLARE_FEATURE(kPageAnnotationsService);
BASE_DECLARE_FEATURE(kPageContentProvider);
BASE_DECLARE_FEATURE(kPartnerCustomizationsUma);
BASE_DECLARE_FEATURE(kPowerSavingModeBroadcastReceiverInBackground);
BASE_DECLARE_FEATURE(kPreconnectOnTabCreation);
BASE_DECLARE_FEATURE(kPriceChangeModule);
BASE_DECLARE_FEATURE(kProcessRankPolicyAndroid);
BASE_DECLARE_FEATURE(kProtectedTabsAndroid);
BASE_DECLARE_FEATURE(kPwaRestoreUi);
BASE_DECLARE_FEATURE(kPwaRestoreUiAtStartup);
BASE_DECLARE_FEATURE(kReadAloud);
BASE_DECLARE_FEATURE(kReadAloudAudioOverviews);
BASE_DECLARE_FEATURE(kReadAloudAudioOverviewsFeedback);
BASE_DECLARE_FEATURE(kReadAloudAudioOverviewsSkipDisclaimerWhenPossible);
BASE_DECLARE_FEATURE(kReadAloudBackgroundPlayback);
BASE_DECLARE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT);
BASE_DECLARE_FEATURE(kReadAloudInMultiWindow);
BASE_DECLARE_FEATURE(kReadAloudInOverflowMenuInCCT);
BASE_DECLARE_FEATURE(kReadAloudPlayback);
BASE_DECLARE_FEATURE(kReadAloudServerExperiments);
BASE_DECLARE_FEATURE(kReadAloudTapToSeek);
BASE_DECLARE_FEATURE(kRecentlyClosedTabsAndWindows);
BASE_DECLARE_FEATURE(kRecordSuppressionMetrics);
BASE_DECLARE_FEATURE(kReengagementNotification);
BASE_DECLARE_FEATURE(kRelatedSearchesAllLanguage);
BASE_DECLARE_FEATURE(kRelatedSearchesSwitch);
BASE_DECLARE_FEATURE(kReloadTabUiResourcesIfChanged);
BASE_DECLARE_FEATURE(kRemoveTabFocusOnShowingAndSelect);
BASE_DECLARE_FEATURE(kRightEdgeGoesForwardGestureNav);
BASE_DECLARE_FEATURE(kRobustWindowManagement);
BASE_DECLARE_FEATURE(kRobustWindowManagementExperimental);
BASE_DECLARE_FEATURE(kSearchInCCT);
BASE_DECLARE_FEATURE(kSearchInCCTAlternateTapHandling);
BASE_DECLARE_FEATURE(kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kSearchInCCTIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kSearchResumptionModuleAndroid);
BASE_DECLARE_FEATURE(kSettingsMultiColumn);
BASE_DECLARE_FEATURE(kSettingsSingleActivity);
BASE_DECLARE_FEATURE(kShareCustomActionsInCCT);
BASE_DECLARE_FEATURE(kSharingHubLinkToggle);
BASE_DECLARE_FEATURE(kShortCircuitUnfocusAnimation);
BASE_DECLARE_FEATURE(kShowCloseAllIncognitoTabsButton);
BASE_DECLARE_FEATURE(kShowHomeButtonPolicyAndroid);
BASE_DECLARE_FEATURE(kShowNewTabAnimations);
BASE_DECLARE_FEATURE(kShowTabListAnimations);
BASE_DECLARE_FEATURE(kSmallerTabStripTitleLimit);
BASE_DECLARE_FEATURE(kStartSurfaceReturnTime);
BASE_DECLARE_FEATURE(kSubmenusInAppMenu);
BASE_DECLARE_FEATURE(kSubmenusTabContextMenuLffTabStrip);
BASE_DECLARE_FEATURE(kSuppressToolbarCapturesAtGestureEnd);
BASE_DECLARE_FEATURE(kTabArchivalDragDropAndroid);
BASE_DECLARE_FEATURE(kTabClosureMethodRefactor);
BASE_DECLARE_FEATURE(kTabCollectionAndroid);
BASE_DECLARE_FEATURE(kTabFreezingUsesDiscard);
BASE_DECLARE_FEATURE(kTabGroupAndroidVisualDataCleanup);
BASE_DECLARE_FEATURE(kTabGroupEntryPointsAndroid);
BASE_DECLARE_FEATURE(kTabGroupParityBottomSheetAndroid);
BASE_DECLARE_FEATURE(kTabModelInitFixes);
BASE_DECLARE_FEATURE(kTabStorageSqlitePrototype);
BASE_DECLARE_FEATURE(kTabStripAutoSelectOnCloseChange);
BASE_DECLARE_FEATURE(kTabStripDensityChangeAndroid);
BASE_DECLARE_FEATURE(kTabStripGroupDragDropAndroid);
BASE_DECLARE_FEATURE(kTabStripIncognitoMigration);
BASE_DECLARE_FEATURE(kTabStripMouseCloseResizeDelay);
BASE_DECLARE_FEATURE(kTabSwitcherDragDropAndroid);
BASE_DECLARE_FEATURE(kTabSwitcherGroupSuggestionsAndroid);
BASE_DECLARE_FEATURE(kTabSwitcherGroupSuggestionsTestModeAndroid);
BASE_DECLARE_FEATURE(kTabWindowManagerReportIndicesMismatch);
BASE_DECLARE_FEATURE(kTabletTabStripAnimation);
BASE_DECLARE_FEATURE(kTestDefaultDisabled);
BASE_DECLARE_FEATURE(kTestDefaultEnabled);
BASE_DECLARE_FEATURE(kThirdPartyDisableChromeAutofillSettingsScreen);
BASE_DECLARE_FEATURE(kTinkerTankBottomSheet);
BASE_DECLARE_FEATURE(kToolbarPhoneAnimationRefactor);
BASE_DECLARE_FEATURE(kToolbarSnapshotRefactor);
BASE_DECLARE_FEATURE(kToolbarStaleCaptureBugFix);
BASE_DECLARE_FEATURE(kToolbarTabletResizeRefactor);
BASE_DECLARE_FEATURE(kTopControlsRefactor);
BASE_DECLARE_FEATURE(kTopControlsRefactorV2);
BASE_DECLARE_FEATURE(kTouchToSearchCallout);
BASE_DECLARE_FEATURE(kTrustedWebActivityContactsDelegation);
BASE_DECLARE_FEATURE(kUmaBackgroundSessions);
BASE_DECLARE_FEATURE(kUmaSessionCorrectnessFixes);
BASE_DECLARE_FEATURE(kUpdateCompositorForSurfaceControl);
BASE_DECLARE_FEATURE(kUseActivityManagerForTabActivation);
BASE_DECLARE_FEATURE(kUseInitialNetworkStateAtStartup);
BASE_DECLARE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid);
BASE_DECLARE_FEATURE(kWebOtpCrossDeviceSimpleString);
BASE_DECLARE_FEATURE(kXplatSyncedSetup);
// go/keep-sorted end
// BASE_DECLARE_FEATURE_END

// clang-format on

// For FeatureParam, Alphabetical:
inline constexpr base::FeatureParam<int> kAppIntegrationMaxDonationCountParam(
    &kAndroidAppIntegrationMultiDataSource,
    "max_donation_count",
    100);

inline constexpr base::FeatureParam<int>
    kAppIntegrationCCTVisitDurationLimitSecParam(
        &kAndroidAppIntegrationMultiDataSource,
        "cct_visit_duration_limit_sec",
        3);

inline constexpr base::FeatureParam<int>
    kAuxiliarySearchHistoryDonationDelayInSeconds{
        &kAuxiliarySearchHistoryDonation,
        /*name=*/"auxiliary_search_history_donation_delay",
        /*default_value=*/base::Minutes(5).InSeconds()};

inline constexpr base::FeatureParam<int> kAuxiliarySearchMaxBookmarksCountParam(
    &kAuxiliarySearchDonation,
    "auxiliary_search_max_donation_bookmark",
    100);

inline constexpr base::FeatureParam<size_t> kAuxiliarySearchMaxTabsCountParam(
    &kAuxiliarySearchDonation,
    "auxiliary_search_max_donation_tab",
    100);

inline constexpr base::FeatureParam<bool> kCCTNavigationalPrefetchHoldback(
    &kCCTNavigationalPrefetch,
    "holdback",
    false);

// If it does not support PERCEPTIBLE importance (e.g. Android Q- does not
// support not-perceptible binding), protected tabs have MODERATE importance as
// fallback.
inline constexpr base::FeatureParam<bool> kFallbackToModerateParam(
    &kProtectedTabsAndroid,
    "fallback_to_moderate",
    /*default_value=*/false);

inline constexpr base::FeatureParam<int>
    kReadAloudAudioOverviewsSpeedAdditionPercentage(
        &kReadAloudAudioOverviews,
        "read_aloud_audio_overviews_speed_addition_percentage",
        /* default_value=*/20);

inline constexpr base::FeatureParam<bool>
    kShouldConsiderLanguageInOverviewReadability(
        &kReadAloudAudioOverviews,
        "read_aloud_audio_overviews_should_consider_language_in_overview_"
        "readability",
        /* default_value=*/false);

inline constexpr base::FeatureParam<int>
    kReadAloudAudioReadabilityDelayMsAfterPageLoad(
        &kReadAloud,
        "read_aloud_readability_delay_ms_after_page_load",
        /* default_value=*/1500);

inline constexpr base::FeatureParam<bool> kTouchToSearchCalloutIph(
    &kTouchToSearchCallout,
    "iph",
    /*default_value=*/false);

inline constexpr base::FeatureParam<bool>
    kTouchToSearchCalloutSnippetAsSubtitle(&kTouchToSearchCallout,
                                           "snippet_as_subtitle",
                                           /*default_value=*/false);

}  // namespace chrome::android

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
