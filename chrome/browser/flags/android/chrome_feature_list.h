// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include <string>

#include "base/feature.h"
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
BASE_DECLARE_FEATURE(kAccountPickerDialog);
BASE_DECLARE_FEATURE(kActorLiveNotification);
BASE_DECLARE_FEATURE(kActorNotificationIntentRouting);
BASE_DECLARE_FEATURE(kActorStepProgressNotification);
BASE_DECLARE_FEATURE(kAllocInstanceIdIncreasedDefaultRange);
BASE_DECLARE_FEATURE(kAllowMultipleMediaNotifications);
BASE_DECLARE_FEATURE(kAlwaysDrawCompositedToolbarHairline);
BASE_DECLARE_FEATURE(kAndroidActorTaskTimeout);
BASE_DECLARE_FEATURE(kAndroidAnimatedProgressBarInViz);
BASE_DECLARE_FEATURE(kAndroidAppIntegrationMultiDataSource);
BASE_DECLARE_FEATURE(kAndroidAppRatingPrompt);
BASE_DECLARE_FEATURE(kAndroidAtomsLogging);
BASE_DECLARE_FEATURE(kAndroidAutofillPrefObserver);
BASE_DECLARE_FEATURE(kAndroidBottomBar);
BASE_DECLARE_FEATURE(kAndroidBottomBarAim);
BASE_DECLARE_FEATURE(kAndroidBricksNativePage);
BASE_DECLARE_FEATURE(kAndroidContextMenuDisabledMenuItems);
BASE_DECLARE_FEATURE(kAndroidDesktopBookmarkLayout);
BASE_DECLARE_FEATURE(kAndroidDesktopBookmarkPopup);
BASE_DECLARE_FEATURE(kAndroidDesktopHistoryLayout);
BASE_DECLARE_FEATURE(kAndroidDeviceSignalsDisclaimer);
BASE_DECLARE_FEATURE(kAndroidElegantTextHeight);
BASE_DECLARE_FEATURE(kAndroidFirstRunLaunchBounds);
BASE_DECLARE_FEATURE(kAndroidFreLayoutUpdate);
BASE_DECLARE_FEATURE(kAndroidHistoryClustering);
BASE_DECLARE_FEATURE(kAndroidNoCaptureWhenScrollingDisabledOnDesktop);
BASE_DECLARE_FEATURE(kAndroidNoVisibleHintForDifferentTLD);
BASE_DECLARE_FEATURE(kAndroidOmniboxFocusedNewTabPage);
BASE_DECLARE_FEATURE(kAndroidOpenIncognitoAsWindowRestrictions);
BASE_DECLARE_FEATURE(kAndroidPageInfoAsAppMenuItem);
BASE_DECLARE_FEATURE(kAndroidProgressBarVisualUpdate);
BASE_DECLARE_FEATURE(kAndroidSaveCardNonBlockingDialog);
BASE_DECLARE_FEATURE(kAndroidSettingsContainment);
BASE_DECLARE_FEATURE(kAndroidSettingsUrl);
BASE_DECLARE_FEATURE(kAndroidSetupList);
BASE_DECLARE_FEATURE(kAndroidStartupImprovements);
BASE_DECLARE_FEATURE(kAndroidSurfaceColorUpdate);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterArchiveOnDesktop);
BASE_DECLARE_FEATURE(kAndroidTabDeclutterDedupeTabIdsKillSwitch);
BASE_DECLARE_FEATURE(kAndroidTabSkipSaveTabsKillswitch);
BASE_DECLARE_FEATURE(kAndroidTabstripStartupCaptureBugFix);
BASE_DECLARE_FEATURE(kAndroidThemeModule);
BASE_DECLARE_FEATURE(kAndroidThemeResourceProvider);
BASE_DECLARE_FEATURE(kAndroidToolbarScrollAblation);
BASE_DECLARE_FEATURE(kAndroidVerticalTabs);
BASE_DECLARE_FEATURE(kAndroidXRUsesSurfaceControl);
BASE_DECLARE_FEATURE(kAndroidXrImmersivePlayer);
BASE_DECLARE_FEATURE(kAndroidZoomImmersive);
BASE_DECLARE_FEATURE(kAnimatedGifRefactor);
BASE_DECLARE_FEATURE(kAnimatedImageDragShadow);
BASE_DECLARE_FEATURE(kAnnotatedPageContentsVirtualStructure);
BASE_DECLARE_FEATURE(kApb144Patch1);
BASE_DECLARE_FEATURE(kApb144Patch2);
BASE_DECLARE_FEATURE(kApb144Patch3);
BASE_DECLARE_FEATURE(kApb144Patch4);
BASE_DECLARE_FEATURE(kApb144Patch6);
BASE_DECLARE_FEATURE(kApb144Patch7);
BASE_DECLARE_FEATURE(kApb144Patch8);
BASE_DECLARE_FEATURE(kApb144Patch9);
BASE_DECLARE_FEATURE(kAppSpecificHistory);
BASE_DECLARE_FEATURE(kAppSpecificHistoryViewIntent);
BASE_DECLARE_FEATURE(kArchivedTabsTeardown);
BASE_DECLARE_FEATURE(kAsyncNotificationManager);
BASE_DECLARE_FEATURE(kAsyncNotificationManagerForDownload);
BASE_DECLARE_FEATURE(kAutomotiveBackButtonBarStreamline);
BASE_DECLARE_FEATURE(kAuxiliarySearchDonation);
BASE_DECLARE_FEATURE(kAuxiliarySearchHistoryDonation);
BASE_DECLARE_FEATURE(kAvoidDoubleMultiwindowChanges);
BASE_DECLARE_FEATURE(kBackGestureReflectsDesktopBehavior);
BASE_DECLARE_FEATURE(kBackgroundThreadPool);
BASE_DECLARE_FEATURE(kBlockIntentsWhileLocked);
BASE_DECLARE_FEATURE(kBookmarkPaneAndroid);
BASE_DECLARE_FEATURE(kBookmarksBarContextMenu);
BASE_DECLARE_FEATURE(kBookmarksBarNTP);
BASE_DECLARE_FEATURE(kBottomSheetAsBrowserControls);
BASE_DECLARE_FEATURE(kBottomSheetOnDesktopWindowing);
BASE_DECLARE_FEATURE(kBrowserControlsDebugging);
BASE_DECLARE_FEATURE(kBrowserControlsEarlyResize);
BASE_DECLARE_FEATURE(kBrowserControlsHidingToken);
BASE_DECLARE_FEATURE(kBrowserControlsPersistsOnCvh);
BASE_DECLARE_FEATURE(kBrowserControlsRenderDrivenShowConstraint);
BASE_DECLARE_FEATURE(kCCTAdaptiveButton);
BASE_DECLARE_FEATURE(kCCTAdaptiveButtonTestSwitch);
BASE_DECLARE_FEATURE(kCCTBlockTouchesDuringEnterAnimation);
BASE_DECLARE_FEATURE(kCCTClientDataHeader);
BASE_DECLARE_FEATURE(kCCTContextualMenuItems);
BASE_DECLARE_FEATURE(kCCTDestroyTabWhenModelIsEmpty);
BASE_DECLARE_FEATURE(kCCTDontOverrideIntentMimeType);
BASE_DECLARE_FEATURE(kCCTExtendTrustedCdnPublisher);
BASE_DECLARE_FEATURE(kCCTFreInSameTask);
BASE_DECLARE_FEATURE(kCCTGoogleBottomBar);
BASE_DECLARE_FEATURE(kCCTGoogleBottomBarVariantLayouts);
BASE_DECLARE_FEATURE(kCCTIncognitoAvailableToThirdParty);
BASE_DECLARE_FEATURE(kCCTMinimized);
BASE_DECLARE_FEATURE(kCCTMinimizedEnabledByDefault);
BASE_DECLARE_FEATURE(kCCTNavigationInfoScreenshot);
BASE_DECLARE_FEATURE(kCCTNavigationMetrics);
BASE_DECLARE_FEATURE(kCCTNavigationalPrefetch);
BASE_DECLARE_FEATURE(kCCTOpenInBrowserButtonIfAllowedByEmbedder);
BASE_DECLARE_FEATURE(kCCTOpenInBrowserButtonIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kCCTPageContentRequestAllowed);
BASE_DECLARE_FEATURE(kCCTPageContentRequestEnabled);
BASE_DECLARE_FEATURE(kCCTRealtimeEngagementEventsInBackground);
BASE_DECLARE_FEATURE(kCCTReportParallelRequestStatus);
BASE_DECLARE_FEATURE(kCCTReportPrerenderEvents);
BASE_DECLARE_FEATURE(kCCTResetTimeoutAllowed);
BASE_DECLARE_FEATURE(kCCTResizableForThirdParties);
BASE_DECLARE_FEATURE(kCCTRetainingStateInMemory);
BASE_DECLARE_FEATURE(kCCTTabModalDialog);
BASE_DECLARE_FEATURE(kCCTTabSwitcherEnabledForChromeExperiment);
BASE_DECLARE_FEATURE(kCCTTabSwitcherEnabledForEmbedderExperiment);
BASE_DECLARE_FEATURE(kCacheDeprecatedSystemLocationSetting);
BASE_DECLARE_FEATURE(kCacheIsGoogleSigned);
BASE_DECLARE_FEATURE(kCacheIsMultiInstanceApi31Enabled);
BASE_DECLARE_FEATURE(kCastDeviceFilter);
BASE_DECLARE_FEATURE(kCctTabResumption);
BASE_DECLARE_FEATURE(kChangeUnfocusedPriority);
BASE_DECLARE_FEATURE(kChromeNativeUrlOverriding);
BASE_DECLARE_FEATURE(kChromeShareScreenshot);
BASE_DECLARE_FEATURE(kChromeSharingHubLaunchAdjacent);
BASE_DECLARE_FEATURE(kChromeSurveyNextAndroid);
BASE_DECLARE_FEATURE(kClampAutomotiveScaling);
BASE_DECLARE_FEATURE(kClankGlicContextMenu);
BASE_DECLARE_FEATURE(kClankStartupLatencyInjection);
BASE_DECLARE_FEATURE(kClankWhatsNew);
BASE_DECLARE_FEATURE(kClearIntentWhenRecreated);
BASE_DECLARE_FEATURE(kCommandLineOnNonRooted);
BASE_DECLARE_FEATURE(kCompositorViewRemeasureFix);
BASE_DECLARE_FEATURE(kContextualPanelCloseButton);
BASE_DECLARE_FEATURE(kContextualSearchDisableOnlineDetection);
BASE_DECLARE_FEATURE(kContextualSearchSuppressShortView);
BASE_DECLARE_FEATURE(kControlsInBrowserToolbarSwipeMock);
BASE_DECLARE_FEATURE(kControlsVisibilityFromNavigations);
BASE_DECLARE_FEATURE(kCopyLinkToHighlight);
BASE_DECLARE_FEATURE(kCrossDeviceTabPaneAndroid);
BASE_DECLARE_FEATURE(kCrossDeviceTaskHandoff);
BASE_DECLARE_FEATURE(kCrossWindowTabGroupOperations);
BASE_DECLARE_FEATURE(kDebugToolbarPositioning);
BASE_DECLARE_FEATURE(kDefaultBrowserPromoAndroid2);
BASE_DECLARE_FEATURE(kDefaultBrowserPromoEntryPoint);
BASE_DECLARE_FEATURE(kDefaultBrowserPromoFre);
BASE_DECLARE_FEATURE(kDeferNavigationStateChanged);
BASE_DECLARE_FEATURE(kDesktopAndroidBackgroundTabLoading);
BASE_DECLARE_FEATURE(kDesktopAndroidLinkCapturing);
BASE_DECLARE_FEATURE(kDesktopAndroidTWADeleteBrowserData);
BASE_DECLARE_FEATURE(kDesktopAndroidTWADisclosures);
BASE_DECLARE_FEATURE(kDesktopAndroidTWADisclosuresHelpLink);
BASE_DECLARE_FEATURE(kDesktopUAOnConnectedDisplay);
BASE_DECLARE_FEATURE(kDeviceAuthenticatorAndroidx);
BASE_DECLARE_FEATURE(kDisableGridTabSwitcher);
BASE_DECLARE_FEATURE(kDisablePartnerHomepageAndroid);
BASE_DECLARE_FEATURE(kDisableScrollbarOfFadingEdgeScrollView);
BASE_DECLARE_FEATURE(kDiscardPageWithCrashedSubframePolicy);
BASE_DECLARE_FEATURE(kDontAutoHideBrowserControls);
BASE_DECLARE_FEATURE(kDontPrefetchLibraries);
BASE_DECLARE_FEATURE(kEdgeToEdgeAutomotive);
BASE_DECLARE_FEATURE(kEdgeToEdgeBottomChin);
BASE_DECLARE_FEATURE(kEdgeToEdgeEverywhere);
BASE_DECLARE_FEATURE(kEdgeToEdgeExtraLogs);
BASE_DECLARE_FEATURE(kEdgeToEdgeMonitorConfigurations);
BASE_DECLARE_FEATURE(kEdgeToEdgeTablet);
BASE_DECLARE_FEATURE(kEdgeToEdgeUseBackupNavbarInsets);
BASE_DECLARE_FEATURE(kEdgelessTopInset);
BASE_DECLARE_FEATURE(kEnableAndroidSidePanel);
BASE_DECLARE_FEATURE(kEnableAndroidSidePanelDevFeature);
BASE_DECLARE_FEATURE(kEnableAndroidSidePanelLogs);
BASE_DECLARE_FEATURE(kEnableBrowserWindowInterfaceForCustomTabActivity);
BASE_DECLARE_FEATURE(kEnableEscapeHandlingForSecondaryActivities);
BASE_DECLARE_FEATURE(kEnableSwipeToSwitchPane);
BASE_DECLARE_FEATURE(kEnableToolbarPositioningInResizeMode);
BASE_DECLARE_FEATURE(kEnableXAxisActivityTransition);
BASE_DECLARE_FEATURE(kEnforceIncognitoIsolation);
BASE_DECLARE_FEATURE(kExperimentsForAgsa);
BASE_DECLARE_FEATURE(kFaviconDisableHostFallback);
BASE_DECLARE_FEATURE(kFlyoutInBookmarksBar);
BASE_DECLARE_FEATURE(kForceTranslucentNotificationTrampoline);
BASE_DECLARE_FEATURE(kFullscreenInsetsApiMigration);
BASE_DECLARE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive);
BASE_DECLARE_FEATURE(kGestureUserEducationBackSwipe);
BASE_DECLARE_FEATURE(kGmsCoreBindServiceOptimization);
BASE_DECLARE_FEATURE(kGridTabSwitcherSurfaceColorUpdate);
BASE_DECLARE_FEATURE(kHistoryPaneAndroid);
BASE_DECLARE_FEATURE(kHomeButtonRemoval);
BASE_DECLARE_FEATURE(kImprovedA2HS);
BASE_DECLARE_FEATURE(kInAppWindowManagerDeprecation);
BASE_DECLARE_FEATURE(kIncognitoAsWindowFullScreen);
BASE_DECLARE_FEATURE(kIncognitoModeForcedAndroid);
BASE_DECLARE_FEATURE(kIncognitoScreenshot);
BASE_DECLARE_FEATURE(kIncognitoThemeOverlayTesting);
BASE_DECLARE_FEATURE(kInlinePdfV2);
BASE_DECLARE_FEATURE(kInlinePdfV2Download);
BASE_DECLARE_FEATURE(kInlinePdfV2Incognito);
BASE_DECLARE_FEATURE(kKeyboardEscBackNavigation);
BASE_DECLARE_FEATURE(kLanguagesPreference);
BASE_DECLARE_FEATURE(kLaunchCauseScreenOffFix);
BASE_DECLARE_FEATURE(kLensOnQuickActionSearchWidget);
BASE_DECLARE_FEATURE(kLinkHoverStatusBar);
BASE_DECLARE_FEATURE(kLoadAllTabsAtStartup);
BASE_DECLARE_FEATURE(kLocationBarModelOptimizations);
BASE_DECLARE_FEATURE(kLockTopControlsOnLargeTabletsV2);
BASE_DECLARE_FEATURE(kLongScreenshotsLenientMemoryCheck);
BASE_DECLARE_FEATURE(kLongScreenshotsNoMemoryCheck);
BASE_DECLARE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition);
BASE_DECLARE_FEATURE(kMostVisitedTilesCustomization);
BASE_DECLARE_FEATURE(kMostVisitedTilesReselect);
BASE_DECLARE_FEATURE(kMoveToFrontInLaunchIntentDispatcher);
BASE_DECLARE_FEATURE(kMultiInstanceSharedPrefsMigration);
BASE_DECLARE_FEATURE(kNavigationListMenu);
BASE_DECLARE_FEATURE(kNotificationPermissionVariant);
BASE_DECLARE_FEATURE(kNotificationTrampoline);
BASE_DECLARE_FEATURE(kNotificationTrampolineNoNewTask);
BASE_DECLARE_FEATURE(kNtpAurora);
BASE_DECLARE_FEATURE(kNtpAuroraV2);
BASE_DECLARE_FEATURE(kNtpMvcRefactor);
BASE_DECLARE_FEATURE(kNtpVision);
BASE_DECLARE_FEATURE(kOmahaMinSdkVersionAndroid);
BASE_DECLARE_FEATURE(kOnDemandBackgroundTabContextCapture);
BASE_DECLARE_FEATURE(kOnDemandBackgroundTabContextCaptureOptimization);
BASE_DECLARE_FEATURE(kOnStartupWindowPolicy);
BASE_DECLARE_FEATURE(kOneStepAimAccess);
BASE_DECLARE_FEATURE(kOpenDownloadInPreferredApp);
BASE_DECLARE_FEATURE(kOptimizeGeolocationHeaderGeneration);
BASE_DECLARE_FEATURE(kOptionalButtonNoHardwareLayerKillswitch);
BASE_DECLARE_FEATURE(kPCCTMinimumHeight);
BASE_DECLARE_FEATURE(kPageAnnotationsService);
BASE_DECLARE_FEATURE(kPageContentProvider);
BASE_DECLARE_FEATURE(kPartnerCustomizationsUma);
BASE_DECLARE_FEATURE(kPdfLauncherActivity);
BASE_DECLARE_FEATURE(kPdfReuseFragment);
BASE_DECLARE_FEATURE(kPersistAcrossReboots);
BASE_DECLARE_FEATURE(kPersistAcrossRebootsDebugLogs);
BASE_DECLARE_FEATURE(kPowerSavingModeBroadcastReceiverInBackground);
BASE_DECLARE_FEATURE(kPreconnectOnTabCreation);
BASE_DECLARE_FEATURE(kPriceChangeModule);
BASE_DECLARE_FEATURE(kPrintSelectionMenu);
BASE_DECLARE_FEATURE(kProtectRecentlyVisibleTab);
BASE_DECLARE_FEATURE(kPwaRestoreUi);
BASE_DECLARE_FEATURE(kPwaRestoreUiAtStartup);
BASE_DECLARE_FEATURE(kQueuedCompositorWebContentsUpdates);
BASE_DECLARE_FEATURE(kReadAloudAudioOverviews);
BASE_DECLARE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT);
BASE_DECLARE_FEATURE(kReadAloudServerExperiments);
BASE_DECLARE_FEATURE(kRecordSuppressionMetrics);
BASE_DECLARE_FEATURE(kReengagementNotification);
BASE_DECLARE_FEATURE(kRelatedSearchesAllLanguage);
BASE_DECLARE_FEATURE(kRelatedSearchesSwitch);
BASE_DECLARE_FEATURE(kRemoveTabFocusOnShowingAndSelect);
BASE_DECLARE_FEATURE(kRobustWindowManagementExperimental);
BASE_DECLARE_FEATURE(kSafetyFrePromo);
BASE_DECLARE_FEATURE(kScheduleWindowCleaning);
BASE_DECLARE_FEATURE(kSearchInCCT);
BASE_DECLARE_FEATURE(kSearchInCCTAlternateTapHandling);
BASE_DECLARE_FEATURE(kSearchInCCTAlternateTapHandlingIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kSearchInCCTIfEnabledByEmbedder);
BASE_DECLARE_FEATURE(kSessionRestoreAfterCrash);
BASE_DECLARE_FEATURE(kSettingsInTab);
BASE_DECLARE_FEATURE(kSettingsInTabUrlNav);
BASE_DECLARE_FEATURE(kSettingsMultiColumn);
BASE_DECLARE_FEATURE(kSettingsSingleActivity);
BASE_DECLARE_FEATURE(kShareCustomActionsInCCT);
BASE_DECLARE_FEATURE(kSharingHubLinkToggle);
BASE_DECLARE_FEATURE(kShortCircuitUnfocusAnimation);
BASE_DECLARE_FEATURE(kShowTabListAnimations);
BASE_DECLARE_FEATURE(kSidePanelTopHairlineRefactorAndroid);
BASE_DECLARE_FEATURE(kSmallerTabStripTitleLimit);
BASE_DECLARE_FEATURE(kStartSurfaceReturnTime);
BASE_DECLARE_FEATURE(kSubmenusInAppMenu);
BASE_DECLARE_FEATURE(kSubmenusInAppMenuLff);
BASE_DECLARE_FEATURE(kSyncRestoreOnStartupPref);
BASE_DECLARE_FEATURE(kTabAndroidGracefulShutdown);
BASE_DECLARE_FEATURE(kTabBottomSheet);
BASE_DECLARE_FEATURE(kTabBottomSheetFullHeight);
BASE_DECLARE_FEATURE(kTabBottomSheetHalfHeight);
BASE_DECLARE_FEATURE(kTabBottomSheetResizeWebview);
BASE_DECLARE_FEATURE(kTabClosureCommittedMethodRefactor);
BASE_DECLARE_FEATURE(kTabClosureMethodRefactor);
BASE_DECLARE_FEATURE(kTabOpenerTracking);
BASE_DECLARE_FEATURE(kTabSearchForDesktop);
BASE_DECLARE_FEATURE(kTabSharingToolbarAndroid);
BASE_DECLARE_FEATURE(kTabStorageSqlitePrototype);
BASE_DECLARE_FEATURE(kTabStripAutoSelectOnCloseChange);
BASE_DECLARE_FEATURE(kTabStripHeightTransitionGlitchFix);
BASE_DECLARE_FEATURE(kTabStripLayoutTransitionDebounceFix);
BASE_DECLARE_FEATURE(kTabStripStopSpinnerOnLoadStop);
BASE_DECLARE_FEATURE(kTabSwitcherDragDropAndroid);
BASE_DECLARE_FEATURE(kTabSwitcherGroupSuggestionsAndroid);
BASE_DECLARE_FEATURE(kTabSwitcherGroupSuggestionsTestModeAndroid);
BASE_DECLARE_FEATURE(kTabSwitcherMessagesOnDesktopWindowingKillSwitch);
BASE_DECLARE_FEATURE(kTabWindowManagerReportIndicesMismatch);
BASE_DECLARE_FEATURE(kTestDefaultDisabled);
BASE_DECLARE_FEATURE(kTestDefaultEnabled);
BASE_DECLARE_FEATURE(kTextHighlightFullLink);
BASE_DECLARE_FEATURE(kThreeDotMenuBackButton);
BASE_DECLARE_FEATURE(kTipsSelfService);
BASE_DECLARE_FEATURE(kToolbarCaptureFixForSPAs);
BASE_DECLARE_FEATURE(kToolbarPhoneAnimationRefactor);
BASE_DECLARE_FEATURE(kToolbarProgressBarRefactor);
BASE_DECLARE_FEATURE(kToolbarSnapshotRefactor);
BASE_DECLARE_FEATURE(kToolbarTabletResizeRefactor);
BASE_DECLARE_FEATURE(kTouchToSearchCallout);
BASE_DECLARE_FEATURE(kTrustedWebActivityContactsDelegation);
BASE_DECLARE_FEATURE(kTweakApplicationPreloadLoadNativeFirst);
BASE_DECLARE_FEATURE(kTweakApplicationPreloadMoveWarmUp);
BASE_DECLARE_FEATURE(kTweakApplicationPreloadSkipNewInstance);
BASE_DECLARE_FEATURE(kTweakApplicationPreloadSkipWarmUp);
BASE_DECLARE_FEATURE(kUmaBackgroundSessions);
BASE_DECLARE_FEATURE(kUmaSessionCorrectnessFixes);
BASE_DECLARE_FEATURE(kUniversalKeyboardHandling);
BASE_DECLARE_FEATURE(kUnparcelIntentFileDescriptors);
BASE_DECLARE_FEATURE(kUseActivityManagerForTabActivation);
BASE_DECLARE_FEATURE(kUseAppTaskForCustomTabActivation);
BASE_DECLARE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid);
BASE_DECLARE_FEATURE(kUseWebUiNtpAndroid);
BASE_DECLARE_FEATURE(kVerifyStartupSigninState);
BASE_DECLARE_FEATURE(kVirtualKeyboardResizesContentTransientOvershootFix);
BASE_DECLARE_FEATURE(kVirtualKeyboardTransientInnerHeightFix);
// TODO(crbug.com/543076349): When removing this flag, move navigation bar color
// setup to BaseCustomTabActivity#performPreInflationStartup() and remove the
// calls from CustomTabActivity and finishNativeInitialization().
BASE_DECLARE_FEATURE(kWebAppNavigationBarThemeColor);
BASE_DECLARE_FEATURE(kWebAppShortEdgesCutoutMode);
BASE_DECLARE_FEATURE(kWebOtpCrossDeviceSimpleString);
BASE_DECLARE_FEATURE(kWebUiAndroidTheming);
BASE_DECLARE_FEATURE(kXplatSyncedSetupThemes);
BASE_DECLARE_FEATURE(kYourSavedInfoSettingsPageAndroid);
// go/keep-sorted end
// BASE_DECLARE_FEATURE_END

