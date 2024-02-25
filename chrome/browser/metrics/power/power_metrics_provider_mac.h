// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_

#include <optional>

#include "base/threading/sequence_bound.h"
#include "components/metrics/metrics_provider.h"

class PowerMetricsProvider : public metrics::MetricsProvider {
 public:
  PowerMetricsProvider();
  ~PowerMetricsProvider() override;

  PowerMetricsProvider(const PowerMetricsProvider& other) = delete;
  PowerMetricsProvider& operator=(const PowerMetricsProvider& other) = delete;

  // metrics::MetricsProvider overrides
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

 private:
  // Records metrics from the ThreadPool.
  class Impl;
  std::optional<base::SequenceBound<Impl>> impl_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
