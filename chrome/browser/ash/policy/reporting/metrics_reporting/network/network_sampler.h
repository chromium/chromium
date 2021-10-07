// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"

namespace reporting {

// NetworkSampler collects the telemetry that describes the networks and
// connections states. Currently, information is collected by
// `DeviceStatusCollector`, but can be moved here gradually by implementing
// `Sampler::CollectInfo`.
class NetworkSampler : public Sampler {
 public:
  explicit NetworkSampler(Sampler* https_latency_sampler);
  ~NetworkSampler() override;

  void CollectTelemetry(TelemetryCallback callback) override;

 private:
  Sampler* const https_latency_sampler_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_SAMPLER_H_
