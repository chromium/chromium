// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_INFO_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_INFO_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"

namespace reporting {

// Sampler to collect networks info.
class NetworkInfoSampler : public Sampler {
 public:
  NetworkInfoSampler() = default;

  NetworkInfoSampler(const NetworkInfoSampler&) = delete;
  NetworkInfoSampler& operator=(const NetworkInfoSampler&) = delete;

  ~NetworkInfoSampler() override = default;

  void MaybeCollect(OptionalMetricCallback callback) override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_INFO_SAMPLER_H_
