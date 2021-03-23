// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_

#include "components/metrics/metrics_provider.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"

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
  class Impl;
  scoped_refptr<Impl> impl_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
