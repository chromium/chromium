// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;
using TabLifecycleObserver = resource_coordinator::TabLifecycleObserver;

class FreezeWaiter : public TabLifecycleObserver {
 public:
  explicit FreezeWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    resource_coordinator::TabLifecycleUnitExternal::AddTabLifecycleObserver(
        this);
  }

  ~FreezeWaiter() override {
    resource_coordinator::TabLifecycleUnitExternal::RemoveTabLifecycleObserver(
        this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  void OnFrozenStateChange(content::WebContents* contents,
                           bool is_frozen) override {
    if (web_contents_ != contents || !is_frozen)
      return;

    run_loop_.Quit();
  }

  content::WebContents* web_contents_;
  base::RunLoop run_loop_;
};

void EnsureTabStatsMatchExpectations(const TabsStats& expected,
                                     const TabsStats& actual) {
  EXPECT_EQ(expected.total_tab_count, actual.total_tab_count);
  EXPECT_EQ(expected.total_tab_count_max, actual.total_tab_count_max);
  EXPECT_EQ(expected.max_tab_per_window, actual.max_tab_per_window);
  EXPECT_EQ(expected.window_count, actual.window_count);
  EXPECT_EQ(expected.window_count_max, actual.window_count_max);
}

void FreezeWebContents(content::WebContents* web_contents) {
  FreezeWaiter freeze_waiter(web_contents);
  web_contents->SetPageFrozen(true);
  freeze_waiter.Wait();
}

}  // namespace

class MockTabStatsTrackerDelegate : public TabStatsTrackerDelegate {
 public:
  MockTabStatsTrackerDelegate() = default;
  ~MockTabStatsTrackerDelegate() override = default;

#if defined(OS_WIN)
  OcclusionStatusMap CallComputeNativeWindowOcclusionStatus(
      std::vector<aura::WindowTreeHost*> hosts) override {
    // Checking that the hosts are not nullptr, because of a bug where nullptr
    // was being passed in addition to the desired aura::WindowTreeHost
    // pointers, causing a crash when dereferenced. Crash bug found at:
    // crbug.com/837541
    for (aura::WindowTreeHost* host : hosts)
      DCHECK(host);

    return mock_occlusion_results_;
  }

  void SetMockOcclusionResults(OcclusionStatusMap mock_occlusion_results) {
    mock_occlusion_results_ = mock_occlusion_results;
  }

 private:
  OcclusionStatusMap mock_occlusion_results_;
#endif
};

class TabStatsTrackerBrowserTest : public InProcessBrowserTest {
 public:
  TabStatsTrackerBrowserTest() : tab_stats_tracker_(nullptr) {}

  void SetUpOnMainThread() override {
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
    ASSERT_TRUE(tab_stats_tracker_ != nullptr);
  }

