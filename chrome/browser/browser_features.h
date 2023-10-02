// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

BASE_DECLARE_FEATURE(kClosedTabCache);

BASE_DECLARE_FEATURE(kDestroyProfileOnBrowserClose);
BASE_DECLARE_FEATURE(kDestroySystemProfiles);

BASE_DECLARE_FEATURE(kDevToolsTabTarget);
BASE_DECLARE_FEATURE(kDevToolsVeLogging);

BASE_DECLARE_FEATURE(kNukeProfileBeforeCreateMultiAsync);

BASE_DECLARE_FEATURE(kPromoBrowserCommands);
extern const char kBrowserCommandIdParam[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kQuickSettingsPWANotifications);
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDoubleTapToZoomInTabletMode);
#endif

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kEnableUniveralLinks);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kCopyLinkToText);
BASE_DECLARE_FEATURE(kMuteNotificationSnoozeAction);
#endif

BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlocked);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlockedWarning);
BASE_DECLARE_FEATURE(kTriggerNetworkDataMigration);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kTabCaptureBlueBorderCrOS);
#endif

BASE_DECLARE_FEATURE(kWebUsbDeviceDetection);

BASE_DECLARE_FEATURE(kCertificateTransparencyAskBeforeEnabling);

BASE_DECLARE_FEATURE(kLargeFaviconFromGoogle);
extern const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip;

BASE_DECLARE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch);

BASE_DECLARE_FEATURE(kKeyPinningComponentUpdater);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kAppBoundEncryptionMetrics);
BASE_DECLARE_FEATURE(kLockProfileCookieDatabase);
BASE_DECLARE_FEATURE(kNoPreReadMainDll);
#endif

BASE_DECLARE_FEATURE(kFlexOrgManagementDisclosure);

BASE_DECLARE_FEATURE(kFedCmWithoutThirdPartyCookies);

BASE_DECLARE_FEATURE(kIncomingCallNotifications);

// This flag is used for enabling Omnibox triggered prerendering. See
// crbug.com/1166085 for more details of Omnibox triggered prerendering.
BASE_DECLARE_FEATURE(kOmniboxTriggerForPrerender2);

// This flag is used for enabling Bookmark triggered prerendering. See
// crbug.com/1422819 for more details of Bookmark triggered prerendering.
BASE_DECLARE_FEATURE(kBookmarkTriggerForPrerender2);

// This flag is used for enabling New Tab Page triggered prerendering. See
// crbug.com/1462832 for more details of New Tab Page triggered prerendering.
BASE_DECLARE_FEATURE(kNewTabPageTriggerForPrerender2);
// This parameter is used to set a time threshold for triggering onMouseHover
// prerender. For example, if the value is 300, the New Tab Page prerender
// will start after 300ms after mouseHover duration is over 300ms.
const base::FeatureParam<int>
    kNewTabPagePrerenderStartDelayOnMouseHoverByMiliSeconds{
        &features::kNewTabPageTriggerForPrerender2,
        "prerender_start_delay_on_mouse_hover_ms", 300};

// This flag controls whether to trigger prerendering when the default search
// engine suggests to prerender a search result.
BASE_DECLARE_FEATURE(kSupportSearchSuggestionForPrerender2);
enum class SearchSuggestionPrerenderImplementationType {
  kUsePrefetch,
  kIgnorePrefetch,
};
extern const base::FeatureParam<SearchSuggestionPrerenderImplementationType>
    kSearchSuggestionPrerenderImplementationTypeParam;
// Indicates whether to make search prefetch response shareable to prerender.
// When allowing this, prerender can only copy the cache but cannot take over
// the ownership.
enum class SearchPreloadShareableCacheType {
  kEnabled,
  kDisabled,
};

extern const base::FeatureParam<SearchPreloadShareableCacheType>
    kSearchPreloadShareableCacheTypeParam;

// Disables prerendering on the default search engine predictor. This is useful
// in comparing the impact of the SupportSearchSuggestionForPrerender2 feature
// during its rollout. Once that rollout is complete, this feature should be
// removed and instead we should add a new long-term holdback to
// PreloadingConfig.
BASE_DECLARE_FEATURE(kPrerenderDSEHoldback);

// This is used to enable an experiment for modifying confidence cutoff of
// prerender and preconnect for autocomplete action predictor.
BASE_DECLARE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff);

BASE_DECLARE_FEATURE(kOmniboxTriggerForNoStatePrefetch);

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
