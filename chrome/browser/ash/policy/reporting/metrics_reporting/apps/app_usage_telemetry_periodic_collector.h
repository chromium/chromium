// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_

#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// Periodic collector that collects and reports app usage telemetry originally
// collected by the `AppUsageObserver`. This is a standalone component that is
// similar to the `PeriodicCollector` but only controls the collection rate
// based on the respective policy setting. This is to avoid data staleness
// because we do not associate usage data with a timestamp today.
class AppUsageTelemetryPeriodicCollector
    : public UsageTelemetryPeriodicCollectorBase,
      public ::ash::SessionTerminationManager::Observer {
 public:
  AppUsageTelemetryPeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings);
  AppUsageTelemetryPeriodicCollector(
      const AppUsageTelemetryPeriodicCollector& other) = delete;
  AppUsageTelemetryPeriodicCollector& operator=(
      const AppUsageTelemetryPeriodicCollector& other) = delete;
  ~AppUsageTelemetryPeriodicCollector() override;

 private:
  // ::ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_
