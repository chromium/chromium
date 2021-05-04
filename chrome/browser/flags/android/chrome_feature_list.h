// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// Alphabetical:
extern const base::Feature kAddToHomescreenIPH;
extern const base::Feature kAllowNewIncognitoTabIntents;
extern const base::Feature kAllowRemoteContextForNotifications;
extern const base::Feature kAndroidLayoutChangeTabReparenting;
extern const base::Feature kAndroidManagedByMenuItem;
extern const base::Feature kAndroidPartnerCustomizationPhenotype;
extern const base::Feature kAndroidSearchEngineChoiceNotification;
extern const base::Feature kAssistantIntentExperimentId;
extern const base::Feature kAssistantIntentPageUrl;
extern const base::Feature kAssistantIntentTranslateInfo;
extern const base::Feature kAppLaunchpad;
extern const base::Feature kAppMenuMobileSiteOption;
extern const base::Feature kBackgroundTaskComponentUpdate;
extern const base::Feature kBackgroundThreadPool;
extern const base::Feature kBentoOffline;
extern const base::Feature kBookmarkBottomSheet;
extern const base::Feature kCloseTabSuggestions;
extern const base::Feature kCriticalPersistedTabData;
extern const base::Feature kCastDeviceFilter;
extern const base::Feature kCCTBackgroundTab;
extern const base::Feature kCCTClientDataHeader;
extern const base::Feature kCCTExternalLinkHandling;
extern const base::Feature kCCTHideVisits;
extern const base::Feature kCCTIncognito;
extern const base::Feature kCCTIncognitoAvailableToThirdParty;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kCCTRedirectPreconnect;
extern const base::Feature kCCTRemoveRemoteViewIds;
extern const base::Feature kCCTReportParallelRequestStatus;
extern const base::Feature kCCTResourcePrefetch;
extern const base::Feature kDontAutoHideBrowserControls;
extern const base::Feature kChromeShareHighlightsAndroid;
extern const base::Feature kChromeShareLongScreenshot;
extern const base::Feature kChromeShareQRCode;
extern const base::Feature kChromeShareScreenshot;
extern const base::Feature kChromeSharingHub;
extern const base::Feature kChromeSharingHubV15;
extern const base::Feature kChromeStartupDelegate;
extern const base::Feature kChromeSurveyNextAndroid;
extern const base::Feature kCommandLineOnNonRooted;
extern const base::Feature kCommerceMerchantViewer;
extern const base::Feature kConditionalTabStripAndroid;
extern const base::Feature kContextMenuEnableLensShoppingAllowlist;
extern const base::Feature kContextMenuGoogleLensChip;
extern const base::Feature kContextMenuPerformanceInfo;
extern const base::Feature kContextMenuSearchWithGoogleLens;
extern const base::Feature kContextMenuShopWithGoogleLens;
extern const base::Feature kContextMenuSearchAndShopWithGoogleLens;
extern const base::Feature kContextMenuTranslateWithGoogleLens;
extern const base::Feature kContentSuggestionsScrollToLoad;
extern const base::Feature kContextualSearchDebug;
extern const base::Feature kContextualSearchForceCaption;
extern const base::Feature kContextualSearchLegacyHttpPolicy;
extern const base::Feature kContextualSearchLiteralSearchTap;
extern const base::Feature kContextualSearchLongpressResolve;
extern const base::Feature kContextualSearchMlTapSuppression;
extern const base::Feature kContextualSearchSecondTap;
extern const base::Feature kContextualSearchTapDisableOverride;
extern const base::Feature kContextualSearchThinWebViewImplementation;
extern const base::Feature kContextualSearchTranslations;
extern const base::Feature kDarkenWebsitesCheckboxInThemesSetting;
extern const base::Feature kDirectActions;
extern const base::Feature kDontPrefetchLibraries;
extern const base::Feature kDownloadAutoResumptionThrottling;
extern const base::Feature kDownloadFileProvider;
extern const base::Feature kDownloadNotificationBadge;
extern const base::Feature kDownloadProgressInfoBar;
extern const base::Feature kDownloadRename;
extern const base::Feature kDuetTabStripIntegrationAndroid;
extern const base::Feature kEnhancedProtectionPromoCard;
extern const base::Feature kEphemeralTabUsingBottomSheet;
extern const base::Feature kExperimentsForAgsa;
extern const base::Feature kExploreSites;
extern const base::Feature kFocusOmniboxInIncognitoTabIntents;
extern const base::Feature kGoogleLensSdkIntent;
extern const base::Feature kHandleMediaIntents;
extern const base::Feature kHomepagePromoCard;
extern const base::Feature kImmersiveUiMode;
extern const base::Feature kIncognitoScreenshot;
extern const base::Feature kImprovedA2HS;
extern const base::Feature kInlineUpdateFlow;
extern const base::Feature kInstantStart;
extern const base::Feature kKitKatSupported;
extern const base::Feature kLanguagesPreference;
extern const base::Feature kLensCameraAssistedSearch;
extern const base::Feature kNotificationSuspender;
extern const base::Feature kOfflineIndicatorV2;
extern const base::Feature kOfflineMeasurementsBackgroundTask;
extern const base::Feature kOmniboxSpareRenderer;
extern const base::Feature kPageAnnotationsService;
extern const base::Feature kProbabilisticCryptidRenderer;
extern const base::Feature kReachedCodeProfiler;
extern const base::Feature kReengagementNotification;
extern const base::Feature kReaderModeInCCT;
extern const base::Feature kRelatedSearches;
extern const base::Feature kRelatedSearchesUi;
extern const base::Feature kSearchEnginePromoExistingDevice;
extern const base::Feature kSearchEnginePromoNewDevice;
extern const base::Feature kServiceManagerForBackgroundPrefetch;
extern const base::Feature kServiceManagerForDownload;
extern const base::Feature kShareButtonInTopToolbar;
extern const base::Feature kShoppingAssist;
extern const base::Feature kSpannableInlineAutocomplete;
extern const base::Feature kSpecialLocaleWrapper;
extern const base::Feature kSpecialUserDecision;
extern const base::Feature kSwapPixelFormatToFixConvertFromTranslucent;
extern const base::Feature kTabEngagementReportingAndroid;
extern const base::Feature kTabGroupsAndroid;
extern const base::Feature kTabGroupsContinuationAndroid;
extern const base::Feature kTabGroupsUiImprovementsAndroid;
extern const base::Feature kTabGridLayoutAndroid;
extern const base::Feature kTabReparenting;
extern const base::Feature kTabSwitcherOnReturn;
extern const base::Feature kTabToGTSAnimation;
extern const base::Feature kTestDefaultDisabled;
extern const base::Feature kTestDefaultEnabled;
extern const base::Feature kThemeRefactorAndroid;
extern const base::Feature kToolbarIphAndroid;
extern const base::Feature kToolbarIphAndroidCohort1;
extern const base::Feature kToolbarIphAndroidCohort2;
extern const base::Feature kToolbarIphAndroidCohort3;
extern const base::Feature kToolbarIphAndroidCohort4;
extern const base::Feature kToolbarIphAndroidCohort5;
extern const base::Feature kToolbarIphAndroidCohort6;
extern const base::Feature kToolbarIphAndroidCohort7;
extern const base::Feature kToolbarIphAndroidCohort8;
extern const base::Feature kToolbarMicIphAndroid;
extern const base::Feature kToolbarUseHardwareBitmapDraw;
extern const base::Feature kTrustedWebActivityLocationDelegation;
extern const base::Feature kTrustedWebActivityNewDisclosure;
extern const base::Feature kTrustedWebActivityPostMessage;
extern const base::Feature kTrustedWebActivityQualityEnforcement;
extern const base::Feature kTrustedWebActivityQualityEnforcementForced;
extern const base::Feature kTrustedWebActivityQualityEnforcementWarning;
extern const base::Feature kStartSurfaceAndroid;
extern const base::Feature kUmaBackgroundSessions;
extern const base::Feature kUpdateNotificationSchedulingIntegration;
extern const base::Feature
    kUpdateNotificationScheduleServiceImmediateShowOption;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kVoiceSearchAudioCapturePolicy;
extern const base::Feature kVoiceButtonInTopToolbar;
extern const base::Feature kVrBrowsingFeedback;
extern const base::Feature kPrefetchNotificationSchedulingIntegration;

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
