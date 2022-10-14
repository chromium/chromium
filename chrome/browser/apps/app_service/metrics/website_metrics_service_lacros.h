// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_

#include "base/timer/timer.h"

namespace apps {

// Service to initialize and control website metric recorders per day in the
// Lacros side.
class WebsiteMetricsServiceLacros {
 public:
  WebsiteMetricsServiceLacros();
  WebsiteMetricsServiceLacros(const WebsiteMetricsServiceLacros&) = delete;
  WebsiteMetricsServiceLacros& operator=(const WebsiteMetricsServiceLacros&) =
      delete;
  ~WebsiteMetricsServiceLacros();

  // Start the timer for website metrics.
  void Start();

 private:
  friend class WebsiteMetricsBrowserTest;

  // Helper function to check if 5 mintues have arrived.
  void CheckForFiveMinutes();

  // Helper function to check if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKMs events.
  void CheckForNoisyAppKMReportingInterval();

  // A periodic timer that checks if five minutes have arrived.
  base::RepeatingTimer five_minutes_timer_;

  // A periodic timer that checks if the reporting interval for noisy AppKMs has
  // arrived to report noisy AppKM events.
  base::RepeatingTimer noisy_appkm_reporting_interval_timer_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_SERVICE_LACROS_H_
