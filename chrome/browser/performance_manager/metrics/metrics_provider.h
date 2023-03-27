// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/prefs/pref_change_registrar.h"

class ChromeMetricsServiceClient;
class PerformanceManagerMetricsProviderTest;
class PrefService;

namespace performance_manager {

// A metrics provider to add some performance manager related metrics to the UMA
// protos on each upload.
class MetricsProvider : public ::metrics::MetricsProvider,
                        public performance_manager::user_tuning::
                            UserPerformanceTuningManager::Observer {
 public:
  enum class EfficiencyMode {
    // No efficiency mode for the entire upload window
    kNormal = 0,
    // In high efficiency mode for the entire upload window
    kHighEfficiency = 1,
    // In battery saver mode for the entire upload window
    kBatterySaver = 2,
    // Both modes enabled for the entire upload window
    kBoth = 3,
    // The modes were changed during the upload window
    kMixed = 4,
    // Max value, used in UMA histograms macros
    kMaxValue = kMixed
  };

  static MetricsProvider* GetInstance();

  ~MetricsProvider() override;

  void Initialize();

  // metrics::MetricsProvider:
  // This is only called from UMA code but is public for testing.
  void ProvideCurrentSessionData(
      ::metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  friend class ::ChromeMetricsServiceClient;
  friend class ::PerformanceManagerMetricsProviderTest;

  explicit MetricsProvider(PrefService* local_state);

  // UserPerformanceTuningManager::Observer:
  void OnBatterySaverModeChanged(bool is_active) override;

  void OnTuningModesChanged();
  EfficiencyMode ComputeCurrentMode() const;

  void RecordAvailableMemoryMetrics();

  PrefChangeRegistrar pref_change_registrar_;
  const raw_ptr<PrefService> local_state_;
  EfficiencyMode current_mode_ = EfficiencyMode::kNormal;

  bool battery_saver_enabled_ = false;

  bool initialized_ = false;

  base::RepeatingTimer available_memory_metrics_timer_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_H_
