// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
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
    // In memory saver mode for the entire upload window
    kMemorySaver = 1,
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
  static constexpr bool ShouldCollectCpuFrequencyMetrics() {
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_WIN)
    return true;
#else
    return false;
#endif
  }

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
  void OnBatterySaverActiveChanged(bool is_active) override;

  void OnMemorySaverPrefChanged();
  void OnTuningModesChanged();
  EfficiencyMode ComputeCurrentMode() const;
  bool IsMemorySaverEnabled() const;

  void RecordAvailableMemoryMetrics();
  void ResetTrackers();

  static void RecordCpuFrequencyMetrics(base::TimeTicks should_run_at);
  static void ScheduleCpuFrequencyTask();
  static void PostCpuFrequencyEstimation();

  struct DiskMetrics {
    int64_t free_bytes;
    int64_t total_bytes;
  };

  class DiskMetricsThreadPoolGetter {
   public:
    DiskMetrics ComputeDiskMetrics(const base::FilePath& user_data_dir);
  };

  void RecordDiskMetrics();
  void PostDiskMetricsTask();
  void SavePendingDiskMetrics(DiskMetrics metrics);

  PrefChangeRegistrar pref_change_registrar_;
  const raw_ptr<PrefService> local_state_;
  EfficiencyMode current_mode_ = EfficiencyMode::kNormal;

  bool battery_saver_enabled_ = false;

  bool initialized_ = false;

  base::SequenceBound<DiskMetricsThreadPoolGetter> disk_metrics_getter_;
  std::optional<DiskMetrics> pending_disk_metrics_;

  base::RepeatingTimer available_memory_metrics_timer_;

  std::unique_ptr<ScopedTimeInModeTracker> battery_saver_mode_tracker_;
  std::unique_ptr<ScopedTimeInModeTracker> memory_saver_mode_tracker_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_METRICS_PROVIDER_DESKTOP_H_
