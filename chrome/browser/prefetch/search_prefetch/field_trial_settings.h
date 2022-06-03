// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_

#include "base/feature_list.h"
#include "base/time/time.h"

extern const base::Feature kSearchPrefetchService;
extern const base::Feature kSearchPrefetchServicePrefetching;

extern const char kSearchPrefetchServiceCommandLineFlag[];

// Whether the search prefetch service and other objects should be created.
bool SearchPrefetchServiceIsEnabled();

// Whether the search prefetch service actually initiates prefetches.
bool SearchPrefetchServicePrefetchingIsEnabled();

// The amount of time a response is considered valid after the prefetch starts.
base::TimeDelta SearchPrefetchCachingLimit();

// The number of prefetches that can be initiated (but not served) within a time
// period as long as |SearchPrefetchCachingLimit()|
size_t SearchPrefetchMaxAttemptsPerCachingDuration();

// The amount of time that a network error will cause the search prefetch
// service to stop prefetching responses.
base::TimeDelta SearchPrefetchErrorBackoffDuration();

// Only prefetch results when they are the top match and the default match.
// Nothing is prefetched if the default match is not prefetchable.
bool SearchPrefetchOnlyFetchDefaultMatch();

// When a request is inflight, but no longer shows up in the match list, whether
// the request is canceled or allowed to finish.
bool SearchPrefetchShouldCancelUneededInflightRequests();

// Whether Search Prefetch should use a streaming model to serve requests or
// wait for the entire response to be streamed before being able to serve
// requests.
bool StreamSearchPrefetchResponses();

// The max number of stored cached prefetch responses. This is stored as a list
// of navigation URLs to prefetch URLs.
size_t SearchPrefetchMaxCacheEntries();

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
