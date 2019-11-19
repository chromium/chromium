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
// system-related events and properties during session restore or background tab
// opening session.
class TabManagerStatsCollector final : public SessionRestoreObserver {
 public:
  enum SessionType {
    // SessionRestore is the duration from the time when the browser starts to
    // restore tabs until the time when the browser has finished loading those
    // tabs or when the browser stops loading tabs due to memory pressure.
    // During SessionRestore, some other tabs could be open due to user action,
    // but that would not affect the session end time point. For example, a
    // browser starts to restore tab1 and tab2. Tab3 is open due to user
    // clicking a link in tab1. SessionRestore ends after tab1 and tab2 finishes
    // loading, even if tab3 is still loading.
    kSessionRestore,

    // BackgroundTabOpening session is the duration from the time when the
    // browser starts to open background tabs until the time when browser has
    // finished loading those tabs or paused loading due to memory pressure.
    // A new session starts when the browser resumes loading background tabs
    // when memory pressure returns to normal. During the BackgroundTabOpening
    // session, some background tabs could become foreground due to user action,
    // but that would not affect the session end time point. For example, a
    // browser has tab1 in foreground, and starts to open tab2 and tab3 in
    // background. The session will end after tab2 and tab3 finishes loading,
    // even if tab2 and tab3 are brought to foreground before they are loaded.
    // BackgroundTabOpening session excludes the background tabs being
    // controlled by SessionRestore, so that those two activities are clearly
    // separated.
    kBackgroundTabOpening
  };

  // Houses all of the background tab count statistics gathered by
  // TabManagerStatsCollector for background tab opening.
  struct BackgroundTabCountStats {
    // Reset everything to zero. This is called when a background tab opening
    // session starts.
    void Reset();

    // The max number of background tabs pending or loading in a background
    // tab opening session.
    size_t tab_count;

    // The max number of background tabs that were paused to load in a
    // background tab opening session due to memory pressure.
    size_t tab_paused_count;

    // The number of background tabs whose loading was triggered by TabManager
    // automatically.
    size_t tab_load_auto_started_count;

    // The number of background tabs whose loading was triggered by user
    // selection.
    size_t tab_load_user_initiated_count;
  };

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

  // Record expected task queueing durations of foreground tabs in session
  // restore.
  void RecordExpectedTaskQueueingDuration(content::WebContents* contents,
                                          base::TimeDelta queueing_time);

  // Record background tab count for BackgroundTabOpening.
  void RecordBackgroundTabCount();

  // SessionRestoreObserver
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

  // The following two functions defines the start and end of a background tab
  // opening session.
  void OnBackgroundTabOpeningSessionStarted();
  void OnBackgroundTabOpeningSessionEnded();

  // Track background tabs and update background tab count stats.
  void TrackNewBackgroundTab(size_t pending_tabs, size_t loading_tabs) {
    background_tab_count_stats_.tab_count = std::max(
        background_tab_count_stats_.tab_count, pending_tabs + loading_tabs + 1);
  }
  void TrackPausedBackgroundTabs(size_t paused_tabs) {
    background_tab_count_stats_.tab_paused_count =
        std::max(background_tab_count_stats_.tab_paused_count, paused_tabs);
  }
  void TrackBackgroundTabLoadAutoStarted() {
    background_tab_count_stats_.tab_load_auto_started_count++;
  }
  void TrackBackgroundTabLoadUserInitiated() {
    background_tab_count_stats_.tab_load_user_initiated_count++;
  }

  // Record UMA histograms for system swap metrics.
  void RecordSwapMetrics(SessionType type,
                         const std::string& metric_name,
                         uint64_t count,
                         base::TimeDelta interval);

  // Handles the situation when failing to update swap metrics.
  void OnUpdateSwapMetricsFailed();

  // Called by WebContentsData when a tab starts loading. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it, which will happen if the
  // user navigates to a new page and |contents| is resused.
  void OnDidStartMainFrameNavigation(content::WebContents* contents);

  // Called by TabManager when it decides to load the next tab. Used as the
  // signal to record how often timeout happens. Timeout means it wants to start
  // loading the next tab even though the previous tab is still loading.
  void OnWillLoadNextBackgroundTab(bool timeout);

  // Called by TabManager when a tab is considered loaded. Used as the signal to
  // record tab switch load time metrics for |contents|.
  void OnTabIsLoaded(content::WebContents* contents);