 protected:
  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  TabStatsTracker* tab_stats_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest, FrozenTabPercentage) {
  std::vector<Browser*> browsers;

  // Create 3 windows.
  browsers.push_back(browser());
  browsers.push_back(CreateBrowser(ProfileManager::GetActiveUserProfile()));
  browsers.push_back(CreateBrowser(ProfileManager::GetActiveUserProfile()));

  // Ensure there are 3 tabs per window.
  for (Browser* browser : browsers) {
    ui_test_utils::NavigateToURL(browser, GURL("about:blank"));
    AddTabAtIndexToBrowser(browser, 1, GURL("about:blank"),
                           ui::PAGE_TRANSITION_TYPED, true);
    AddTabAtIndexToBrowser(browser, 2, GURL("about:blank"),
                           ui::PAGE_TRANSITION_TYPED, true);
  }

  // Verify that there are 6 hidden tabs, none of which are frozen.
  tab_stats_tracker_->OnHeartbeatEvent();
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.6To20HiddenTabs",
                                     1);
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.1To5HiddenTabs",
                                     0);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Tabs.FrozenTabPercentage.6To20HiddenTabs"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Freeze 2 of the hidden tabs.
  FreezeWebContents(browsers[0]->tab_strip_model()->GetWebContentsAt(0));
  FreezeWebContents(browsers[1]->tab_strip_model()->GetWebContentsAt(0));

  // Verify that 2/6 hidden tabs are frozen.
  tab_stats_tracker_->OnHeartbeatEvent();
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.6To20HiddenTabs",
                                     2);
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.1To5HiddenTabs",
                                     0);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Tabs.FrozenTabPercentage.6To20HiddenTabs"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(33, 1)));

  // Close one of the frozen, hidden tabs.
  browsers[0]->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_NONE);

  // Verify that 1/5 hidden tabs are frozen.
  tab_stats_tracker_->OnHeartbeatEvent();
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.6To20HiddenTabs",
                                     2);
  histogram_tester_.ExpectTotalCount("Tabs.FrozenTabPercentage.1To5HiddenTabs",
                                     1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Tabs.FrozenTabPercentage.1To5HiddenTabs"),
              testing::ElementsAre(base::Bucket(20, 1)));
}

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabsAndWindowsAreCountedAccurately) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  TabsStats expected_stats = {};

  // There should be only one window with one tab at startup.
  expected_stats.total_tab_count = 1;
  expected_stats.total_tab_count_max = 1;
  expected_stats.max_tab_per_window = 1;
  expected_stats.window_count = 1;
  expected_stats.window_count_max = 1;

  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  // Add a tab and make sure that the counters get updated.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  ++expected_stats.max_tab_per_window;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  --expected_stats.total_tab_count;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  ++expected_stats.total_tab_count;
  ++expected_stats.window_count;
  ++expected_stats.window_count_max;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  AddTabAtIndexToBrowser(browser, 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  CloseBrowserSynchronously(browser);
  expected_stats.total_tab_count = 1;
  expected_stats.window_count = 1;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
}

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabDeletionGetsHandledProperly) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  constexpr base::TimeDelta kValidLongInterval = base::TimeDelta::FromHours(12);

  TabStatsDataStore* data_store = tab_stats_tracker_->tab_stats_data_store();
  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
      data_store->AddInterval();

  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);

  EXPECT_EQ(2U, interval_map->size());

  content::WebContents* web_contents =
      data_store->existing_tabs_for_testing()->begin()->first;

  // Delete one of the WebContents without calling the |OnTabRemoved| function,
  // the WebContentsObserver owned by |tab_stats_tracker_| should be notified
  // and this should be handled correctly.
  TabStatsDataStore::TabID tab_id =
      data_store->GetTabIDForTesting(web_contents).value();
  browser()->tab_strip_model()->DetachWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  EXPECT_TRUE(base::Contains(*interval_map, tab_id));
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  EXPECT_EQ(1U, interval_map->size());
  EXPECT_FALSE(base::Contains(*interval_map, tab_id));

  web_contents = data_store->existing_tabs_for_testing()->begin()->first;

  // Do this a second time, ensures that the situation where there's no existing
  // tabs is handled properly.
  tab_id = data_store->GetTabIDForTesting(web_contents).value();
  browser()->tab_strip_model()->DetachWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  EXPECT_TRUE(base::Contains(*interval_map, tab_id));
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  EXPECT_EQ(0U, interval_map->size());
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TestCalculateAndRecordNativeWindowVisibilities) {
  std::unique_ptr<MockTabStatsTrackerDelegate> mock_delegate =
      std::make_unique<MockTabStatsTrackerDelegate>();

  // Maintaining this reference to |mock_delegate| is safe because the
  // TabStatsTracker will outlive this test class.
  MockTabStatsTrackerDelegate* mock_delegate_raw = mock_delegate.get();
  tab_stats_tracker_->SetDelegateForTesting(std::move(mock_delegate));

  TabStatsTrackerDelegate::OcclusionStatusMap mock_occlusion_results;

  mock_delegate_raw->SetMockOcclusionResults(mock_occlusion_results);

  tab_stats_tracker_->CalculateAndRecordNativeWindowVisibilities();

  // There should be 1 entry for each zero window bucket.
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Occluded",
                                      0, 1);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Visible",
                                      0, 1);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Hidden",
                                      0, 1);

  // There should be no entries in the 1 window bucket.
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Occluded",
                                      1, 0);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Visible",
                                      1, 0);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Hidden",
                                      1, 0);

  // Create a browser for each aura::Window::OcclusionState.
  mock_occlusion_results[CreateBrowser(ProfileManager::GetActiveUserProfile())
                             ->window()
                             ->GetNativeWindow()
                             ->GetHost()] =
      aura::Window::OcclusionState::HIDDEN;
  mock_occlusion_results[CreateBrowser(ProfileManager::GetActiveUserProfile())
                             ->window()
                             ->GetNativeWindow()
                             ->GetHost()] =
      aura::Window::OcclusionState::VISIBLE;
  mock_occlusion_results[CreateBrowser(ProfileManager::GetActiveUserProfile())
                             ->window()
                             ->GetNativeWindow()
                             ->GetHost()] =
      aura::Window::OcclusionState::OCCLUDED;

  mock_delegate_raw->SetMockOcclusionResults(mock_occlusion_results);

  // There should now be 1 entry for each 1 window bucket.
  tab_stats_tracker_->CalculateAndRecordNativeWindowVisibilities();
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Occluded",
                                      1, 1);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Visible",
                                      1, 1);
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Hidden",
                                      1, 1);

  mock_occlusion_results.clear();

  // Create 5 aura::Window::OcclusionState browsers.
  for (int count = 0; count < 5; count++) {
    mock_occlusion_results[CreateBrowser(ProfileManager::GetActiveUserProfile())
                               ->window()
                               ->GetNativeWindow()
                               ->GetHost()] =
        aura::Window::OcclusionState::OCCLUDED;
  }

  mock_delegate_raw->SetMockOcclusionResults(mock_occlusion_results);
  tab_stats_tracker_->CalculateAndRecordNativeWindowVisibilities();

  // There should be 1 entry in the 5 window occluded bucket.
  histogram_tester_.ExpectBucketCount("Windows.NativeWindowVisibility.Occluded",
                                      5, 1);
}

#endif  // defined(OS_WIN)
}  // namespace metrics
