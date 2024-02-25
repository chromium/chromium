// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_KIOSK_HEARTBEAT_KIOSK_HEARTBEAT_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_KIOSK_HEARTBEAT_KIOSK_HEARTBEAT_TELEMETRY_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"

namespace reporting {
// Sampler used to create KioskHeartbeat messages to be sent via ERP controlled
// by a KioskHeartbeatTelemetryCollector.
class KioskHeartbeatTelemetrySampler : public Sampler {
 public:
  KioskHeartbeatTelemetrySampler() = default;
  KioskHeartbeatTelemetrySampler(const KioskHeartbeatTelemetrySampler& other) =
      delete;
  KioskHeartbeatTelemetrySampler& operator=(
      const KioskHeartbeatTelemetrySampler& other) = delete;
  ~KioskHeartbeatTelemetrySampler() override = default;

  // Sends KioskHeartbeats whenever called and passes it to the callback.
  void MaybeCollect(OptionalMetricCallback callback) override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_KIOSK_HEARTBEAT_KIOSK_HEARTBEAT_TELEMETRY_SAMPLER_H_
