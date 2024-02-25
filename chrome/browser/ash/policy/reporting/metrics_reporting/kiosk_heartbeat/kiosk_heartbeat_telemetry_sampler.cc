// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/kiosk_heartbeat/kiosk_heartbeat_telemetry_sampler.h"

#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

void KioskHeartbeatTelemetrySampler::MaybeCollect(
    OptionalMetricCallback callback) {
  MetricData metric_data;
  // using mutable_.. to just add the empty heartbeat_telemetry message.
  metric_data.mutable_telemetry_data()->mutable_heartbeat_telemetry();

  std::move(callback).Run(std::move(metric_data));
}
}  // namespace reporting
