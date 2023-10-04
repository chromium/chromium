// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_periodic_collector.h"

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

AppUsageTelemetryPeriodicCollector::AppUsageTelemetryPeriodicCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings)
    : UsageTelemetryPeriodicCollectorBase(
          sampler,
          metric_report_queue,
          reporting_settings,
          ::ash::reporting::kReportAppUsageCollectionRateMs,
          metrics::kDefaultAppUsageTelemetryCollectionRate) {
  ::ash::SessionTerminationManager::Get()->AddObserver(this);
}

AppUsageTelemetryPeriodicCollector::~AppUsageTelemetryPeriodicCollector() {
  // `SessionTerminationManager` outlives the collector so we unregister it as
  // an observer on destruction.
  ::ash::SessionTerminationManager::Get()->RemoveObserver(this);
}

void AppUsageTelemetryPeriodicCollector::OnSessionWillBeTerminated() {
  // Make an attempt to collect any usage data that was recently recorded from
  // the `AppUsageObserver` so we can prevent data staleness should the profile
  // be inaccessible for too long.
  Collect(/*is_event_driven=*/false);
}

}  // namespace reporting
