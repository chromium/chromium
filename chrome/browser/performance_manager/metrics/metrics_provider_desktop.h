// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/prefs/pref_change_registrar.h"

class ChromeMetricsServiceClient;
class PerformanceManagerMetricsProviderDesktopTest;
class PrefService;

namespace performance_manager {

class ScopedTimeInModeTracker;

// A metrics provider to add some performance manager related metrics to the UMA
// protos on each upload, such as related to memory saver, battery saver, and
// available physical memory. Only present on desktop platforms.
class MetricsProviderDesktop : public ::metrics::MetricsProvider,
                               public performance_manager::user_tuning::
                                   BatterySaverModeManager::Observer {
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

  static MetricsProviderDesktop* GetInstance();

  ~MetricsProviderDesktop() override;

  void Initialize();

  // metrics::MetricsProvider:
  // This is only called from UMA code but is public for testing.
  void ProvideCurrentSessionData(
      ::metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  friend class ::ChromeMetricsServiceClient;
  friend class ::PerformanceManagerMetricsProviderDesktopTest;

  explicit MetricsProviderDesktop(PrefService* local_state);

  // BatterySaverModeManager::Observer:
  void OnBatterySaverModeChanged(bool is_active) override;

  void OnHighEfficiencyPrefChanged();
  void OnTuningModesChanged();
  EfficiencyMode ComputeCurrentMode() const;
  bool IsHighEfficiencyEnabled() const;

  void RecordAvailableMemoryMetrics();
  void ResetTrackers();

  PrefChangeRegistrar pref_change_registrar_;
  const raw_ptr<PrefService> local_state_;
  EfficiencyMode current_mode_ = EfficiencyMode::kNormal;

  bool battery_saver_enabled_ = false;

  bool initialized_ = false;

  base::RepeatingTimer available_memory_metrics_timer_;

  std::unique_ptr<ScopedTimeInModeTracker> battery_saver_mode_tracker_;
  std::unique_ptr<ScopedTimeInModeTracker> high_efficiency_mode_tracker_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_
