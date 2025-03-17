// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_MAC_H_

#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_provider.h"

// GoogleUpdateMetricsProviderMac is responsible for filling out the
// GoogleUpdate of the UMA SystemProfileProto, and emits the browser's hashed
// cohort ID to the GoogleUpdate.InstallDetails.UpdateCohortId histogram.
class GoogleUpdateMetricsProviderMac : public metrics::MetricsProvider {
 public:
  GoogleUpdateMetricsProviderMac();
  GoogleUpdateMetricsProviderMac(const GoogleUpdateMetricsProviderMac&) =
      delete;
  GoogleUpdateMetricsProviderMac& operator=(
      const GoogleUpdateMetricsProviderMac&) = delete;
  ~GoogleUpdateMetricsProviderMac() override;

  // metrics::MetricsProvider overrides
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  base::WeakPtrFactory<GoogleUpdateMetricsProviderMac> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_MAC_H_
