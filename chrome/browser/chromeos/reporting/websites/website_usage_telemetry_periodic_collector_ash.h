// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_ASH_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_ASH_H_

#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// Periodic collector that collects and reports website usage telemetry
// originally collected by the `WebsiteUsageObserver`. This is a standalone
// component in Ash that is similar to the `PeriodicCollector` but only controls
// the collection rate based on respective policy setting. This is to prevent
// data staleness because the observer does not associate usage data with a
// timestamp today.
class WebsiteUsageTelemetryPeriodicCollectorAsh
    : public UsageTelemetryPeriodicCollectorBase,
      public ::ash::SessionTerminationManager::Observer {
 public:
  WebsiteUsageTelemetryPeriodicCollectorAsh(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings);
  WebsiteUsageTelemetryPeriodicCollectorAsh(
      const WebsiteUsageTelemetryPeriodicCollectorAsh& other) = delete;
  WebsiteUsageTelemetryPeriodicCollectorAsh& operator=(
      const WebsiteUsageTelemetryPeriodicCollectorAsh& other) = delete;
  ~WebsiteUsageTelemetryPeriodicCollectorAsh() override;

 private:
  // ::ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_ASH_H_
