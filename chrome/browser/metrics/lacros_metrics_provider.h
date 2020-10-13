// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

// Provides metrics for the lacros-chrome binary.
// NOTE: The ash-chrome binary uses ChromeOSMetricsProvider.
class LacrosMetricsProvider : public metrics::MetricsProvider {
 public:
  LacrosMetricsProvider();
  LacrosMetricsProvider(const LacrosMetricsProvider&) = delete;
  LacrosMetricsProvider& operator=(const LacrosMetricsProvider&) = delete;
  ~LacrosMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  //  CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_
