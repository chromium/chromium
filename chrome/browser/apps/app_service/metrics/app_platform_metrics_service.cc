// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace apps {

namespace {

constexpr base::TimeDelta kTimerInterval = base::Minutes(10);
constexpr base::TimeDelta kFiveMinutes = base::Minutes(5);

// Returns the number of days since the origin.
int GetDayId(base::Time time) {
  return time.UTCMidnight().since_origin().InDaysFloored();
}

}  // namespace

constexpr char kAppPlatformMetricsDayId[] = "app_platform_metrics.day_id";

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
  registry->RegisterDictionaryPref(kAppRunningDuration);
  registry->RegisterDictionaryPref(kAppActivatedCount);
}

// static
int AppPlatformMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

void AppPlatformMetricsService::Start(
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry) {
  app_platform_app_metrics_ = std::make_unique<apps::AppPlatformMetrics>(
      profile_, app_registry_cache, instance_registry);
  app_platform_input_metrics_ = std::make_unique<apps::AppPlatformInputMetrics>(
      profile_, app_registry_cache, instance_registry);

  day_id_ = profile_->GetPrefs()->GetInteger(kAppPlatformMetricsDayId);
  CheckForNewDay();

  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &AppPlatformMetricsService::CheckForNewDay);

  // Check every |kFiveMinutes|.
  five_minutes_timer_.Start(FROM_HERE, kFiveMinutes, this,
                            &AppPlatformMetricsService::CheckForFiveMinutes);
}

void AppPlatformMetricsService::CheckForNewDay() {
  base::Time now = base::Time::Now();

  DCHECK(app_platform_app_metrics_);
  app_platform_app_metrics_->OnTenMinutes();

  if (day_id_ < GetDayId(now)) {
    day_id_ = GetDayId(now);
    app_platform_app_metrics_->OnNewDay();
    profile_->GetPrefs()->SetInteger(kAppPlatformMetricsDayId, day_id_);
  }
}

void AppPlatformMetricsService::CheckForFiveMinutes() {
  app_platform_app_metrics_->OnFiveMinutes();
  app_platform_input_metrics_->OnFiveMinutes();
}

}  // namespace apps
