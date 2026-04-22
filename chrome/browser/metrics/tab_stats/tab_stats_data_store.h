// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_DATA_STORE_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_DATA_STORE_H_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/profiler/sample_metadata.h"
#include "base/sequence_checker.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "content/public/browser/visibility.h"

using mojom::LifecycleUnitDiscardReason;

class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

FORWARD_DECLARE_TEST(TabStatsTrackerBrowserTest,
                     TabDeletionGetsHandledProperly);

// Keeps track of all the information needed by TabStatsTracker. Stats are
// stored internally to be retrieved at a later point.
class TabStatsDataStore : public TabStatsObserver {
 public:
  // Houses all of the statistics gathered by the TabStatsTracker.
  struct TabsStats {
    // Constructor, initializes everything to zero.
    TabsStats();
    TabsStats(const TabsStats& other);
    TabsStats& operator=(const TabsStats& other);

    // The total number of tabs opened across all the windows.
    size_t total_tab_count;

    // The maximum total number of tabs that has been opened across all the
    // windows at the same time.
    size_t total_tab_count_max;

    // The maximum total number of tabs that has been opened at the same time in
    // a single window.
    size_t max_tab_per_window;

    // The total number of windows opened.
    size_t window_count;

    // The maximum total number of windows opened at the same time.
    size_t window_count_max;

    // The number of tabs discarded, per discard reason.
    std::array<size_t,
               static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) + 1>
        tab_discard_counts;

    // The number of tabs reloaded after a discard, per discard reason.
    std::array<size_t,
               static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) + 1>
        tab_reload_counts;
  };

  explicit TabStatsDataStore(PrefService* pref_service);

  TabStatsDataStore(const TabStatsDataStore&) = delete;
  TabStatsDataStore& operator=(const TabStatsDataStore&) = delete;

  ~TabStatsDataStore() override;

  // TabStatsObserver:
  void OnWindowAdded() override;
  void OnWindowRemoved() override;
  void OnTabAdded(content::WebContents* web_contents) override;
  void OnTabRemoved(content::WebContents* web_contents) override;

  // Update the maximum number of tabs in a single window if |value| exceeds
  // this.
  // TODO(sebmarchand): Store a list of windows in this class and track the
  // number of tabs per window.
  void UpdateMaxTabsPerWindowIfNeeded(size_t value);

  // Reset all the maximum values to the current state, to be used once the
  // metrics have been reported.
  void ResetMaximumsToCurrentState();

  // Updates discard/reload counts when the discarded state of a tab changes.
  // Updates the discard count when is_discarded is true. Updates the reload
  // count when is_discarded is false.
  void OnTabDiscardStateChange(LifecycleUnitDiscardReason discard_reason,
                               bool is_discarded);

  // Clears the discard and reload counters. Called after reporting the counter
  // values.
  void ClearTabDiscardAndReloadCounts();

  const TabsStats& tab_stats() const { return tab_stats_; }

 protected:
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TabDeletionGetsHandledProperly);

  // Update the maximums metrics if needed.
  void UpdateTotalTabCountMaxIfNeeded();
  void UpdateWindowCountMaxIfNeeded();

 private:
  // Record the stack sampling meta data with the current tab count;
  void RecordSamplingMetaData();

  // The tabs stats.
  TabsStats tab_stats_;

  // Used to asssociate sampling profiler samples to the number of tabs.
  base::SampleMetadata tab_number_sample_meta_data_ =
      base::SampleMetadata("NumberOfTabs", base::SampleMetadataScope::kProcess);

  // A raw pointer to the PrefService used to read and write the statistics.
  raw_ptr<PrefService> pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_DATA_STORE_H_
