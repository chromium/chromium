// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_cpu_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdCpuSamplerHandler::~CrosHealthdCpuSamplerHandler() = default;

void CrosHealthdCpuSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& cpu_result = result->cpu_result;

  if (!cpu_result.is_null()) {
    switch (cpu_result->which()) {
      case cros_healthd::CpuResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting CPU info: "
                 << cpu_result->get_error()->msg;
        break;
      }

      case cros_healthd::CpuResult::Tag::kCpuInfo: {
        const auto& cpu_info = cpu_result->get_cpu_info();
        if (cpu_info.is_null()) {
          DVLOG(1) << "Null CpuInfo from cros_healthd";
          break;
        }

        // Gather keylocker info.
        metric_data = std::make_optional<MetricData>();
        auto* const keylocker_info_out = metric_data->mutable_info_data()
                                             ->mutable_cpu_info()
                                             ->mutable_keylocker_info();
        const auto* const keylocker_info = cpu_info->keylocker_info.get();
        if (keylocker_info) {
          keylocker_info_out->set_supported(true);
          keylocker_info_out->set_configured(
              keylocker_info->keylocker_configured);
        } else {
          // If keylocker info isn't set, it is not supported on the board.
          keylocker_info_out->set_supported(false);
          keylocker_info_out->set_configured(false);
        }

        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting
