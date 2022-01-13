// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// NetworkTelemetrySampler collects the telemetry that describes the networks
// and connections states.
class NetworkTelemetrySampler : public Sampler {
 public:
  explicit NetworkTelemetrySampler(Sampler* https_latency_sampler);

  NetworkTelemetrySampler(const NetworkTelemetrySampler&) = delete;
  NetworkTelemetrySampler& operator=(const NetworkTelemetrySampler&) = delete;

  ~NetworkTelemetrySampler() override;

  void Collect(MetricCallback callback) override;

 private:
  void HandleNetworkTelemetryResult(
      MetricCallback callback,
      ::chromeos::cros_healthd::mojom::TelemetryInfoPtr result);

  Sampler* const https_latency_sampler_;

  base::WeakPtrFactory<NetworkTelemetrySampler> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
