// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_UPGRADE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_UPGRADE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// UpgradeMetricsProvider reports the state of detected pending updates in UMA
// reports.
class UpgradeMetricsProvider : public metrics::MetricsProvider {
 public:
  UpgradeMetricsProvider();

  UpgradeMetricsProvider(const UpgradeMetricsProvider&) = delete;
  UpgradeMetricsProvider& operator=(const UpgradeMetricsProvider&) = delete;

  ~UpgradeMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  // CHROME_BROWSER_METRICS_UPGRADE_METRICS_PROVIDER_H_
