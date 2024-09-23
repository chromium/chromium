// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
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
  // Observer that can be used to monitor the lifecycle of certain components
  // owned by `AppPlatformMetricsService`.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Triggered once the `AppPlatformMetrics` component is initialized.
    // This enables external components to delay interactions with the
    // component until it is ready.
    virtual void OnAppPlatformMetricsInit(
        AppPlatformMetrics* app_platform_metrics) {}

    // Triggered once the `WebsiteMetrics` component is initialized. This
    // enables external components to delay interactions with the component
    // until it is ready.
    virtual void OnWebsiteMetricsInit(WebsiteMetrics* website_metrics) {}

    // Triggered when the `AppPlatformMetricsService` will be destroyed. This
    // can be used by observer to unregister itself as an observer as well as
    // prevent use-after-free errors.
    virtual void OnAppPlatformMetricsServiceWillBeDestroyed() = 0;
  };

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
             InstanceRegistry& instance_registry,
             apps::AppCapabilityAccessCache& app_capability_access_cache);

  apps::AppPlatformMetrics* AppPlatformMetrics() {
    return app_platform_app_metrics_.get();
  }

  apps::WebsiteMetrics* WebsiteMetrics() { return website_metrics_.get(); }

  // Add observer to the observer list.
  void AddObserver(Observer* observer);

  // Remove observer from the observer list.
  void RemoveObserver(Observer* observer);

  void SetWebsiteMetricsForTesting(
      std::unique_ptr<apps::WebsiteMetrics> website_metrics);

 private:
  friend class AppPlatformInputMetricsTest;

  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  // Helper function to check if 5 mintues have arrived.
  void CheckForFiveMinutes();

  // Helper function to check if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKMs events.
  void CheckForNoisyAppKMReportingInterval();

  const raw_ptr<Profile> profile_;

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

  // List of observers that will be notified of certain component lifecycle
  // changes.
  base::ObserverList<Observer> observers_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_H_
