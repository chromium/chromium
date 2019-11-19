// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_

#include <base/feature_list.h>
#include <jni.h>

namespace chrome {
namespace android {

// Alphabetical:
extern const base::Feature kAdjustWebApkInstallationSpace;
extern const base::Feature kAllowNewIncognitoTabIntents;
extern const base::Feature kAllowRemoteContextForNotifications;
extern const base::Feature kAndroidNightMode;
extern const base::Feature kAndroidNightModeCCT;
extern const base::Feature kAndroidNightModeForQ;
extern const base::Feature kAndroidPayIntegrationV1;
extern const base::Feature kAndroidPayIntegrationV2;
extern const base::Feature kAndroidPaymentApps;
extern const base::Feature kAndroidSearchEngineChoiceNotification;
extern const base::Feature kAndroidSetupSearchEngine;
extern const base::Feature kAndroidSiteSettingsUIRefresh;
extern const base::Feature kBackgroundTaskComponentUpdate;
extern const base::Feature kBookmarksShowInFolder;
extern const base::Feature kCloseTabSuggestions;
extern const base::Feature kCastDeviceFilter;
extern const base::Feature kCCTBackgroundTab;
extern const base::Feature kCCTExternalLinkHandling;
extern const base::Feature kCCTIncognito;
extern const base::Feature kCCTModule;
extern const base::Feature kCCTModuleCache;
extern const base::Feature kCCTModuleCustomHeader;
extern const base::Feature kCCTModuleCustomRequestHeader;
extern const base::Feature kCCTModuleDexLoading;
extern const base::Feature kCCTModulePostMessage;
extern const base::Feature kCCTModuleUseIntentExtras;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kCCTRedirectPreconnect;
extern const base::Feature kCCTReportParallelRequestStatus;
extern const base::Feature kCCTResourcePrefetch;
extern const base::Feature kCCTTargetTranslateLanguage;
extern const base::Feature kChromeDuetFeature;
extern const base::Feature kChromeDuetAdaptive;
extern const base::Feature kDontAutoHideBrowserControls;
extern const base::Feature kChromeDuetLabeled;
extern const base::Feature kChromeSharingHub;
extern const base::Feature kChromeSmartSelection;
extern const base::Feature kClickToCallOpenDialerDirectly;
extern const base::Feature kCommandLineOnNonRooted;
extern const base::Feature kContactsPickerSelectAll;
extern const base::Feature kContextMenuSearchWithGoogleLens;
extern const base::Feature kContentSuggestionsScrollToLoad;
extern const base::Feature kContextualSearchDefinitions;
extern const base::Feature kContextualSearchLongpressResolve;
extern const base::Feature kContextualSearchMlTapSuppression;
extern const base::Feature kContextualSearchSecondTap;
extern const base::Feature kContextualSearchSimplifiedServer;
extern const base::Feature kContextualSearchTapDisableOverride;
extern const base::Feature kContextualSearchTranslationModel;
extern const base::Feature kDarkenWebsitesCheckboxInThemesSetting;
extern const base::Feature kDirectActions;
extern const base::Feature kDontPrefetchLibraries;
extern const base::Feature kDownloadAutoResumptionThrottling;
extern const base::Feature kDownloadFileProvider;
extern const base::Feature kDownloadNotificationBadge;
extern const base::Feature kDownloadProgressInfoBar;
extern const base::Feature kDownloadRename;
extern const base::Feature kDrawVerticallyEdgeToEdge;
extern const base::Feature kEphemeralTab;
extern const base::Feature kEphemeralTabUsingBottomSheet;
extern const base::Feature kExploreSites;
extern const base::Feature kHandleMediaIntents;
extern const base::Feature kHomepageLocation;
extern const base::Feature kHorizontalTabSwitcherAndroid;
extern const base::Feature kImmersiveUiMode;
extern const base::Feature kImprovedA2HS;
extern const base::Feature kInlineUpdateFlow;
extern const base::Feature kIntentBlockExternalFormRedirectsNoGesture;
extern const base::Feature kJellyBeanSupported;
extern const base::Feature kLanguagesPreference;
extern const base::Feature kNewPhotoPicker;
extern const base::Feature kNotificationSuspender;
extern const base::Feature kNoCreditCardAbort;
extern const base::Feature kNTPLaunchAfterInactivity;
extern const base::Feature kOfflineHome;
extern const base::Feature kOfflineIndicatorV2;
extern const base::Feature kOmniboxSpareRenderer;
extern const base::Feature kOverlayNewLayout;
extern const base::Feature kPayWithGoogleV1;
extern const base::Feature kPhotoPickerVideoSupport;
extern const base::Feature kReachedCodeProfiler;
extern const base::Feature kReorderBookmarks;
extern const base::Feature kReaderModeInCCT;
extern const base::Feature kRevampedContextMenu;
extern const base::Feature kScrollToExpandPaymentHandler;
extern const base::Feature kSearchEnginePromoExistingDevice;
extern const base::Feature kSearchEnginePromoNewDevice;
extern const base::Feature kServiceManagerForBackgroundPrefetch;
extern const base::Feature kServiceManagerForDownload;
extern const base::Feature kSettingsModernStatusBar;
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
extern const base::Feature kTabSwitcherLongpressMenu;
extern const base::Feature kTabSwitcherOnReturn;
extern const base::Feature kTabToGTSAnimation;
extern const base::Feature kTrustedWebActivityPostMessage;
extern const base::Feature kStartSurfaceAndroid;
extern const base::Feature kUmaBackgroundSessions;
extern const base::Feature kUpdateNotificationSchedulingIntegration;
extern const base::Feature kUsageStatsFeature;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kVideoPersistence;
extern const base::Feature kVrBrowsingFeedback;
extern const base::Feature kWebApkAdaptiveIcon;

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
