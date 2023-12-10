// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_boot_performance_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdBootPerformanceSamplerHandler::
    ~CrosHealthdBootPerformanceSamplerHandler() = default;

void CrosHealthdBootPerformanceSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  const std::string kShutdownReasonNotApplicable = "N/A";
  std::optional<MetricData> metric_data;

  const auto& boot_performance_result = result->boot_performance_result;
  if (!boot_performance_result.is_null()) {
    switch (boot_performance_result->which()) {
      case cros_healthd::BootPerformanceResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting Boot Performance info: "
                 << boot_performance_result->get_error()->msg;
        break;
      }

      case cros_healthd::BootPerformanceResult::Tag::kBootPerformanceInfo: {
        const auto& boot_performance_info =
            boot_performance_result->get_boot_performance_info();
        if (boot_performance_info.is_null()) {
          DVLOG(1) << "Null BootPerformanceInfo from cros_healthd";
          break;
        }

        metric_data = std::make_optional<MetricData>();
        auto* const boot_info_out = metric_data->mutable_telemetry_data()
                                        ->mutable_boot_performance_telemetry();
        // Gather boot performance info.
        boot_info_out->set_boot_up_seconds(
            (int64_t)boot_performance_info->boot_up_seconds);
        boot_info_out->set_boot_up_timestamp_seconds(
            (int64_t)boot_performance_info->boot_up_timestamp);
        if (boot_performance_info->shutdown_reason !=
            kShutdownReasonNotApplicable) {
          boot_info_out->set_shutdown_seconds(
              (int64_t)boot_performance_info->shutdown_seconds);
          boot_info_out->set_shutdown_timestamp_seconds(
              (int64_t)boot_performance_info->shutdown_timestamp);
        }
        boot_info_out->set_shutdown_reason(
            boot_performance_info->shutdown_reason);
        break;
      }
    }
  }

  std::move(callback).Run(metric_data);
}

}  // namespace reporting
