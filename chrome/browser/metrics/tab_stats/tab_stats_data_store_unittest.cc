// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_data_store.h"

#include <memory>

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

class TabStatsDataStoreTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabStatsDataStoreTest() {
    TabStatsTracker::RegisterPrefs(pref_service_.registry());
    data_store_ = std::make_unique<TabStatsDataStore>(&pref_service_);
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TabStatsDataStore> data_store_;
};

}  // namespace

TEST_F(TabStatsDataStoreTest, TabStatsGetsReloadedFromLocalState) {
  // This tests creates add some tabs/windows to a data store instance and then
  // reinitializes it (and so the count of active tabs/windows drops to zero).
  // As the TabStatsTracker constructor restores its state from the pref service
  // the maximums should be restored.
  size_t expected_tab_count = 12;
  std::vector<std::unique_ptr<content::WebContents>> test_web_contents_vec;
  for (size_t i = 0; i < expected_tab_count; ++i) {
    test_web_contents_vec.emplace_back(CreateTestWebContents());
    data_store_->OnTabAdded(test_web_contents_vec.back().get());
  }
  size_t expected_window_count = 5;
  for (size_t i = 0; i < expected_window_count; ++i)
    data_store_->OnWindowAdded();
  size_t expected_max_tab_per_window = expected_tab_count - 1;
  data_store_->UpdateMaxTabsPerWindowIfNeeded(expected_max_tab_per_window);

  TabsStats stats = data_store_->tab_stats();
  EXPECT_EQ(expected_tab_count, stats.total_tab_count_max);
  EXPECT_EQ(expected_max_tab_per_window, stats.max_tab_per_window);
  EXPECT_EQ(expected_window_count, stats.window_count_max);

  // Reset the |tab_stats_tracker_| and ensure that the maximums are restored.
  data_store_ = std::make_unique<TabStatsDataStore>(&pref_service_);

  TabsStats stats2 = data_store_->tab_stats();
  EXPECT_EQ(stats.total_tab_count_max, stats2.total_tab_count_max);
  EXPECT_EQ(stats.max_tab_per_window, stats2.max_tab_per_window);
  EXPECT_EQ(stats.window_count_max, stats2.window_count_max);
  // The actual number of tabs/window should be 0 as it's not stored in the
  // pref service.
  EXPECT_EQ(0U, stats2.window_count);
  EXPECT_EQ(0U, stats2.total_tab_count);
}

TEST_F(TabStatsDataStoreTest, DiscardsFromLocalState) {
  // This test updates the discard/reload counts to a data store instance and
  // then reinitialize it. The data store instance should restore the discard
  // and reload counts from the pref service.
  constexpr size_t kExpectedDiscardsExternal = 3;
  constexpr size_t kExpectedDiscardsUrgent = 5;
  constexpr size_t kExpectedDiscardsProactive = 6;
  constexpr size_t kExpectedDiscardsSuggested = 7;
  constexpr size_t kExpectedReloadsExternal = 8;
  constexpr size_t kExpectedReloadsUrgent = 13;
  constexpr size_t kExpectedReloadsProactive = 4;
  constexpr size_t kExpectedReloadsSuggested = 9;
  for (size_t i = 0; i < kExpectedDiscardsExternal; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::EXTERNAL,
                                         /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsUrgent; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::URGENT,
                                         /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsProactive; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::PROACTIVE,
                                         /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsSuggested; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::SUGGESTED,
                                         /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedReloadsExternal; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::EXTERNAL,
                                         /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsUrgent; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::URGENT,
                                         /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsProactive; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::PROACTIVE,
                                         /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsSuggested; ++i) {
    data_store_->OnTabDiscardStateChange(LifecycleUnitDiscardReason::SUGGESTED,
                                         /*is_discarded=*/false);
  }

  const size_t external =
      static_cast<size_t>(LifecycleUnitDiscardReason::EXTERNAL);
  const size_t urgent = static_cast<size_t>(LifecycleUnitDiscardReason::URGENT);
  const size_t proactive =
      static_cast<size_t>(LifecycleUnitDiscardReason::PROACTIVE);
  const size_t suggested =
      static_cast<size_t>(LifecycleUnitDiscardReason::SUGGESTED);
  TabsStats stats = data_store_->tab_stats();
  EXPECT_EQ(kExpectedDiscardsExternal, stats.tab_discard_counts[external]);
  EXPECT_EQ(kExpectedDiscardsUrgent, stats.tab_discard_counts[urgent]);
  EXPECT_EQ(kExpectedDiscardsProactive, stats.tab_discard_counts[proactive]);
  EXPECT_EQ(kExpectedDiscardsSuggested, stats.tab_discard_counts[suggested]);
  EXPECT_EQ(kExpectedReloadsExternal, stats.tab_reload_counts[external]);
  EXPECT_EQ(kExpectedReloadsUrgent, stats.tab_reload_counts[urgent]);
  EXPECT_EQ(kExpectedReloadsProactive, stats.tab_reload_counts[proactive]);
  EXPECT_EQ(kExpectedReloadsSuggested, stats.tab_reload_counts[suggested]);

  // Resets the |data_store_| and checks discard/reload counters are restored.
  data_store_ = std::make_unique<TabStatsDataStore>(&pref_service_);

  TabsStats stats2 = data_store_->tab_stats();
  EXPECT_EQ(kExpectedDiscardsExternal, stats2.tab_discard_counts[external]);
  EXPECT_EQ(kExpectedDiscardsUrgent, stats2.tab_discard_counts[urgent]);
  EXPECT_EQ(kExpectedDiscardsProactive, stats2.tab_discard_counts[proactive]);
  EXPECT_EQ(kExpectedDiscardsSuggested, stats2.tab_discard_counts[suggested]);
  EXPECT_EQ(kExpectedReloadsExternal, stats2.tab_reload_counts[external]);
  EXPECT_EQ(kExpectedReloadsUrgent, stats2.tab_reload_counts[urgent]);
  EXPECT_EQ(kExpectedReloadsProactive, stats2.tab_reload_counts[proactive]);
  EXPECT_EQ(kExpectedReloadsSuggested, stats2.tab_reload_counts[suggested]);
}

}  // namespace metrics
