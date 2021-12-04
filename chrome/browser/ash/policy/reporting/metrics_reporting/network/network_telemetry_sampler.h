// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_

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
  Sampler* const https_latency_sampler_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
