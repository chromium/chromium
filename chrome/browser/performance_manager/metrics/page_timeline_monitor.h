// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_sample.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_cpu_monitor.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::metrics {

class CpuProbe;
class PageTimelineMonitorUnitTest;

// Periodically reports tab state via UKM, to enable analysis of usage patterns
// over time.
class PageTimelineMonitor : public PageNode::ObserverDefaultImpl,
                            public GraphOwned,
                            public GraphRegisteredImpl<PageTimelineMonitor>,
                            public TabPageObserver {
 public:
  // These values are logged to UKM. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with PageState
  // in enums.xml.
  enum class PageState {
    kFocused = 0,
    kVisible = 1,
    kBackground = 2,
    kThrottled = 3,
    kFrozen = 4,
    kDiscarded = 5,
    kMaxValue = kDiscarded,
  };

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
  explicit PageTimelineMonitor(bool enable_system_cpu_probe = true);

  ~PageTimelineMonitor() override;
  PageTimelineMonitor(const PageTimelineMonitor& other) = delete;
  PageTimelineMonitor& operator=(const PageTimelineMonitor&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // TabPageObserver:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // PageNode::Observer:
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnPageLifecycleStateChanged(const PageNode* page_node) override;

  void SetBatterySaverEnabled(bool enabled);

 private:
  friend class PageTimelineMonitorBrowserTest;
  friend PageTimelineMonitorUnitTest;
  FRIEND_TEST_ALL_PREFIXES(
      PageTimelineMonitorUnitTest,
      TestPageTimelineDoesntRecordIfShouldCollectSliceReturnsFalse);
  FRIEND_TEST_ALL_PREFIXES(PageTimelineMonitorUnitTest,
                           TestUpdateFaviconInBackground);
  FRIEND_TEST_ALL_PREFIXES(PageTimelineMonitorUnitTest,
                           TestUpdateTitleInBackground);
  FRIEND_TEST_ALL_PREFIXES(PageTimelineMonitorUnitTest,
                           TestUpdateLifecycleState);
  FRIEND_TEST_ALL_PREFIXES(PageTimelineMonitorUnitTest,
                           TestUpdatePageNodeBeforeTypeChange);

  struct PageNodeInfo {
    base::TimeTicks time_of_creation;
    bool currently_visible;
    PageNode::LifecycleState current_lifecycle;
    base::TimeTicks time_of_most_recent_state_change;
    base::TimeTicks time_of_last_foreground_millisecond_update;
    int total_foreground_milliseconds{0};
    int tab_id;

    PageTimelineMonitor::PageState GetPageState();

    explicit PageNodeInfo(base::TimeTicks time_of_creation,
                          const PageNode* page_node,
                          int tab_id)
        : time_of_creation(time_of_creation),
          currently_visible(page_node->IsVisible()),
          current_lifecycle(page_node->GetLifecycleState()),
          time_of_most_recent_state_change(base::TimeTicks::Now()),
          time_of_last_foreground_millisecond_update(
              time_of_most_recent_state_change),
          tab_id(tab_id) {}
    ~PageNodeInfo() = default;
  };

  // Suffix for CPU intervention histograms.
  enum class CPUInterventionSuffix {
    kBaseline,
    kImmediate,
    kDelayed,
  };

  using PageCPUUsageVector = std::vector<std::pair<const PageNode*, double>>;

  using PageNodeInfoMap = std::map<const TabPageDecorator::TabHandle*,
                                   std::unique_ptr<PageNodeInfo>>;

  // Asynchronously collects the PageResourceUsage UKM. Calls `done_closure`
  // when finished.
  void CollectPageResourceUsage(base::OnceClosure done_closure);

  // Invoked asynchronously from CollectPageResourceUsage() when measurements
  // are ready.
  void OnPageResourceUsageResult(const PageCPUUsageVector& page_cpu_usage,
                                 absl::optional<PressureSample> system_cpu);

  // Method collecting a slice for the PageTimelineState UKM.
  void CollectSlice();

  bool ShouldCollectSlice() const;

  // Asynchronously checks if the CPU metrics are still above the threshold
  // after a delay.
  void CheckDelayedCPUInterventionMetrics();

  // Invoked asynchronously from CheckDelayedCPUInterventionMetrics() when
  // measurements are ready.
  void OnDelayedCPUInterventionMetricsResult(
      const PageCPUUsageVector& page_cpu_usage,
      absl::optional<PressureSample> system_cpu);

  // Log CPU intervention metrics with the provided suffix.
  void LogCPUInterventionMetrics(
      const PageCPUUsageVector& page_cpu_usage,
      const absl::optional<PressureSample>& system_cpu,
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
                              absl::optional<PressureSample>)> callback);

  // Invoked asynchronously from CalculatePageCPUUsage() when system CPU
  // measurements are available. Invokes `cpu_monitor_` to get page CPU
  // measurements.
  void OnSystemCPUUsageResult(
      base::OnceCallback<void(const PageCPUUsageVector&,
                              absl::optional<PressureSample>)> callback,
      absl::optional<PressureSample> system_cpu);

  // Invoked asynchronously from OnSystemCPUUsageResult() when page CPU
  // measurements are also ready. Converts the measurements in `cpu_usage_map`
  // to a vector and passes both page and system results to `callback`.
  void OnPageCPUUsageResult(
      base::OnceCallback<void(const PageCPUUsageVector&,
                              absl::optional<PressureSample>)> callback,
      absl::optional<PressureSample> system_cpu,
      const PageTimelineCPUMonitor::CPUUsageMap& cpu_usage_map);

  // If this is called, CollectSlice() and CollectPageResourceUsage() will not
  // be called on a timer. Tests can call them manually.
  void SetTriggerCollectionManuallyForTesting();

  // If this is called, the given callback will be called instead of
  // ShouldCollectSlice().
  void SetShouldCollectSliceCallbackForTesting(base::RepeatingCallback<bool()>);

  // Passes the given `factory` to PageTimelineCPUMonitor.
  void SetCPUMeasurementDelegateFactoryForTesting(
      Graph* graph,
      PageTimelineCPUMonitor::CPUMeasurementDelegate::Factory* factory);

  // Lets tests examine the contents of `page_node_info_map_`.
  PageNodeInfoMap& GetPageNodeInfoForTesting();

  // CHECK's that `page_node` and `info` are in the right state to be
  // mapped to each other in `page_node_info_map_`.
  void CheckPageState(const PageNode* page_node, const PageNodeInfo& info);

  SEQUENCE_CHECKER(sequence_checker_);

  // Monotonically increasing counters for tabs and slices.
  int slice_id_counter_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A map in which we store info about PageNodes to keep track of their state,
  // as well as the timing of their state transitions.
  PageNodeInfoMap page_node_info_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer which is used to trigger CollectSlice(), which records the UKM.
  base::RepeatingTimer collect_slice_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

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

  // Time when last slice was run.
  base::TimeTicks time_of_last_slice_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::TimeTicks::Now();

  // Time of last PageResourceUsage collection.
  base::TimeTicks time_of_last_resource_usage_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::TimeTicks::Now();

  // Function which is called to determine whether a PageTimelineState slice
  // should be collected. Overridden in tests.
  base::RepeatingCallback<bool()> should_collect_slice_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool battery_saver_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Helper to take CPU measurements for the UKM.
  PageTimelineCPUMonitor cpu_monitor_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Helpers to take system CPU measurements for UMA.
  std::unique_ptr<CpuProbe> system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<CpuProbe> delayed_system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // WeakPtrFactory for the RepeatingTimer to call a method on this object.
  base::WeakPtrFactory<PageTimelineMonitor> weak_factory_{this};
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_
