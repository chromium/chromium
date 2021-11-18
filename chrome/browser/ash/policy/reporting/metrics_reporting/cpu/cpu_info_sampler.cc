// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cpu/cpu_info_sampler.h"

#include "base/logging.h"

namespace reporting {
namespace {
void OnHealthdCpuInfoReceived(
    MetricCallback callback,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
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
        std::move(callback).Run(metric_data);

        break;
      }
    }
  }
}
}  // namespace

CpuInfoSampler::CpuInfoSampler()
    : CpuInfoSampler(base::BindRepeating(CpuInfoSampler::FetchCpuInfo)) {}

CpuInfoSampler::CpuInfoSampler(CpuInfoFetcher cpu_info_fetcher)
    : cpu_info_fetcher_(std::move(cpu_info_fetcher)) {}

CpuInfoSampler::~CpuInfoSampler() = default;

void CpuInfoSampler::Collect(MetricCallback callback) {
  cpu_info_fetcher_.Run(
      base::BindOnce(OnHealthdCpuInfoReceived, std::move(callback)));
}

void CpuInfoSampler::FetchCpuInfo(
    base::OnceCallback<void(cros_healthd::TelemetryInfoPtr)> healthd_callback) {
  chromeos::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<cros_healthd::ProbeCategoryEnum>{
          cros_healthd::ProbeCategoryEnum::kCpu},
      std::move(healthd_callback));
}
}  // namespace reporting
