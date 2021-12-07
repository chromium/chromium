// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/logging.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace cros_healthd = chromeos::cros_healthd::mojom;

namespace reporting {
namespace {
void HandleCpuResult(MetricCallback callback,
                     CrosHealthdMetricSampler::MetricType metric_type,
                     chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  bool anything_reported = false;
  MetricData metric_data;
  auto* keylocker_info_out = metric_data.mutable_info_data()
                                 ->mutable_cpu_info()
                                 ->mutable_keylocker_info();
  const auto& cpu_result = result->cpu_result;

  if (!cpu_result.is_null()) {
    switch (cpu_result->which()) {
      case chromeos::cros_healthd::mojom::CpuResult::Tag::ERROR: {
        DVLOG(1) << "cros_healthd: Error getting CPU info: "
                 << cpu_result->get_error()->msg;
        break;
      }

      case chromeos::cros_healthd::mojom::CpuResult::Tag::CPU_INFO: {
        const auto& cpu_info = cpu_result->get_cpu_info();
        if (cpu_info.is_null()) {
          DVLOG(1) << "Null CpuInfo from cros_healthd";
          break;
        }

        // Gather keylocker info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          auto* keylocker_info = cpu_info->keylocker_info.get();
          if (keylocker_info) {
            keylocker_info_out->set_supported(true);
            keylocker_info_out->set_configured(
                keylocker_info->keylocker_configured);
          } else {
            // If keylocker info isn't set, it is not supported on the board.
            keylocker_info_out->set_supported(false);
            keylocker_info_out->set_configured(false);
          }
          anything_reported = true;
        }
        break;
      }
    }
  }

  if (anything_reported) {
    std::move(callback).Run(metric_data);
  }
}

void OnHealthdInfoReceived(
    MetricCallback callback,
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdMetricSampler::MetricType metric_type,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  if (!result) {
    DVLOG(1) << "cros_healthd: null telemetry result";
    return;
  }

  switch (probe_category) {
    case cros_healthd::ProbeCategoryEnum::kCpu: {
      HandleCpuResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    default: {
      NOTREACHED();
      return;
    }
  }
}
}  // namespace

CrosHealthdMetricSampler::CrosHealthdMetricSampler(
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdMetricSampler::MetricType metric_type)
    : probe_category_(probe_category), metric_type_(metric_type) {}

CrosHealthdMetricSampler::~CrosHealthdMetricSampler() = default;

void CrosHealthdMetricSampler::Collect(MetricCallback callback) {
  auto healthd_callback =
      base::BindOnce(OnHealthdInfoReceived, std::move(callback),
                     probe_category_, metric_type_);
  chromeos::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
      std::move(healthd_callback));
}
}  // namespace reporting