// clang-format on

// For FeatureParam, Alphabetical:
inline constexpr base::FeatureParam<int> kAppIntegrationMaxDonationCountParam(
    &kAndroidAppIntegrationMultiDataSource,
    "max_donation_count",
    100);

inline constexpr base::FeatureParam<std::string>
    kAndroidSidePanelDevFeatureScopeParam(&kEnableAndroidSidePanelDevFeature,
                                          "scope",
                                          "window");

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

inline constexpr base::FeatureParam<bool>
    kClearDeviceSignalsPermissionOnStartup{
        &kAndroidDeviceSignalsDisclaimer,
        "clear_device_signals_permission_on_startup", false};

inline constexpr base::FeatureParam<bool>
    kEnableAndroidSidePanelDisableAnimations(&kEnableAndroidSidePanel,
                                             "disable_animations",
                                             false);

inline constexpr base::FeatureParam<int> kGestureUserEducationPageDelay(
    &kGestureUserEducationBackSwipe,
    "gesture-user-education-page-delay",
    /*default_value=*/4000);

// The initial fallback delay in seconds for TabContextCaptureRequest before
// triggering page context capture if page load events do not arrive.
inline constexpr base::FeatureParam<int>
    kOnDemandBackgroundTabContextCaptureInitialFallbackDelaySeconds(
        &kOnDemandBackgroundTabContextCaptureOptimization,
        "initial_fallback_delay_seconds",
        /*default_value=*/25);

