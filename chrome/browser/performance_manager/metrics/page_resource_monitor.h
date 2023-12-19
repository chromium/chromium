// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_RESOURCE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_RESOURCE_MONITOR_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/metrics/page_resource_cpu_monitor.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/system_cpu/pressure_sample.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace system_cpu {
class CpuProbe;
}

namespace performance_manager::metrics {

class PageResourceMonitorUnitTest;

// Periodically reports tab resource usage via UKM.
class PageResourceMonitor : public GraphOwned {
 public:
  // These values are logged to UKM. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // PageMeasurementBackgroundState in enums.xml.
  enum class PageMeasurementBackgroundState {
    kForeground = 0,
    kBackground = 1,
    kAudibleInBackground = 2,
    kBackgroundMixedAudible = 3,
    kMixedForegroundBackground = 4,
    kMaxValue = kMixedForegroundBackground,
  };

  // If `enable_system_cpu_probe` is false, `system_cpu_probe_` will be left
  // null. This is mainly intended for tests.
  explicit PageResourceMonitor(bool enable_system_cpu_probe = true);

  ~PageResourceMonitor() override;
  PageResourceMonitor(const PageResourceMonitor& other) = delete;
  PageResourceMonitor& operator=(const PageResourceMonitor&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  friend PageResourceMonitorUnitTest;

  // Suffix for CPU intervention histograms.
  enum class CPUInterventionSuffix {
    kBaseline,
    kImmediate,
    kDelayed,
  };

  // The percent CPU usage for each PageNode that was measured. This stores a
  // PageContext instead of a node pointer in case the PageNode is deleted while
  // taking asynchronous system CPU measurements.
  using PageCPUUsageVector =
      std::vector<std::pair<resource_attribution::PageContext, double>>;

  // Asynchronously collects the PageResourceUsage UKM. Calls `done_closure`
  // when finished.
  void CollectPageResourceUsage(base::OnceClosure done_closure);

  // Invoked asynchronously from CollectPageResourceUsage() when measurements
  // are ready.
  void OnPageResourceUsageResult(
      const PageCPUUsageVector& page_cpu_usage,
      absl::optional<system_cpu::PressureSample> system_cpu);

  // Asynchronously checks if the CPU metrics are still above the threshold
  // after a delay.
  void CheckDelayedCPUInterventionMetrics();

  // Invoked asynchronously from CheckDelayedCPUInterventionMetrics() when
  // measurements are ready.
  void OnDelayedCPUInterventionMetricsResult(
      const PageCPUUsageVector& page_cpu_usage,
      absl::optional<system_cpu::PressureSample> system_cpu);

  // Log CPU intervention metrics with the provided suffix.
  void LogCPUInterventionMetrics(
      const PageCPUUsageVector& page_cpu_usage,
      const absl::optional<system_cpu::PressureSample>& system_cpu,
      const base::TimeTicks now,
      CPUInterventionSuffix histogram_suffix);

  // Asynchronously calculates per-PageNode CPU usage, converts the results to a
  // vector, and passes them to `callback`. Also queries either
  // `system_cpu_probe_` or `delayed_system_cpu_probe_`, depending on the value
  // of `use_delayed_system_cpu_probe`, for a PressureSample to pass to
  // `callback`.
  void CalculatePageCPUUsage(
      bool use_delayed_system_cpu_probe,
      base::OnceCallback<void(const PageCPUUsageVector&,
                              absl::optional<system_cpu::PressureSample>)>
          callback);

  // Invoked asynchronously from CalculatePageCPUUsage() when page CPU
  // measurements are ready. Converts the measurements in `cpu_usage_map`
  // to a vector, collects system CPU from either `system_cpu_probe_` or
  // `delayed_system_cpu_probe_` (depending on the value of
  // `use_delayed_system_cpu_probe`) and passes both page and system results to
  // `callback`.
  void OnPageCPUUsageResult(
      bool use_delayed_system_cpu_probe,
      base::OnceCallback<void(const PageCPUUsageVector&,
                              absl::optional<system_cpu::PressureSample>)>
          callback,
      const PageResourceCPUMonitor::CPUUsageMap& cpu_usage_map);

  // If this is called, CollectPageResourceUsage() will not be called on a
  // timer. Tests can call it manually.
  void SetTriggerCollectionManuallyForTesting();

  // Passes the given `factory` to PageResourceCPUMonitor.
  void SetCPUMeasurementDelegateFactoryForTesting(
      Graph* graph,
      PageResourceCPUMonitor::CPUMeasurementDelegate::Factory* factory);

  SEQUENCE_CHECKER(sequence_checker_);

  // Timer which is used to trigger CollectPageResourceUsage().
  base::RepeatingTimer collect_page_resource_usage_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer which handles logging high CPU after a potential delay.
  base::OneShotTimer log_cpu_on_delay_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of whether the browser has exceeded the CPU threshold.
  absl::optional<base::TimeTicks> time_of_last_cpu_threshold_exceeded_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;

  // Pointer to this process' graph.
  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Time of last PageResourceUsage collection.
  base::TimeTicks time_of_last_resource_usage_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::TimeTicks::Now();

  // Helper to take CPU measurements for the UKM.
  PageResourceCPUMonitor cpu_monitor_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Helpers to take system CPU measurements for UMA.
  std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<system_cpu::CpuProbe> delayed_system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // WeakPtrFactory for the RepeatingTimer to call a method on this object.
  base::WeakPtrFactory<PageResourceMonitor> weak_factory_{this};
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_RESOURCE_MONITOR_H_
