// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_periodic_collector.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

AppUsageTelemetryPeriodicCollector::AppUsageTelemetryPeriodicCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      rate_controller_(std::make_unique<MetricRateController>(
          base::BindRepeating(&AppUsageTelemetryPeriodicCollector::Collect,
                              base::Unretained(this),
                              /*is_event_driven=*/false),
          reporting_settings,
          ::ash::reporting::kReportAppUsageCollectionRateMs,
          metrics::kDefaultAppUsageTelemetryCollectionRate,
          /*rate_unit_to_ms=*/1)) {
  ::ash::SessionTerminationManager::Get()->AddObserver(this);
  rate_controller_->Start();
}

AppUsageTelemetryPeriodicCollector::~AppUsageTelemetryPeriodicCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `SessionTerminationManager` outlives the collector so we unregister it as
  // an observer on destruction.
  ::ash::SessionTerminationManager::Get()->RemoveObserver(this);
}

void AppUsageTelemetryPeriodicCollector::OnMetricDataCollected(
    bool is_event_driven,
    absl::optional<MetricData> metric_data) {
  if (!metric_data.has_value()) {
    // No data to report.
    return;
  }
  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  metric_report_queue_->Enqueue(std::move(metric_data.value()));
}

bool AppUsageTelemetryPeriodicCollector::CanCollect() const {
  // `AppUsageObserver` performs necessary checks and validation to ensure the
  // app is allowlisted for reporting purposes. Because we do not record the
  // timestamp with this usage telemetry data, we report it right away to
  // prevent staleness.
  return true;
}

void AppUsageTelemetryPeriodicCollector::OnSessionWillBeTerminated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make an attempt to collect any usage data that was recently recorded from
  // the `AppUsageObserver` so we can prevent data staleness should the profile
  // be inaccessible for too long.
  Collect(/*is_event_driven=*/false);
  rate_controller_.reset();
}

}  // namespace reporting
