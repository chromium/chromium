// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"

#include <string>

#include "base/command_line.h"

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
  return base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "max_attempts_per_caching_duration",
      2);
}

base::TimeDelta SearchPrefetchErrorBackoffDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(kSearchPrefetchServicePrefetching,
                                             "error_backoff_duration_ms",
                                             60000));
}

bool SearchPrefetchOnlyFetchDefaultMatch() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSearchPrefetchServicePrefetching, "only_prefetch_default_match", false);
}
