// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

UsageTelemetryPeriodicCollectorBase::UsageTelemetryPeriodicCollectorBase(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      rate_controller_(std::make_unique<MetricRateController>(
          base::BindRepeating(&UsageTelemetryPeriodicCollectorBase::Collect,
                              base::Unretained(this),
                              /*is_event_driven=*/false),
          reporting_settings,
          rate_setting_path,
          default_rate,
          rate_unit_to_ms)) {
  rate_controller_->Start();
}

UsageTelemetryPeriodicCollectorBase::~UsageTelemetryPeriodicCollectorBase() =
    default;

void UsageTelemetryPeriodicCollectorBase::OnMetricDataCollected(
    bool is_event_driven,
    std::optional<MetricData> metric_data) {
  if (!metric_data.has_value()) {
    // No data to report.
    return;
  }
  metric_data->set_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  metric_report_queue_->Enqueue(std::move(metric_data.value()));
}

bool UsageTelemetryPeriodicCollectorBase::CanCollect() const {
  // Relevant checks and validation is performed during metric collection by
  // corresponding usage observers. Because we normally do not record timestamp
  // with usage telemetry data, we report it right away to prevent staleness.
  return true;
}

}  // namespace reporting
