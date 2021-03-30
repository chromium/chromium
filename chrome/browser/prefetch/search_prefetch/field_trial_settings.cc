// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"

#include <string>

#include "base/command_line.h"
#include "base/system/sys_info.h"

// Enables the feature completely with a few skipped checks to make local
// testing easier.
const char kSearchPrefetchServiceCommandLineFlag[] =
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSearchPrefetchServiceCommandLineFlag)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(kSearchPrefetchServicePrefetching)) {
    return false;
  }

  return base::SysInfo::AmountOfPhysicalMemoryMB() >
         base::GetFieldTrialParamByFeatureAsInt(
             kSearchPrefetchServicePrefetching, "device_memory_threshold_MB",
             3000);
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

bool StreamSearchPrefetchResponses() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSearchPrefetchServicePrefetching, "stream_responses", true);
}

size_t SearchPrefetchMaxCacheEntries() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kSearchPrefetchServicePrefetching, "cache_size", 10);
}
