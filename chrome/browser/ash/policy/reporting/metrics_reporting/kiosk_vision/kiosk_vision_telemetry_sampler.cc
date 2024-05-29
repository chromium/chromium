// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/kiosk_vision/kiosk_vision_telemetry_sampler.h"

#include <utility>

#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

void KioskVisionTelemetrySampler::MaybeCollect(
    OptionalMetricCallback callback) {
  MetricData metric_data;

  // TODO(b/342360588): Get telemetry data for kiosk vision from Kiosk Vision
  // framework.
  TelemetryData* telemetry_data = metric_data.mutable_telemetry_data();
  telemetry_data->mutable_kiosk_vision_telemetry();
  telemetry_data->mutable_kiosk_vision_status();

  std::move(callback).Run(std::move(metric_data));
}
}  // namespace reporting
