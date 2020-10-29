// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"

#include <string>

#include "base/command_line.h"

// Enables the feature completely with a few skipped checks to make local
// testing easier.
constexpr char kSearchPrefetchServiceCommandLineFlag[] =
    "enable-search-prefetch-service";

const base::Feature kSearchPrefetchService{"SearchPrefetchService",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchPrefetchServicePrefetching{
    "SearchPrefetchServicePrefetching", base::FEATURE_DISABLED_BY_DEFAULT};

bool SearchPrefetchServiceIsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kSearchPrefetchServiceCommandLineFlag) ||
         base::FeatureList::IsEnabled(kSearchPrefetchService);
}

bool SearchPrefetchServicePrefetchingIsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kSearchPrefetchServiceCommandLineFlag) ||
         base::FeatureList::IsEnabled(kSearchPrefetchServicePrefetching);
}

base::TimeDelta SearchPrefetchCachingLimit() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(kSearchPrefetchServicePrefetching,
                                             "prefetch_caching_limit_ms",
                                             60000));
}

size_t SearchPrefetchMaxAttemptsPerCachingDuration() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSearchPrefetchServiceCommandLineFlag)) {
    return 100;
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "max_attempts_per_caching_duration",
      2);
}

base::TimeDelta SearchPrefetchErrorBackoffDuration() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSearchPrefetchServiceCommandLineFlag)) {
    return base::TimeDelta::FromSeconds(1);
  }
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(kSearchPrefetchServicePrefetching,
                                             "error_backoff_duration_ms",
                                             60000));
}

bool SearchPrefetchOnlyFetchDefaultMatch() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSearchPrefetchServicePrefetching, "only_prefetch_default_match", false);
}

bool SearchPrefetchShouldCancelUneededInflightRequests() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSearchPrefetchServicePrefetching, "cancel_inflight_unneeded", true);
}
