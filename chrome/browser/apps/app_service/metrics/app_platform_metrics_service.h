// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_

#include <utility>

#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/metrics/app_discovery_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_input_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/profiles/profile.h"

class PrefRegistrySimple;

namespace apps {

class AppRegistryCache;
class InstanceRegistry;

extern const char kAppPlatformMetricsDayId[];

// Service to initialize and control app platform metric recorders per day in
// Chrome OS.
class AppPlatformMetricsService {
 public:
  explicit AppPlatformMetricsService(Profile* profile);
  AppPlatformMetricsService(const AppPlatformMetricsService&) = delete;
  AppPlatformMetricsService& operator=(const AppPlatformMetricsService&) =
      delete;
  ~AppPlatformMetricsService();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the day id for a given time for testing.
  static int GetDayIdForTesting(base::Time time);

  // Start the timer and check if a new day has arrived.
  void Start(AppRegistryCache& app_registry_cache,
             InstanceRegistry& instance_registry);

  apps::AppPlatformMetrics* AppPlatformMetrics() {
    return app_platform_app_metrics_.get();
  }

  void SetWebsiteMetricsForTesting(
      std::unique_ptr<apps::WebsiteMetrics> website_metrics);

 private:
  friend class AppPlatformInputMetricsTest;
  friend class WebsiteMetricsBrowserTest;

  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  // Helper function to check if 5 mintues have arrived.
  void CheckForFiveMinutes();

  // Helper function to check if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKMs events.
  void CheckForNoisyAppKMReportingInterval();

  Profile* const profile_;

  int day_id_;

  // A periodic timer that checks if a new day has arrived.
  base::RepeatingTimer timer_;

  // A periodic timer that checks if five minutes have arrived.
  base::RepeatingTimer five_minutes_timer_;

  // A periodic timer that checks if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKM events.
  base::RepeatingTimer noisy_appkm_reporting_interval_timer_;

  std::unique_ptr<apps::AppPlatformMetrics> app_platform_app_metrics_;
  std::unique_ptr<apps::AppPlatformInputMetrics> app_platform_input_metrics_;
  std::unique_ptr<apps::WebsiteMetrics> website_metrics_;
  std::unique_ptr<apps::AppDiscoveryMetrics> app_discovery_metrics_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_
