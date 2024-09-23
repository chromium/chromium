// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/kiosk_vision/kiosk_vision_telemetry_sampler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

void KioskVisionTelemetrySampler::MaybeCollect(
    OptionalMetricCallback callback) {
  auto* telemetry_processor =
      ash::KioskController::Get().GetKioskVisionTelemetryProcessor();

  if (!telemetry_processor) {
    LOG(WARNING) << "No telemetry processor. Cannot collect telemetry data.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  reporting::MetricData metric_data;
  *metric_data.mutable_telemetry_data() =
      telemetry_processor->GenerateTelemetryData();

  std::move(callback).Run(std::move(metric_data));
}
}  // namespace reporting
