// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_psr_sampler_handler.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EnterpriseReportingPsrResult {
  kOk = 0,
  kErrorGettingPsr = 1,
  kUnknownSystemResultType = 2,
  kNullPsrInfo = 3,
  kPsrUnsupported = 4,
  kMaxValue = kPsrUnsupported
};

// Records PSR result to UMA.
void RecordPsrResult(EnterpriseReportingPsrResult result) {
  base::UmaHistogramEnumeration("Browser.ERP.PsrResult", result);
}

}  // namespace

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdPsrSamplerHandler::~CrosHealthdPsrSamplerHandler() = default;

void CrosHealthdPsrSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  absl::optional<MetricData> metric_data;
  base::ScopedClosureRunner run_callback_on_return(base::BindOnce(
      [](OptionalMetricCallback callback,
         absl::optional<MetricData>* metric_data) {
        std::move(callback).Run(std::move(*metric_data));
      },
      std::move(callback), &metric_data));

  const auto& system_result = result->system_result;
  if (system_result.is_null()) {
    return;
  }

  if (system_result->which() != cros_healthd::SystemResult::Tag::kSystemInfo) {
    switch (system_result->which()) {
      case cros_healthd::SystemResult::Tag::kError:
        RecordPsrResult(EnterpriseReportingPsrResult::kErrorGettingPsr);
        LOG(ERROR) << "cros_healthd: Error getting PSR info: "
                   << system_result->get_error()->msg;
        return;
      default:
        RecordPsrResult(EnterpriseReportingPsrResult::kUnknownSystemResultType);
        LOG(ERROR) << "cros_healthd: Unknown system result type: "
                   << base::to_underlying(system_result->which());
        return;
    }
  }

  const auto& psr_info = system_result->get_system_info()->psr_info;
  if (psr_info.is_null()) {
    RecordPsrResult(EnterpriseReportingPsrResult::kNullPsrInfo);
    LOG(ERROR) << "Null PsrInfo from cros_healthd";
    return;
  }

  if (!psr_info->is_supported) {
    RecordPsrResult(EnterpriseReportingPsrResult::kPsrUnsupported);
    return;
  }

  // Gather PSR info.
  metric_data = absl::make_optional<MetricData>();
  auto* const runtime_counters_telemetry =
      metric_data->mutable_telemetry_data()
          ->mutable_runtime_counters_telemetry();

  runtime_counters_telemetry->set_uptime_runtime_seconds(
      psr_info->uptime_seconds);
  runtime_counters_telemetry->set_counter_enter_sleep(psr_info->s3_counter);
  runtime_counters_telemetry->set_counter_enter_hibernation(
      psr_info->s4_counter);
  runtime_counters_telemetry->set_counter_enter_poweroff(psr_info->s5_counter);
  RecordPsrResult(EnterpriseReportingPsrResult::kOk);
}
}  // namespace reporting
