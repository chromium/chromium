// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

BASE_DECLARE_FEATURE(kSearchPrefetchServicePrefetching);

BASE_DECLARE_FEATURE(kSearchPrefetchBlockBeforeHeaders);

BASE_DECLARE_FEATURE(kSearchPrefetchOnlyAllowDefaultMatchPreloading);

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
// Overrides the max cache size for testing. This should be used only when tests
// need to override the cache size dynamically. Otherwise, the cache size should
// be set through base::ScopedFeatureList.
void SetSearchPrefetchMaxCacheEntriesForTesting(size_t cache_site);

// The amount of time that needs to have elapsed before we consider a prefetch
// eligible to be served.
base::TimeDelta SearchPrefetchBlockHeadStart();

BASE_DECLARE_FEATURE(kSearchNavigationPrefetch);

// Feature params for the "pf" query param for suggest prefetch and navigation
// prefetch respectively. This param allows the search server to treat the
// requests differently based on the source.
extern const base::FeatureParam<std::string> kSuggestPrefetchParam;
extern const base::FeatureParam<std::string> kNavigationPrefetchParam;

// An experimental feature to measure if starting search prefetches during
// navigation events provides benefit over the typical navigation flow.
bool IsSearchNavigationPrefetchEnabled();

// A flavor of navigation prefetch that triggers when the user changes the
// selected index in omnibox to a search suggestion via arrow buttons. This is
// for Desktop only.
bool IsUpOrDownArrowPrefetchEnabled();

// A flavor of navigation prefetch that triggers when the user pushes the mouse
// down on a Search suggestion. This is for Desktop only.
bool IsSearchMouseDownPrefetchEnabled();

// A flavor of navigation prefetch that triggers when the user touches down on a
// Search suggestion. This is for Android only.
bool IsTouchDownPrefetchEnabled();

// Allows the top selection to be prefetched by navigation prefetch strategies.
bool AllowTopNavigationPrefetch();

// Allows search history suggestions to be prefetched by navigation prefetch
// strategies.
bool PrefetchSearchHistorySuggestions();

// Whether Omnibox prefetch and prerender should be restricted to the suggestion
// being the default match.
bool OnlyAllowDefaultMatchPreloading();

// When this feature is enabled, SearchPrefetchService will send a request to
// the network service to preload shared dictionary from the disk storage for
// the AutocompleteResult's `destination_url`.
BASE_DECLARE_FEATURE(kAutocompleteDictionaryPreload);

// The amount of time preloaded dictionary is kept alive.
extern const base::FeatureParam<base::TimeDelta>
    kAutocompletePreloadedDictionaryTimeout;

// If enabled, suppresses SearchPrefetch (https://crbug.com/350519234)
BASE_DECLARE_FEATURE(kSuppressesSearchPrefetchOnSlowNetwork);

// The threshold to determine if the network is slow or not.
extern const base::FeatureParam<base::TimeDelta>
    kSuppressesSearchPrefetchOnSlowNetworkThreshold;

// If enabled, SearchPrefetchService is ensured on
// `SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest`, so that
// navigation can consult the search prefetch cache even during browser startup.
BASE_DECLARE_FEATURE(kEnsureSearchPrefetchServiceOnInterceptor);

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
