// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_

// This file is used to manage some static functions and constants for
// prerender2. Some typical cases can be:
// * Indicates whether a prerender2-related feature is enabled.
// * Stores the constants to avoid hardcoded strings.

namespace base {
struct Feature;
}  // namespace base

namespace prerender_utils {

// This is a temporal flag added for supporting a workaround that allows
// prerender2 to stop the location bar from the displaying prefetch flag, we
// will turn if off after we confirm the prerendered document will handle it by
// themselves.
extern const base::Feature kHidePrefetchParameter;

extern const char kDefaultSearchEngineMetricSuffix[];
extern const char kDirectUrlInputMetricSuffix[];

bool IsDirectUrlInputPrerenderEnabled();

bool IsSearchSuggestionPrerenderEnabled();

bool ShouldUpdateCacheEntryManually();

// Whether supporting upgrading a prefetch response to prerender page.
bool SearchPrefetchUpgradeToPrerenderIsEnabled();

}  // namespace prerender_utils

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