// The overall flush timeout in seconds for TabContextualizationController to
// flush pending page context callbacks if primary main frame load completion
// does not arrive within this duration, preventing indefinite hangs on pages
// with continuous subframe/ad loading.
inline constexpr base::FeatureParam<int>
    kOnDemandBackgroundTabContextCaptureOverallFlushTimeoutSeconds(
        &kOnDemandBackgroundTabContextCaptureOptimization,
        "overall_flush_timeout_seconds",
        /*default_value=*/5);

// The overall hard timeout in seconds for TabContextCaptureRequest covering
// the entire capture lifecycle (load wait, APC extraction, and screenshot)
// before aborting with UnableToCapture().
inline constexpr base::FeatureParam<int>
    kOnDemandBackgroundTabContextCaptureOverallTimeoutSeconds(
        &kOnDemandBackgroundTabContextCaptureOptimization,
        "overall_timeout_seconds",
        /*default_value=*/35);

inline constexpr base::FeatureParam<int> kProtectRecentlyVisibleTabDuration(
    &kProtectRecentlyVisibleTab,
    "duration_in_seconds",
    /*default_value=*/base::Minutes(10).InSeconds());

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

inline constexpr base::FeatureParam<std::string>
    kReadAloudAudioOverviewsSupportedLanguages(
        &kReadAloudAudioOverviews,
        "read_aloud_audio_overviews_supported_languages",
        /* default_value=*/"en");

inline constexpr base::FeatureParam<bool> kTouchToSearchCalloutIph(
    &kTouchToSearchCallout,
    "iph",
    /*default_value=*/false);

inline constexpr base::FeatureParam<bool>
    kTouchToSearchCalloutSnippetAsSubtitle(&kTouchToSearchCallout,
                                           "snippet_as_subtitle",
                                           /*default_value=*/false);

inline constexpr base::FeatureParam<bool>
    kXplatSyncedSetupThemesObservationOnly(&kXplatSyncedSetupThemes,
                                           "observation_only",
                                           /*default_value=*/false);

}  // namespace chrome::android

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
