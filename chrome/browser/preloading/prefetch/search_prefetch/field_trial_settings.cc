// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"

namespace {
size_t g_cache_size_for_testing = 0;
}  // namespace

BASE_FEATURE(kSearchPrefetchServicePrefetching,
             "SearchPrefetchServicePrefetching",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchPrefetchBlockBeforeHeaders,
             "SearchPrefetchBlockBeforeHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool SearchPrefetchBlockBeforeHeadersIsEnabled() {
  return base::FeatureList::IsEnabled(kSearchPrefetchBlockBeforeHeaders);
}

bool SearchPrefetchServicePrefetchingIsEnabled() {
  if (!base::FeatureList::IsEnabled(kSearchPrefetchServicePrefetching)) {
    return false;
  }

  return base::SysInfo::AmountOfPhysicalMemoryMB() >
         base::GetFieldTrialParamByFeatureAsInt(
             kSearchPrefetchServicePrefetching, "device_memory_threshold_MB",
             3000);
}

base::TimeDelta SearchPrefetchCachingLimit() {
  return base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "prefetch_caching_limit_ms", 60000));
}

size_t SearchPrefetchMaxAttemptsPerCachingDuration() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "max_attempts_per_caching_duration",
      7);
}

base::TimeDelta SearchPrefetchErrorBackoffDuration() {
  return base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "error_backoff_duration_ms", 60000));
}

size_t SearchPrefetchMaxCacheEntries() {
  if (g_cache_size_for_testing > 0) {
    return g_cache_size_for_testing;
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "cache_size", 10);
}

void SetSearchPrefetchMaxCacheEntriesForTesting(size_t cache_size) {
  g_cache_size_for_testing = cache_size;
}

base::TimeDelta SearchPrefetchBlockHeadStart() {
  return base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchBlockBeforeHeaders, "block_head_start_ms", 0));
}

BASE_FEATURE(kSearchNavigationPrefetch,
             "SearchNavigationPrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kSuggestPrefetchParam{
    &kSearchNavigationPrefetch, "suggest_prefetch_param", "cs"};

const base::FeatureParam<std::string> kNavigationPrefetchParam{
    &kSearchNavigationPrefetch, "navigation_prefetch_param", "cs"};

bool IsSearchNavigationPrefetchEnabled() {
  return base::FeatureList::IsEnabled(kSearchNavigationPrefetch);
}

bool IsUpOrDownArrowPrefetchEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(kSearchNavigationPrefetch,
                                                 "up_or_down_arrow", true);
}

bool IsSearchMouseDownPrefetchEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(kSearchNavigationPrefetch,
                                                 "mouse_down", true);
}

bool IsTouchDownPrefetchEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(kSearchNavigationPrefetch,
                                                 "touch_down", true);
}

bool AllowTopNavigationPrefetch() {
  return base::GetFieldTrialParamByFeatureAsBool(kSearchNavigationPrefetch,
                                                 "allow_top_selection", true);
}

bool PrefetchSearchHistorySuggestions() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSearchNavigationPrefetch, "prefetch_search_history", true);
}

BASE_FEATURE(kSearchPrefetchOnlyAllowDefaultMatchPreloading,
             "SearchPrefetchOnlyAllowDefaultMatchPreloading",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool OnlyAllowDefaultMatchPreloading() {
  return base::FeatureList::IsEnabled(
      kSearchPrefetchOnlyAllowDefaultMatchPreloading);
}

BASE_FEATURE(kAutocompleteDictionaryPreload,
             "AutocompleteDictionaryPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kAutocompletePreloadedDictionaryTimeout{
        &kAutocompleteDictionaryPreload,
        "autocomplete_preloaded_dictionary_timeout", base::Milliseconds(60000)};

BASE_FEATURE(kSuppressesSearchPrefetchOnSlowNetwork,
             "SuppressesSearchPrefetchOnSlowNetwork",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Regarding how this number was chosen, see the design doc linked from
// crbug.com/350519234.
const base::FeatureParam<base::TimeDelta>
    kSuppressesSearchPrefetchOnSlowNetworkThreshold{
        &kSuppressesSearchPrefetchOnSlowNetwork,
        "slow_network_threshold_for_search_prefetch", base::Milliseconds(208)};

BASE_FEATURE(kEnsureSearchPrefetchServiceOnInterceptor,
             "EnsureSearchprefetchServiceOnInterceptor",
             base::FEATURE_ENABLED_BY_DEFAULT);