  // Called by TabManager when a WebContents is destroyed. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it.
  void OnWebContentsDestroyed(content::WebContents* contents);

  bool is_in_background_tab_opening_session() const {
    return is_in_background_tab_opening_session_;
  }

 private:
  class SwapMetricsDelegate;
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsSwitchToTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsTabSwitchLoadTime);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorParameterizedTest,
                           HistogramsExpectedTaskQueueingDuration);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorParameterizedTest,
                           HistogramsTabCount);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTest,
                           HistogramsSessionOverlap);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTest, PeriodicSamplingWorks);

  // Returns true if the browser is currently in more than one session with
  // different types. We do not want to report metrics in this situation to have
  // meaningful results. For example, SessionRestore and BackgroundTabOpening
  // session could overlap.
  bool IsInOverlappedSession();

  // This is called when we enter overlapped session. We need to clear all stats
  // in such situation to avoid reporting mixed metric results.
  void ClearStatsWhenInOverlappedSession();

  // Create and initialize the swap metrics driver if needed. This will set the
  // driver to null when it is in overlapped session situation, so that we do
  // not report swap metrics.
  void CreateAndInitSwapMetricsDriverIfNeeded(SessionType type);

  // Update session and sequence information for UKM recording.
  void UpdateSessionAndSequence();

  // This is called sometime after startup, and initiates periodic CanFreeze/
  // CanDiscard metric sampling. It posts a delayed task to
  // PerformPeriodicSample.
  void StartPeriodicSampling();

  // This is called when a sample should be taken. First call is via
  // StartPeriodicSampling and then it posts a delayed task to call itself.
  void PerformPeriodicSample();

  // Helper function for RecordSampledTabData. Records a single UKM entry for
  // the provided DecisionDetails and destination lifecycle state.
  static void RecordDecisionDetails(LifecycleUnit* lifecycle_unit,
                                    const DecisionDetails& decision_details,
                                    LifecycleUnitState new_state);

  static const char
      kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration[];
  static const char
      kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration[];
  static const char kHistogramSessionRestoreSwitchToTab[];
  static const char kHistogramBackgroundTabOpeningSwitchToTab[];
  static const char kHistogramSessionRestoreTabSwitchLoadTime[];
  static const char kHistogramBackgroundTabOpeningTabSwitchLoadTime[];
  static const char kHistogramBackgroundTabOpeningTabCount[];
  static const char kHistogramBackgroundTabOpeningTabPausedCount[];
  static const char kHistogramBackgroundTabOpeningTabLoadAutoStartedCount[];
  static const char kHistogramBackgroundTabOpeningTabLoadUserInitiatedCount[];
  static const char kHistogramBackgroundTabOpeningTabLoadTimeout[];
  static const char kHistogramSessionOverlapSessionRestore[];
  static const char kHistogramSessionOverlapBackgroundTabOpening[];

  // The rough sampling interval for low-frequency sampled stats. This should
  // be O(minutes).
  static constexpr base::TimeDelta kLowFrequencySamplingInterval =
      base::TimeDelta::FromMinutes(5);

  // TabManagerStatsCollector should be used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  int session_id_ = -1;
  int sequence_ = 0;

  bool is_session_restore_loading_tabs_ = false;
  bool is_in_background_tab_opening_session_ = false;

  bool is_overlapping_session_restore_ = false;
  bool is_overlapping_background_tab_opening_ = false;

  // This is shared between SessionRestore and BackgroundTabOpening because we
  // do not report metrics when those two overlap.
  std::unique_ptr<content::SwapMetricsDriver> swap_metrics_driver_;

  // The set of foreground tabs during session restore or background tab opening
  // sessions that were switched to have not yet finished loading, mapped to the
  // time that they were switched to. It's possible to have multiple background
  // opening sessions or session restores happening simultaneously in different
  // windows, which means there can be multiple foreground tabs that have been
  // switched to that haven't finished loading. Because of that, we need to
  // track each foreground tab and its corresponding switch-to time.
  std::unordered_map<content::WebContents*, base::TimeTicks>
      foreground_contents_switched_to_times_;

  BackgroundTabCountStats background_tab_count_stats_;

  // The start time of an ongoing periodic sample.
  base::TimeTicks sample_start_time_;

  base::WeakPtrFactory<TabManagerStatsCollector> weak_factory_{this};
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
