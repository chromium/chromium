// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/decision_details.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class SwapMetricsDriver;
class WebContents;
}  // namespace content

namespace resource_coordinator {

// TabManagerStatsCollector records UMAs on behalf of TabManager for tab and
// system-related events and properties during session restore.
//
// SessionRestore is the duration from the time when the browser starts to
// restore tabs until the time when the browser has finished loading those tabs
// or when the browser stops loading tabs due to memory pressure. During
// SessionRestore, some other tabs could be open due to user action, but that
// would not affect the session end time point. For example, a browser starts to
// restore tab1 and tab2. Tab3 is open due to user clicking a link in tab1.
// SessionRestore ends after tab1 and tab2 finishes loading, even if tab3 is
// still loading.

class TabManagerStatsCollector final : public SessionRestoreObserver {
 public:
  TabManagerStatsCollector();
  ~TabManagerStatsCollector();

  // Records histograms *before* starting to urgently discard LifecycleUnits.
  // |num_alive_tabs| is the number of tabs that are not pending load or
  // discarded.
  void RecordWillDiscardUrgently(int num_alive_tabs);

  // Records UMA histograms for the tab state when switching to a different tab
  // during session restore.
  void RecordSwitchToTab(content::WebContents* old_contents,
                         content::WebContents* new_contents);

  // SessionRestoreObserver
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

  // Record UMA histograms for system swap metrics.
  void RecordSwapMetrics(const std::string& metric_name,
                         uint64_t count,
                         base::TimeDelta interval);

  // Handles the situation when failing to update swap metrics.
  void OnUpdateSwapMetricsFailed();

  // Called by WebContentsData when a tab starts loading. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it, which will happen if the
  // user navigates to a new page and |contents| is resused.
  void OnDidStartMainFrameNavigation(content::WebContents* contents);

  // Called by TabManager when a tab is considered loaded. Used as the signal to
  // record tab switch load time metrics for |contents|.
  void OnTabIsLoaded(content::WebContents* contents);

  // Called by TabManager when a WebContents is destroyed. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it.
  void OnWebContentsDestroyed(content::WebContents* contents);

 private:
  class SwapMetricsDelegate;
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsSwitchToTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsTabSwitchLoadTime);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorParameterizedTest,
                           HistogramsTabCount);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorPrerenderingTest,
                           KeepingWebContentsMapInPrerendering);

  // Create and initialize the swap metrics driver if needed.
  void CreateAndInitSwapMetricsDriverIfNeeded();

  // Update session and sequence information for UKM recording.
  void UpdateSessionAndSequence();

  static const char kHistogramSessionRestoreSwitchToTab[];
  static const char kHistogramSessionRestoreTabSwitchLoadTime[];
  static const char kHistogramSessionOverlapSessionRestore[];

  // The rough sampling interval for low-frequency sampled stats. This should
  // be O(minutes).
  static constexpr base::TimeDelta kLowFrequencySamplingInterval =
      base::Minutes(5);

  // TabManagerStatsCollector should be used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  int session_id_ = -1;
  int sequence_ = 0;

  bool is_session_restore_loading_tabs_ = false;

  std::unique_ptr<content::SwapMetricsDriver> swap_metrics_driver_;

  // The set of foreground tabs during session restore that were switched to
  // that have not yet finished loading, mapped to the time that they were
  // switched to. It's possible to have multiple session restores happening
  // simultaneously in different windows, which means there can be multiple
  // foreground tabs that have been switched to that haven't finished loading.
  // Because of that, we need to track each foreground tab and its corresponding
  // switch-to time.
  std::unordered_map<content::WebContents*, base::TimeTicks>
      foreground_contents_switched_to_times_;

  base::WeakPtrFactory<TabManagerStatsCollector> weak_factory_{this};
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
