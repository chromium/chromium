// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_cpu_monitor.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::metrics {

class PageTimelineMonitorUnitTest;

// Periodically reports tab state via UKM, to enable analysis of usage patterns
// over time.
class PageTimelineMonitor : public PageNode::ObserverDefaultImpl,
                            public GraphOwned,
                            public GraphRegisteredImpl<PageTimelineMonitor> {
 public:
  // Keep in sync with PageState in enums.xml
  enum class PageState {
    kFocused = 0,
    kVisible = 1,
    kBackground = 2,
    kThrottled = 3,
    kFrozen = 4,
    kDiscarded = 5,
    kMaxValue = kDiscarded,
  };

  PageTimelineMonitor();
  ~PageTimelineMonitor() override;
  PageTimelineMonitor(const PageTimelineMonitor& other) = delete;
  PageTimelineMonitor& operator=(const PageTimelineMonitor&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNode::Observer:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnPageLifecycleStateChanged(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_state) override;
  void OnAboutToBeDiscarded(const PageNode* page_node,
                            const PageNode* new_page_node) override;

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

  // Method collecting the PageResourceUsage UKM.
  void CollectPageResourceUsage();

  // Method collecting a slice for the PageTimelineState UKM.
  void CollectSlice();

  bool ShouldCollectSlice() const;

  void SetShouldCollectSliceCallbackForTesting(base::RepeatingCallback<bool()>);

  // Monotonically increasing counters for tabs and slices.
  int slice_id_counter_;

  // A map in which we store info about PageNodes to keep track of their state,
  // as well as the timing of their state transitions.
  std::map<const PageNode*, std::unique_ptr<PageNodeInfo>> page_node_info_map_;

  // Timer which is used to trigger CollectSlice(), which records the UKM.
  base::RepeatingTimer collect_slice_timer_;
  // Pointer to this process' graph.
  raw_ptr<Graph> graph_ = nullptr;

  // Time when last slice was run.
  base::TimeTicks time_of_last_slice_{base::TimeTicks::Now()};

  // Function which is called to determine whether a PageTimelineState slice
  // should be collected. Overridden in tests.
  base::RepeatingCallback<bool()> should_collect_slice_callback_;

  bool battery_saver_enabled_ = false;

  // Helper to take CPU measurements for the UKM.
  PageTimelineCPUMonitor cpu_monitor_;

  // WeakPtrFactory for the RepeatingTimer to call a method on this object.
  base::WeakPtrFactory<PageTimelineMonitor> weak_factory_{this};
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_MONITOR_H_
