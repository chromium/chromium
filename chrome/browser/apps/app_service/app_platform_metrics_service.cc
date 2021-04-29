// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics_service.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace apps {

namespace {

constexpr base::TimeDelta kTimerInterval = base::TimeDelta::FromMinutes(10);

// Returns the number of days since the origin.
int GetDayId(base::Time time) {
  return time.LocalMidnight().since_origin().InDaysFloored();
}

}  // namespace

const char kAppPlatformMetricsDayId[] = "app_platform_metrics.day_id";

AppPlatformMetricsService::AppPlatformMetricsService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

AppPlatformMetricsService::~AppPlatformMetricsService() {
  timer_.Stop();
}

// static
void AppPlatformMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAppPlatformMetricsDayId, 0);
}

// static
int AppPlatformMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

void AppPlatformMetricsService::Start(
    apps::AppRegistryCache& app_registry_cache) {
  app_platform_app_metrics_ =
      std::make_unique<AppPlatformMetrics>(profile_, app_registry_cache);

  day_id_ = profile_->GetPrefs()->GetInteger(kAppPlatformMetricsDayId);
  CheckForNewDay();

  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &AppPlatformMetricsService::CheckForNewDay);
}

void AppPlatformMetricsService::CheckForNewDay() {
  base::Time now = base::Time::Now();

  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id_ < GetDayId(now)) {
    day_id_ = GetDayId(now);
    app_platform_app_metrics_->OnNewDay();
    profile_->GetPrefs()->SetInteger(kAppPlatformMetricsDayId, day_id_);
  }
}

}  // namespace apps
