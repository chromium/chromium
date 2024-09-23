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

extern const char kDefaultSearchEngineMetricSuffix[];
extern const char kDirectUrlInputMetricSuffix[];
extern const char kBookmarkBarMetricSuffix[];
extern const char kNewTabPageMetricSuffix[];
extern const char kLinkPreviewMetricsSuffix[];

bool IsSearchSuggestionPrerenderEnabled();

}  // namespace prerender_utils

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
