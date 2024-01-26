// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_USAGE_TELEMETRY_PERIODIC_COLLECTOR_BASE_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_USAGE_TELEMETRY_PERIODIC_COLLECTOR_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Periodic collector base class implementation that collects and reports usage
// telemetry data originally tracked by the usage observer. Only used by
// standalone components that are meant to be similar to the `PeriodicCollector`
// but only control the collection rate based on the specified policy setting.
// This can also be used to prevent data staleness because usage observers do
// not associate telemetry data with a timestamp today.
class UsageTelemetryPeriodicCollectorBase : public CollectorBase {
 public:
  UsageTelemetryPeriodicCollectorBase(Sampler* sampler,
                                      MetricReportQueue* metric_report_queue,
                                      ReportingSettings* reporting_settings,
                                      const std::string& rate_setting_path,
                                      base::TimeDelta default_rate,
                                      int rate_unit_to_ms = 1);
  UsageTelemetryPeriodicCollectorBase(
      const UsageTelemetryPeriodicCollectorBase& other) = delete;
  UsageTelemetryPeriodicCollectorBase& operator=(
      const UsageTelemetryPeriodicCollectorBase& other) = delete;
  ~UsageTelemetryPeriodicCollectorBase() override;

 private:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             std::optional<MetricData> metric_data) override;

  // CollectorBase:
  bool CanCollect() const override;

  // `MetricReportQueue` used for enqueueing data collected by the sampler.
  const raw_ptr<MetricReportQueue> metric_report_queue_;

  // Component used to control collection rate based on the policy setting.
  const std::unique_ptr<MetricRateController> rate_controller_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_USAGE_TELEMETRY_PERIODIC_COLLECTOR_BASE_H_
