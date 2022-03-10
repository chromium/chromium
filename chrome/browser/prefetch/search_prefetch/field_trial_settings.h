// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_

#include "base/feature_list.h"
#include "base/time/time.h"

extern const base::Feature kSearchPrefetchServicePrefetching;

extern const base::Feature kSearchPrefetchBlockBeforeHeaders;

// Whether matching prefetches can block navigation until they are determined to
// be serve-able or not based on headers.
bool SearchPrefetchBlockBeforeHeadersIsEnabled();

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

// The max number of stored cached prefetch responses. This is stored as a list
// of navigation URLs to prefetch URLs.
size_t SearchPrefetchMaxCacheEntries();

// The amount of time that needs to have elapsed before we consider a prefetch
// eligible to be served.
base::TimeDelta SearchPrefetchBlockHeadStart();

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
