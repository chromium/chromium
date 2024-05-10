// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_psr_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EnterpriseReportingPsrResult {
  kOk = 0,
  kErrorGettingPsr = 1,
  kUnknownSystemResultType = 2,
  kNullPsrInfo = 3,
  kPsrUnsupported = 4,
  kPsrNotStarted = 5,
  kPsrStopped = 6,
  kUnknownPsrLogState = 7,
  kMaxValue = kUnknownPsrLogState
};

constexpr size_t kMaxRetries = 1u;

// Records PSR result to UMA.
void RecordPsrResult(EnterpriseReportingPsrResult result) {
  base::UmaHistogramEnumeration("Browser.ERP.PsrResult", result);
}

}  // namespace

CrosHealthdPsrSamplerHandler::CrosHealthdPsrSamplerHandler() = default;
CrosHealthdPsrSamplerHandler::~CrosHealthdPsrSamplerHandler() = default;

void CrosHealthdPsrSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleResultImpl(std::move(callback), /*num_retries_left=*/kMaxRetries,
                   std::move(result));
}

void CrosHealthdPsrSamplerHandler::Retry(OptionalMetricCallback callback,
                                         size_t num_retries_left) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto healthd_callback = base::BindOnce(
      &CrosHealthdPsrSamplerHandler::HandleResultImpl,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), num_retries_left);
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          std::vector<cros_healthd::ProbeCategoryEnum>{
              cros_healthd::ProbeCategoryEnum::kSystem},
          std::move(healthd_callback));
}

void CrosHealthdPsrSamplerHandler::HandleResultImpl(
    OptionalMetricCallback callback,
    size_t num_retries_left,
    cros_healthd::TelemetryInfoPtr result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<MetricData> metric_data;
  absl::Cleanup run_callback_on_return = [this, &callback, &metric_data,
                                          num_retries_left] {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!metric_data.has_value() && num_retries_left > 0) {
      // Failed to obtain PSR info, try again in 10 seconds. May be due to
      // some race condition in healthd when reading PSR devices.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&CrosHealthdPsrSamplerHandler::Retry,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         num_retries_left - 1),
          wait_time_);
      return;
    }
    std::move(callback).Run(std::move(metric_data));
  };

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

  if (psr_info->log_state != cros_healthd::PsrInfo::LogState::kStarted) {
    // Meaning log state is either kNotStarted (PSR was not started by OEM) or
    // kStopped (PSR log has been tampered and permanently stopped.). Don't
    // report anything as PSR would (no longer) run on this device.
    switch (psr_info->log_state) {
      case cros_healthd::PsrInfo::LogState::kNotStarted:
        RecordPsrResult(EnterpriseReportingPsrResult::kPsrNotStarted);
        break;
      case cros_healthd::PsrInfo::LogState::kStopped:
        RecordPsrResult(EnterpriseReportingPsrResult::kPsrStopped);
        break;
      default:
        LOG(ERROR) << "Unknown log state!";
        RecordPsrResult(EnterpriseReportingPsrResult::kUnknownPsrLogState);
    }
    return;
  }

  // Gather PSR info.
  metric_data = std::make_optional<MetricData>();
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
