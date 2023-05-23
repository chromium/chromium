// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_

#include <memory>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

// Periodic collector that collects and reports app usage telemetry originally
// collected by the `AppUsageObserver`. This is a standalone component that is
// similar to the `PeriodicCollector` but only controls the collection rate
// based on the respective policy setting. This is to avoid data staleness
// because we do not associate usage data with a timestamp today.
class AppUsageTelemetryPeriodicCollector
    : public CollectorBase,
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

 protected:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             absl::optional<MetricData> metric_data) override;

  // CollectorBase:
  bool CanCollect() const override;

 private:
  // ::ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // `MetricReportQueue` used for enqueueing data collected by the sampler.
  const raw_ptr<MetricReportQueue> metric_report_queue_;

  // Component used to control collection rate based on the policy setting.
  std::unique_ptr<MetricRateController> rate_controller_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_PERIODIC_COLLECTOR_H_
