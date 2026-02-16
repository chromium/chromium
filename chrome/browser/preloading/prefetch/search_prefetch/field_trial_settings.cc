// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
size_t g_cache_size_for_testing = 0;

GURL GetDefaultSearchEngineUrl(content::BrowserContext* browser_context) {
  auto* template_url_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!template_url_service) {
    return GURL();
  }

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  return default_search_engine ? default_search_engine->GenerateSearchURL(
                                     template_url_service->search_terms_data())
                               : GURL();
}
}  // namespace

BASE_FEATURE(kSearchPrefetchServicePrefetching,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchPrefetchWithNoVarySearchDiskCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill-switch for debugging CacheAliasLoader and NVS's cache hit rate.
// TODO(https://crbug.com/413557424): Remove the DryRun mode once the
// investigation is done.
BASE_FEATURE(kCacheAliasLoaderDryRunMode, base::FEATURE_ENABLED_BY_DEFAULT);

bool SearchPrefetchServicePrefetchingIsEnabled() {
  if (!base::FeatureList::IsEnabled(kSearchPrefetchServicePrefetching)) {
    return false;
  }

  return base::SysInfo::AmountOfPhysicalMemory().InMiB() >
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

BASE_FEATURE(kSearchNavigationPrefetch,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID)
);

const base::FeatureParam<std::string> kSuggestPrefetchParam{
    &kSearchNavigationPrefetch, "suggest_prefetch_param", "cs"};

const base::FeatureParam<std::string> kNavigationPrefetchParam{
    &kSearchNavigationPrefetch, "navigation_prefetch_param", "op"};

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
             base::FEATURE_ENABLED_BY_DEFAULT);

bool OnlyAllowDefaultMatchPreloading() {
  return base::FeatureList::IsEnabled(
      kSearchPrefetchOnlyAllowDefaultMatchPreloading);
}

bool IsNoVarySearchDiskCacheEnabled() {
  return base::FeatureList::IsEnabled(net::features::kHttpCacheNoVarySearch) &&
         base::FeatureList::IsEnabled(kSearchPrefetchWithNoVarySearchDiskCache);
}

bool CacheAliasLoaderDryRunModeEnabled() {
  return base::FeatureList::IsEnabled(kCacheAliasLoaderDryRunMode);
}

// Debugging feature flag to verify the keep alive request's success rate.
BASE_FEATURE(kSearchPrefetchBeaconLogging, base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter is supposed to be empty, and UA will use the default search
// engine's URL. It will be set in case that search engine provider uses a
// different host to classify activated traffic, or for testing.
const base::FeatureParam<std::string> kSearchPrefetchBeaconHost{
    &kSearchPrefetchBeaconLogging, "search_prefetch_beacon_host", ""};

bool IsSearchPrefetchBeaconLoggingEnabled(
    const GURL& url,
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(kSearchPrefetchBeaconLogging)) {
    return false;
  }
  const std::string& host = kSearchPrefetchBeaconHost.Get();
  if (!host.empty()) {
    if (url.GetHost() != host) {
      return false;
    }
  } else if (!url::IsSameOriginWith(
                 url, GetDefaultSearchEngineUrl(browser_context))) {
    return false;
  }
  std::string value;
  if (!net::GetValueForKeyInQuery(url, "pf", &value)) {
    return false;
  }
  return value == kNavigationPrefetchParam.Get() ||
         value == kSuggestPrefetchParam.Get();
}

bool IsPrefetchIncognitoEnabled() {
  return SearchPrefetchServicePrefetchingIsEnabled() &&
         IsSearchNavigationPrefetchEnabled() &&
         base::GetFieldTrialParamByFeatureAsBool(kSearchNavigationPrefetch,
                                                 "allow_incognito", true);
}

BASE_FEATURE(kAutocompleteDictionaryPreload, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kAutocompletePreloadedDictionaryTimeout{
        &kAutocompleteDictionaryPreload,
        "autocomplete_preloaded_dictionary_timeout", base::Milliseconds(60000)};

BASE_FEATURE(kSuppressesSearchPrefetchOnSlowNetwork,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Regarding how this number was chosen, see the design doc linked from
// crbug.com/350519234.
const base::FeatureParam<base::TimeDelta>
    kSuppressesSearchPrefetchOnSlowNetworkThreshold{
        &kSuppressesSearchPrefetchOnSlowNetwork,
        "slow_network_threshold_for_search_prefetch", base::Milliseconds(208)};

BASE_FEATURE(kSuppressPrefetchForUnsupportedSearchMode,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Allows to specify a comma-separated list of unsupported parameters, e.g.
// "udm=50,param2=value1=value2", in which case both "udm=50" and
// "param2=value1" and "param2=value2" will be considered as unsupported.
const base::FeatureParam<std::string> kUnsupportedSearchPrefetchModes{
    &kSuppressPrefetchForUnsupportedSearchMode,
    "unsupported_search_prefetch_modes", "udm=50"};

bool ShouldSuppressPrefetchForUnsupportedMode(const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          kSuppressPrefetchForUnsupportedSearchMode)) {
    return false;
  }
  std::vector<std::string> unsupported_modes =
      base::SplitString(base::GetFieldTrialParamValueByFeature(
                            kSuppressPrefetchForUnsupportedSearchMode,
                            "unsupported_search_prefetch_modes"),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string_view unsupported_mode : unsupported_modes) {
    std::vector<std::string_view> key_values =
        base::SplitStringPiece(unsupported_mode, "=", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    CHECK(!key_values.empty());
    std::string_view key = key_values[0];
    std::string value;
    if (!net::GetValueForKeyInQuery(url, key, &value)) {
      continue;
    }
    base::span<std::string_view> unsupported_values =
        base::span(key_values).subspan(1u);
    if (std::ranges::contains(unsupported_values, value)) {
      return true;
    }
  }
  return false;
}

bool ShouldSuppressPrefetchForUnsupportedMode(const AutocompleteMatch& match) {
  if (!base::FeatureList::IsEnabled(
          kSuppressPrefetchForUnsupportedSearchMode)) {
    return false;
  }
  // TODO(crbug.com/479054738): AIM suggestions should not be prefetched for
  // now. Revisit this decision later.
  if (match.IsSearchAimSuggestion()) {
    return true;
  }
  return ShouldSuppressPrefetchForUnsupportedMode(match.destination_url);
}
