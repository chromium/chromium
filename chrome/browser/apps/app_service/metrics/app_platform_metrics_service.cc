// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace apps {

namespace {

// Interval for reporting noisy AppKM events.
constexpr base::TimeDelta kNoisyAppKMReportInterval = base::Hours(2);

// Check for a new day every 10 minutes.
constexpr base::TimeDelta kTimerInterval = base::Minutes(10);

// Check for app usage time, input event each 5 minutes.
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

  // Also notify observers.
  for (auto& observer : observers_) {
    observer.OnAppPlatformMetricsServiceWillBeDestroyed();
  }
}

// static
void AppPlatformMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAppPlatformMetricsDayId, 0);
  registry->RegisterDictionaryPref(kAppRunningDuration);
  registry->RegisterDictionaryPref(kAppActivatedCount);
  registry->RegisterDictionaryPref(kAppUsageTime);
  registry->RegisterDictionaryPref(kAppInputEventsKey);
  registry->RegisterDictionaryPref(kWebsiteUsageTime);

  AppDiscoveryMetrics::RegisterProfilePrefs(registry);
}

// static
int AppPlatformMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

void AppPlatformMetricsService::Start(
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry,
    apps::AppCapabilityAccessCache& app_capability_access_cache) {
  app_platform_app_metrics_ = std::make_unique<apps::AppPlatformMetrics>(
      profile_, app_registry_cache, instance_registry);
  app_platform_input_metrics_ = std::make_unique<apps::AppPlatformInputMetrics>(
      profile_, app_registry_cache, instance_registry);
  website_metrics_ = std::make_unique<apps::WebsiteMetrics>(
      profile_, GetUserTypeByDeviceTypeMetrics());
  app_discovery_metrics_ = std::make_unique<apps::AppDiscoveryMetrics>(
      profile_, app_registry_cache, instance_registry,
      app_platform_app_metrics_.get(), app_capability_access_cache);

  day_id_ = profile_->GetPrefs()->GetInteger(kAppPlatformMetricsDayId);
  CheckForNewDay();

  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &AppPlatformMetricsService::CheckForNewDay);

  // Check every `kFiveMinutes` to record app usage time and input events.
  five_minutes_timer_.Start(FROM_HERE, kFiveMinutes, this,
                            &AppPlatformMetricsService::CheckForFiveMinutes);

  // Check every `kNoisyAppKMReportInterval` to report noisy AppKM events.
  noisy_appkm_reporting_interval_timer_.Start(
      FROM_HERE, kNoisyAppKMReportInterval, this,
      &AppPlatformMetricsService::CheckForNoisyAppKMReportingInterval);

  // Also notify observers.
  for (auto& observer : observers_) {
    observer.OnAppPlatformMetricsInit(app_platform_app_metrics_.get());
    observer.OnWebsiteMetricsInit(website_metrics_.get());
  }
}

void AppPlatformMetricsService::AddObserver(
    AppPlatformMetricsService::Observer* observer) {
  observers_.AddObserver(observer);
}

void AppPlatformMetricsService::RemoveObserver(
    AppPlatformMetricsService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppPlatformMetricsService::SetWebsiteMetricsForTesting(
    std::unique_ptr<apps::WebsiteMetrics> website_metrics) {
  website_metrics_ = std::move(website_metrics);
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
  website_metrics_->OnFiveMinutes();
}

void AppPlatformMetricsService::CheckForNoisyAppKMReportingInterval() {
  app_platform_app_metrics_->OnTwoHours();
  app_platform_input_metrics_->OnTwoHours();
  website_metrics_->OnTwoHours();
}

}  // namespace apps
