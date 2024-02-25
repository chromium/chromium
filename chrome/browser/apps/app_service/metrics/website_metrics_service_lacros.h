// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

class Profile;
class PrefRegistrySimple;

namespace apps {

// Service to initialize and control website metric recorders per day in the
// Lacros side.
class WebsiteMetricsServiceLacros {
 public:
  // Observer that can be used to monitor the lifecycle of certain components
  // owned by `WebsiteMetricsServiceLacros`.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Triggered once the `WebsiteMetrics` component is initialized. This
    // enables external components to delay interactions with the component
    // until it is ready.
    virtual void OnWebsiteMetricsInit(WebsiteMetrics* website_metrics) {}

    // Triggered when `WebsiteMetricsServiceLacros` will be destroyed. This can
    // be used by observer to unregister itself as an observer as well as
    // prevent use-after-free errors.
    virtual void OnWebsiteMetricsServiceLacrosWillBeDestroyed() = 0;
  };

  explicit WebsiteMetricsServiceLacros(Profile* profile);
  WebsiteMetricsServiceLacros(const WebsiteMetricsServiceLacros&) = delete;
  WebsiteMetricsServiceLacros& operator=(const WebsiteMetricsServiceLacros&) =
      delete;
  ~WebsiteMetricsServiceLacros();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Asynchronously initialise the device type and then call Start() when done.
  void InitDeviceTypeAndStart();

  // Start the timer for website metrics.
  void Start();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  apps::WebsiteMetrics* WebsiteMetrics() { return website_metrics_.get(); }

  void SetWebsiteMetricsForTesting(
      std::unique_ptr<apps::WebsiteMetrics> website_metrics);

 private:
  friend class WebsiteMetricsBrowserTest;

  // Helper function to check if 5 mintues have arrived.
  void CheckForFiveMinutes();

  // Helper function to check if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKMs events.
  void CheckForNoisyAppKMReportingInterval();

  // Called asynchronously when crosapi returns the device type for metrics.
  void OnGetDeviceTypeForMetrics(int result);

  const raw_ptr<Profile> profile_;

  // A periodic timer that checks if five minutes have arrived.
  base::RepeatingTimer five_minutes_timer_;

  // A periodic timer that checks if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKM events.
  base::RepeatingTimer noisy_appkm_reporting_interval_timer_;

  std::unique_ptr<apps::WebsiteMetrics> website_metrics_;

  // List of observers used to monitor component lifecycle changes.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<WebsiteMetricsServiceLacros> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_
