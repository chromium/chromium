// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include <optional>

#include "base/logging.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdMetricSampler::CrosHealthdMetricSampler(
    std::unique_ptr<CrosHealthdSamplerHandler> handler,
    ash::cros_healthd::mojom::ProbeCategoryEnum probe_category)
    : handler_(std::move(handler)), probe_category_(probe_category) {}

CrosHealthdMetricSampler::~CrosHealthdMetricSampler() = default;

void CrosHealthdMetricSampler::OnHealthdInfoReceived(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) {
  handler_->HandleResult(std::move(callback), std::move(result));
}

void CrosHealthdMetricSampler::MaybeCollect(OptionalMetricCallback callback) {
  auto handler_callback =
      base::BindOnce(&CrosHealthdMetricSampler::OnHealthdInfoReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
          std::move(handler_callback));
}

}  // namespace reporting
