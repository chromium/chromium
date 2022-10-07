// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// Alphabetical:
extern const base::Feature kAdaptiveButtonInTopToolbar;
extern const base::Feature kAdaptiveButtonInTopToolbarCustomizationV2;
extern const base::Feature kAddToHomescreenIPH;
extern const base::Feature kAllowNewIncognitoTabIntents;
extern const base::Feature kAndroidScrollOptimizations;
extern const base::Feature kAndroidSearchEngineChoiceNotification;
extern const base::Feature kAssistantConsentModal;
extern const base::Feature kAssistantConsentSimplifiedText;
extern const base::Feature kAssistantConsentV2;
extern const base::Feature kAssistantIntentExperimentId;
extern const base::Feature kAssistantIntentPageUrl;
extern const base::Feature kAssistantIntentTranslateInfo;
extern const base::Feature kAssistantNonPersonalizedVoiceSearch;
extern const base::Feature kAppLaunchpad;
extern const base::Feature kAppMenuMobileSiteOption;
extern const base::Feature kAppToWebAttribution;
extern const base::Feature kBackgroundThreadPool;
extern const base::Feature kBulkTabRestore;
extern const base::Feature kClearOmniboxFocusAfterNavigation;
extern const base::Feature kCloseTabSuggestions;
extern const base::Feature kCriticalPersistedTabData;
extern const base::Feature kCommerceCoupons;
extern const base::Feature kCastDeviceFilter;
extern const base::Feature kCCTBackgroundTab;
extern const base::Feature kCCTBrandTransparency;
extern const base::Feature kCCTClientDataHeader;
extern const base::Feature kCCTIncognito;
extern const base::Feature kCCTIncognitoAvailableToThirdParty;
extern const base::Feature kCCTNewDownloadTab;
extern const base::Feature kCCTPackageNameRecording;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kCCTRealTimeEngagementSignals;
extern const base::Feature kCCTRedirectPreconnect;
extern const base::Feature kCCTRemoveRemoteViewIds;
extern const base::Feature kCCTReportParallelRequestStatus;
extern const base::Feature kCCTResizable90MaximumHeight;
extern const base::Feature kCCTResizableAllowResizeByUserGesture;
extern const base::Feature kCCTResizableForFirstParties;
extern const base::Feature kCCTResizableForThirdParties;
extern const base::Feature kCCTResizableWindowAboveNavbar;
extern const base::Feature kCCTResourcePrefetch;
extern const base::Feature kCCTRetainingState;
extern const base::Feature kCCTShowAboutBlankUrl;
extern const base::Feature kCCTToolbarCustomizations;
extern const base::Feature kDontAutoHideBrowserControls;
extern const base::Feature kCacheDeprecatedSystemLocationSetting;
extern const base::Feature kChromeNewDownloadTab;
extern const base::Feature kChromeShareLongScreenshot;
extern const base::Feature kChromeShareScreenshot;
extern const base::Feature kChromeSharingHub;
extern const base::Feature kChromeSharingHubLaunchAdjacent;
extern const base::Feature kChromeSurveyNextAndroid;
extern const base::Feature kCommandLineOnNonRooted;
extern const base::Feature kConditionalTabStripAndroid;
extern const base::Feature kContextMenuEnableLensShoppingAllowlist;
extern const base::Feature kContextMenuGoogleLensChip;
extern const base::Feature kContextMenuPerformanceInfo;
extern const base::Feature kContextMenuPopupStyle;
extern const base::Feature kContextMenuSearchWithGoogleLens;
extern const base::Feature kContextMenuShopWithGoogleLens;
extern const base::Feature kContextMenuSearchAndShopWithGoogleLens;
extern const base::Feature kContextMenuTranslateWithGoogleLens;
extern const base::Feature kContextualSearchDelayedIntelligence;
extern const base::Feature kContextualSearchDisableOnlineDetection;
extern const base::Feature kContextualSearchForceCaption;
extern const base::Feature kContextualSearchSuppressShortView;
extern const base::Feature kContextualSearchThinWebViewImplementation;
extern const base::Feature kContextualTriggersSelectionHandles;
extern const base::Feature kContextualTriggersSelectionMenu;
extern const base::Feature kContextualTriggersSelectionSize;
extern const base::Feature kDirectActions;
extern const base::Feature kDisableCompositedProgressBar;
extern const base::Feature kDontPrefetchLibraries;
extern const base::Feature kDownloadAutoResumptionThrottling;
extern const base::Feature kDownloadFileProvider;
extern const base::Feature kDownloadHomeForExternalApp;
extern const base::Feature kDownloadNotificationBadge;
extern const base::Feature kDownloadRename;
extern const base::Feature kDuetTabStripIntegrationAndroid;
extern const base::Feature kExperimentsForAgsa;
extern const base::Feature kExploreSites;
extern const base::Feature kFocusOmniboxInIncognitoTabIntents;
extern const base::Feature kGridTabSwitcherForTablets;
extern const base::Feature kHandleMediaIntents;
extern const base::Feature kImmersiveUiMode;
extern const base::Feature kIncognitoReauthenticationForAndroid;
extern const base::Feature kIncognitoScreenshot;
extern const base::Feature kInfobarScrollOptimization;
extern const base::Feature kImprovedA2HS;
extern const base::Feature kInstanceSwitcher;
extern const base::Feature kInstantStart;
extern const base::Feature kIsVoiceSearchEnabledCache;
extern const base::Feature kKitKatSupported;
extern const base::Feature kLanguagesPreference;
extern const base::Feature kLensCameraAssistedSearch;
extern const base::Feature kLensOnQuickActionSearchWidget;
extern const base::Feature kLocationBarModelOptimizations;
extern const base::Feature kMostRecentTabOnBackgroundCloseTab;
extern const base::Feature kNewInstanceFromDraggedLink;
extern const base::Feature kNewTabPageTilesTitleWrapAround;
extern const base::Feature kNewWindowAppMenu;
extern const base::Feature kNotificationPermissionVariant;
extern const base::Feature kOmahaMinSdkVersionAndroid;
extern const base::Feature kOmniboxModernizeVisualUpdate;
extern const base::Feature kOptimizeGeolocationHeaderGeneration;
extern const base::Feature kOSKResizesVisualViewport;
extern const base::Feature kPageAnnotationsService;
extern const base::Feature kBookmarksImprovedSaveFlow;
extern const base::Feature kBookmarksRefresh;
extern const base::Feature kBackGestureRefactorAndroid;
extern const base::Feature kOptimizeLayoutsForPullRefresh;
extern const base::Feature kPostTaskFocusTab;
extern const base::Feature kProbabilisticCryptidRenderer;
extern const base::Feature kReachedCodeProfiler;
extern const base::Feature kReengagementNotification;
extern const base::Feature kReaderModeInCCT;
extern const base::Feature kRelatedSearches;
extern const base::Feature kRelatedSearchesAlternateUx;
extern const base::Feature kRelatedSearchesInBar;
extern const base::Feature kRelatedSearchesSimplifiedUx;
extern const base::Feature kRelatedSearchesUi;
extern const base::Feature kRequestDesktopSiteDefaults;
extern const base::Feature kRequestDesktopSiteDefaultsControl;
extern const base::Feature kRequestDesktopSiteDefaultsControlSynthetic;
extern const base::Feature kRequestDesktopSiteDefaultsSynthetic;
extern const base::Feature kRequestDesktopSiteOptInControlSynthetic;
extern const base::Feature kRequestDesktopSiteOptInSynthetic;
extern const base::Feature kRequestDesktopSiteDefaultsDowngrade;
extern const base::Feature kSearchEnginePromoExistingDevice;
extern const base::Feature kSearchEnginePromoExistingDeviceV2;
extern const base::Feature kSearchEnginePromoNewDevice;
extern const base::Feature kSearchEnginePromoNewDeviceV2;
extern const base::Feature kShareButtonInTopToolbar;
extern const base::Feature kSharingHubLinkToggle;
extern const base::Feature kShowScrollableMVTOnNTPAndroid;
extern const base::Feature kFeedPositionAndroid;
extern const base::Feature kSafeModeForCachedFlags;
extern const base::Feature kSearchResumptionModuleAndroid;
extern const base::Feature kSpannableInlineAutocomplete;
extern const base::Feature kSpecialLocaleWrapper;
extern const base::Feature kSpecialUserDecision;
extern const base::Feature kSplitCompositorTask;
extern const base::Feature kStoreHoursAndroid;
extern const base::Feature kSuppressToolbarCaptures;
extern const base::Feature kSwapPixelFormatToFixConvertFromTranslucent;
extern const base::Feature kTabEngagementReportingAndroid;
extern const base::Feature kTabGroupsAndroid;
extern const base::Feature kTabGroupsContinuationAndroid;
extern const base::Feature kTabGroupsUiImprovementsAndroid;
extern const base::Feature kTabGroupsForTablets;
extern const base::Feature kTabGridLayoutAndroid;
extern const base::Feature kTabReparenting;
extern const base::Feature kTabSelectionEditorV2;
extern const base::Feature kTabStripImprovements;
extern const base::Feature kDiscoverFeedMultiColumn;
extern const base::Feature kTabSwitcherOnReturn;
extern const base::Feature kTabToGTSAnimation;
extern const base::Feature kTestDefaultDisabled;
extern const base::Feature kTestDefaultEnabled;
extern const base::Feature kToolbarMicIphAndroid;
extern const base::Feature kToolbarPhoneOptimizations;
extern const base::Feature kToolbarScrollAblationAndroid;
extern const base::Feature kToolbarUseHardwareBitmapDraw;
extern const base::Feature kTrustedWebActivityPostMessage;
extern const base::Feature kTrustedWebActivityQualityEnforcement;
extern const base::Feature kTrustedWebActivityQualityEnforcementForced;
extern const base::Feature kTrustedWebActivityQualityEnforcementWarning;
extern const base::Feature kShowExtendedPreloadingSetting;
extern const base::Feature kStartSurfaceAndroid;
extern const base::Feature kStartSurfaceReturnTime;
extern const base::Feature kStartSurfaceRefactor;
extern const base::Feature kUmaBackgroundSessions;
extern const base::Feature kUpdateHistoryEntryPointsInIncognito;
extern const base::Feature
    kUpdateNotificationScheduleServiceImmediateShowOption;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kVoiceSearchAudioCapturePolicy;
extern const base::Feature kVoiceButtonInTopToolbar;
extern const base::Feature kVrBrowsingFeedback;
extern const base::Feature kWebOtpCrossDeviceSimpleString;
extern const base::Feature kWebApkInstallService;
extern const base::Feature kWebApkTrampolineOnInitialIntent;

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
