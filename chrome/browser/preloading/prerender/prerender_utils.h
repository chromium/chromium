// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_

#include "base/feature_list.h"

// This file is used to manage some static functions and constants for
// prerender2. Some typical cases can be:
// * Indicates whether a prerender2-related feature is enabled.
// * Stores the constants to avoid hardcoded strings.
namespace prerender_utils {

// This is a temporal flag added for supporting a workaround that allows
// prerender2 to stop the location bar from the displaying prefetch flag, we
// will turn if off after we confirm the prerendered document will handle it by
// themselves.
BASE_DECLARE_FEATURE(kHidePrefetchParameter);

extern const char kDefaultSearchEngineMetricSuffix[];
extern const char kDirectUrlInputMetricSuffix[];
extern const char kBookmarkBarMetricSuffix[];

bool IsDirectUrlInputPrerenderEnabled();

bool IsSearchSuggestionPrerenderEnabled();

bool ShouldUpdateCacheEntryManually();

// Whether supporting upgrading a prefetch response to prerender page.
bool SearchPrefetchUpgradeToPrerenderIsEnabled();

// Whether supporting shareable search prefetch cache. By enabling this feature,
// prerender won't take the prefetched response away. Instead, it turns to read
// the response stored in StreamingSearchPrefetchURLLoader.
// Always return false if `SearchPrefetchUpgradeToPrerenderIsEnabled` returns
// false.
bool SearchPreloadShareableCacheIsEnabled();

}  // namespace prerender_utils

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
