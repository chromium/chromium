// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_

#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"

#if BUILDFLAG(IS_MAC)
#include <optional>

#include "base/process/process_metrics.h"
#include "base/time/time.h"
#endif

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
  void RecordAvailableMemoryMetrics();
  void RecordA11yFlags();

  base::RepeatingTimer available_memory_metrics_timer_;

#if BUILDFLAG(IS_MAC)
  struct MacMemoryChurnSample {
    base::SystemMemoryInfo memory_info;
    base::TimeTicks sample_ticks;
  };
  std::optional<MacMemoryChurnSample> prev_mac_churn_sample_;
#endif
};

#if BUILDFLAG(IS_MAC)
// Exposed for testing.
void RecordMacSystemMemoryChurnMetrics(const base::SystemMemoryInfo& prev,
                                       const base::SystemMemoryInfo& cur,
                                       base::TimeDelta elapsed);
#endif

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_COMMON_H_
