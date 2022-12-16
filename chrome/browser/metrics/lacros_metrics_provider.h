// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
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
  void ProvideCurrentSessionUKMData() override;
  void ProvideSystemProfileMetrics(metrics::SystemProfileProto* proto) override;

 private:
  // Called after the hardware class is available.
  void OnGetFullHardwareClass(const std::string& full_hardware_class);

  // This class caches the full_hardware_class as fetching it is asynchronous
  // but it must be provided synchronously in ProvideSystemProfileMetrics.
  std::string full_hardware_class_;

  base::WeakPtrFactory<LacrosMetricsProvider> weak_ptr_factory_;
};

#endif  //  CHROME_BROWSER_METRICS_LACROS_METRICS_PROVIDER_H_
