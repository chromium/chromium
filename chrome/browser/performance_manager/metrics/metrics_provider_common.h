// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_

#include "components/metrics/metrics_provider.h"

namespace performance_manager {

// A metrics provider to add some performance manager related metrics to the UMA
// protos on each upload.
class MetricsProviderCommon : public ::metrics::MetricsProvider {
 public:
  MetricsProviderCommon();
  ~MetricsProviderCommon() override;

  // metrics::MetricsProvider:
  // This is only called from UMA code but is public for testing.
  void ProvideCurrentSessionData(
      ::metrics::ChromeUserMetricsExtension* /*uma_proto*/) override;

 private:
  void RecordA11yFlags();
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_
