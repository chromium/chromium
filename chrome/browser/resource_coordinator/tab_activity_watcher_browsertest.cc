// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using ukm::builders::TabManager_TabMetrics;
using ukm::builders::TabManager_Background_ForegroundedOrClosed;

namespace resource_coordinator {

namespace {

const char* kEntryName = TabManager_TabMetrics::kEntryName;
const char* kFOCEntryName =
    TabManager_Background_ForegroundedOrClosed::kEntryName;

// The default metric values for a tab.
const UkmMetricMap kBasicMetricValues({
    {TabManager_TabMetrics::kHasBeforeUnloadHandlerName, 0},
    {TabManager_TabMetrics::kHasFormEntryName, 0},
    {TabManager_TabMetrics::kIsPinnedName, 0},
    {TabManager_TabMetrics::kKeyEventCountName, 0},
    {TabManager_TabMetrics::kNavigationEntryCountName, 1},
    {TabManager_TabMetrics::kSiteEngagementScoreName, 0},
    {TabManager_TabMetrics::kTouchEventCountName, 0},
    {TabManager_TabMetrics::kWasRecentlyAudibleName, 0},
    // TODO(michaelpg): Test TabManager_TabMetrics::kMouseEventCountName.
    // Depending on the test environment, the browser may receive mouse events
    // from the mouse cursor during tests, so we currently don't check this
    // metric.
});

// These parameters don't affect logging.
const bool kIsUserGesture = true;
const bool kCheckNavigationSuccess = true;

}  // namespace

class TabActivityWatcherTest : public InProcessBrowserTest {
 protected:
  TabActivityWatcherTest() = default;
  ~TabActivityWatcherTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    test_urls_ = {embedded_test_server()->GetURL("/title1.html"),
                  embedded_test_server()->GetURL("/title2.html"),
                  embedded_test_server()->GetURL("/title3.html")};
  }

  std::vector<GURL> test_urls_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabActivityWatcherTest);
};

// Tests calculating tab scores using the Tab Ranker.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherTest, CalculateReactivationScore) {
  // Use test clock so tabs have non-zero backgrounded times.
  base::SimpleTestTickClock test_clock;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing(&test_clock);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));

  AddTabAtIndex(1, test_urls_[0], ui::PAGE_TRANSITION_LINK);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));

  browser()->tab_strip_model()->ActivateTabAt(0, kIsUserGesture);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));

  // A background tab is scored successfully.
  base::Optional<float> background_score =
      TabActivityWatcher::GetInstance()->CalculateReactivationScore(
          browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_TRUE(background_score.has_value());

  // Foreground tabs are not modeled by the tab ranker and should not be scored.
  base::Optional<float> foreground_score =
      TabActivityWatcher::GetInstance()->CalculateReactivationScore(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_FALSE(foreground_score.has_value());

  CloseBrowserSynchronously(browser());
}

// Tests only oldest N tabs are scored.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherTest,
                       OnlyCalculateReactivationScoreForOldestN) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTabRanker,
      {{"number_of_oldest_tabs_to_score_with_TabRanker", "1"}});
  // Use test clock so tabs have non-zero backgrounded times.
  base::SimpleTestTickClock test_clock;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing(&test_clock);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));

  AddTabAtIndex(1, test_urls_[0], ui::PAGE_TRANSITION_LINK);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  AddTabAtIndex(2, test_urls_[0], ui::PAGE_TRANSITION_LINK);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  browser()->tab_strip_model()->ActivateTabAt(0, kIsUserGesture);
  test_clock.Advance(base::TimeDelta::FromMinutes(1));

  // tab@1 is scored successfully.
  base::Optional<float> tab_1 =
      TabActivityWatcher::GetInstance()->CalculateReactivationScore(
          browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_TRUE(tab_1.has_value());

  // tab@2 is not scored successfully since it's not in the OldestN.
  base::Optional<float> tab_2 =
      TabActivityWatcher::GetInstance()->CalculateReactivationScore(
          browser()->tab_strip_model()->GetWebContentsAt(2));
  EXPECT_FALSE(tab_2.has_value());

  CloseBrowserSynchronously(browser());
}
// Tests UKM entries generated by TabActivityWatcher/TabMetricsLogger as tabs
// are backgrounded and foregrounded.
// Modeled after the TabActivityWatcherTest unit tests, these browser tests
// focus on end-to-end testing from the first browser launch onwards, verifying
// that window and browser commands are really triggering the paths that lead
// to UKM logs.
class TabActivityWatcherUkmTest : public TabActivityWatcherTest {
 protected:
  TabActivityWatcherUkmTest() = default;

  // TabActivityWatcherTest:
  void PreRunTestOnMainThread() override {
    TabActivityWatcherTest::PreRunTestOnMainThread();

    ukm_entry_checker_ = std::make_unique<UkmEntryChecker>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    // Browser created in BrowserMain() shouldn't result in a background tab
    // being logged.
    EXPECT_EQ(0u, ukm_entry_checker_->NumEntries(kEntryName));

    TabActivityWatcherTest::SetUpOnMainThread();
  }

  void TearDown() override {
    EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kEntryName));

    TabActivityWatcherTest::TearDown();
  }

 protected:
  void ExpectNewForegroundedEntry(const GURL& url) {
    // TODO(michaelpg): Add an interactive_ui_test to test MRU metrics since
    // they can be affected by window activation.
    UkmMetricMap expected_metrics = {
        {TabManager_Background_ForegroundedOrClosed::kIsForegroundedName, 1},
    };
    ukm_entry_checker_->ExpectNewEntry(kFOCEntryName, url, expected_metrics);
  }

  void ExpectNewClosedEntry(const GURL& url) {
    UkmMetricMap expected_metrics = {
        {TabManager_Background_ForegroundedOrClosed::kIsForegroundedName, 0},
    };
    ukm_entry_checker_->ExpectNewEntry(kFOCEntryName, url, expected_metrics);
  }

  // Uses test_ukm_recorder_ to check new event metrics including:
  // (1) the number of UkmEntry with given event_name should be equal to |size|.
  // (2) the newest entry should have source_url equal to |url|.
  // (3) the newest entry should have source_id equal to |source_id| if
  //     |source_id| is not 0 (skip for the case of 0).
  // (4) the newest entry should contain all metrics in |expected_metrics|.
  // Also returns the source_id of the newest entry.
  ukm::SourceId ExpectNewEntryWithSourceId(const GURL& url,
                                           const std::string& event_name,
                                           size_t num_entries,
                                           const UkmMetricMap& expected_metrics,
                                           ukm::SourceId source_id = 0) {
    const std::vector<const ukm::mojom::UkmEntry*> entries =
        test_ukm_recorder_->GetEntriesByName(event_name);
    // Check size.
    EXPECT_EQ(entries.size(), num_entries);
    const ukm::mojom::UkmEntry* entry = entries.back();
    // Check source_url.
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
    // Check source_id.
    if (source_id != 0) {
      EXPECT_EQ(source_id, entry->source_id);
    }
    // Check expected_metrics.
    for (const auto& metric : expected_metrics) {
      test_ukm_recorder_->ExpectEntryMetric(entry, metric.first,
                                            *metric.second);
    }

    return entry->source_id;
  }

  std::unique_ptr<UkmEntryChecker> ukm_entry_checker_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabActivityWatcherUkmTest);
};

// Tests TabMetrics UKMs logged by creating and switching between tabs.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherUkmTest, SwitchTabs) {
  const GURL kTabUrls[] = {
      GURL(),  // "about:blank" tab doesn't have a UKM source.
      test_urls_[0], test_urls_[1],
  };

  EXPECT_EQ(0u, ukm_entry_checker_->NumEntries(kEntryName));

  UkmMetricMap expected_metrics = kBasicMetricValues;
  expected_metrics[TabManager_TabMetrics::kWindowIdName] =
      browser()->session_id().id();

  // Adding a new foreground tab logs the previously active tab.
  AddTabAtIndex(1, kTabUrls[1], ui::PAGE_TRANSITION_LINK);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, kTabUrls[0],
                                       expected_metrics);
  }

  AddTabAtIndex(2, kTabUrls[2], ui::PAGE_TRANSITION_LINK);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, kTabUrls[1],
                                       expected_metrics);
  }

  // Switching to another tab logs the previously active tab.
  browser()->tab_strip_model()->ActivateTabAt(0, kIsUserGesture);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, kTabUrls[2],
                                       expected_metrics);
    ExpectNewForegroundedEntry(kTabUrls[0]);
  }

  browser()->tab_strip_model()->ActivateTabAt(1, kIsUserGesture);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, kTabUrls[0],
                                       expected_metrics);
    ExpectNewForegroundedEntry(kTabUrls[1]);
  }

  // Closing the window doesn't log more TabMetrics UKMs (tested in TearDown()).
  CloseBrowserSynchronously(browser());
  {
    SCOPED_TRACE("");
    ExpectNewClosedEntry(kTabUrls[2]);
    ExpectNewClosedEntry(kTabUrls[0]);
  }
}

// Tests that switching between multiple windows doesn't affect TabMetrics UKMs.
// This is a sanity check; window activation shouldn't make any difference to
// what we log. If we needed to actually test different behavior based on window
// focus, we would run these tests in interactive_ui_tests.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherUkmTest, SwitchWindows) {
  Browser* browser_2 = CreateBrowser(browser()->profile());
  EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kEntryName));

  AddTabAtIndexToBrowser(browser(), 1, test_urls_[0], ui::PAGE_TRANSITION_LINK,
                         kCheckNavigationSuccess);
  {
    SCOPED_TRACE("");
    UkmMetricMap expected_metrics = kBasicMetricValues;
    expected_metrics[TabManager_TabMetrics::kWindowIdName] =
        browser()->session_id().id();
    ukm_entry_checker_->ExpectNewEntry(kEntryName, GURL(), kBasicMetricValues);
  }

  AddTabAtIndexToBrowser(browser_2, 1, test_urls_[1], ui::PAGE_TRANSITION_LINK,
                         kCheckNavigationSuccess);
  {
    SCOPED_TRACE("");
    UkmMetricMap expected_metrics = kBasicMetricValues;
    expected_metrics[TabManager_TabMetrics::kWindowIdName] =
        browser_2->session_id().id();
    ukm_entry_checker_->ExpectNewEntry(kEntryName, GURL(), kBasicMetricValues);
  }

  browser()->window()->Activate();
  browser_2->window()->Activate();
  EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kEntryName));

  // Closing each window doesn't log more TabMetrics UKMs.
  CloseBrowserSynchronously(browser_2);
  CloseBrowserSynchronously(browser());
}

// Tests page with a beforeunload handler.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherUkmTest, BeforeUnloadHandler) {
  // Navigate to a page with a beforeunload handler.
  GURL url(embedded_test_server()->GetURL("/beforeunload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Log metrics for the first tab by switching to a new tab.
  AddTabAtIndex(1, test_urls_[0], ui::PAGE_TRANSITION_LINK);
  UkmMetricMap expected_metrics = kBasicMetricValues;
  expected_metrics[TabManager_TabMetrics::kHasBeforeUnloadHandlerName] = 1;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName] = 2;
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, url, expected_metrics);
  }

  // Sanity check: the new tab doesn't have a beforeunload handler.
  browser()->tab_strip_model()->ActivateTabAt(0, kIsUserGesture);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, test_urls_[0],
                                       kBasicMetricValues);
  }
}

// Tests events logged when dragging a tab between browsers.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherUkmTest, TabDrag) {
  // This test will navigate 3 tabs.
  const GURL kBrowserStartUrl = test_urls_[0];
  const GURL kBrowser2StartUrl = test_urls_[1];
  const GURL kDraggedTabUrl = test_urls_[2];

  Browser* browser_2 = CreateBrowser(browser()->profile());

  ui_test_utils::NavigateToURL(browser(), kBrowserStartUrl);
  ui_test_utils::NavigateToURL(browser_2, kBrowser2StartUrl);

  // Adding a tab backgrounds the original tab in the window.
  AddTabAtIndexToBrowser(browser(), 1, kDraggedTabUrl, ui::PAGE_TRANSITION_LINK,
                         kCheckNavigationSuccess);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(
        kEntryName, kBrowserStartUrl,
        {{TabManager_TabMetrics::kWindowIdName, browser()->session_id().id()}});
  }

  // "Drag" the new tab out of its browser.
  content::WebContents* dragged_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  std::unique_ptr<content::WebContents> owned_dragged_contents =
      browser()->tab_strip_model()->DetachWebContentsAt(1);
  dragged_contents->WasHidden();
  // The other tab in the browser is now foregrounded.
  ExpectNewForegroundedEntry(kBrowserStartUrl);

  // "Drop" the tab into the other browser. This requires showing and
  // reactivating the tab, but to the user, it never leaves the foreground, so
  // we don't log a foregrounded event for it.
  browser_2->tab_strip_model()->InsertWebContentsAt(
      1, std::move(owned_dragged_contents), TabStripModel::ADD_NONE);
  dragged_contents->WasShown();
  browser_2->tab_strip_model()->ActivateTabAt(1, kIsUserGesture);
  EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kFOCEntryName));

  // The first tab in this window was backgrounded when the new one was
  // inserted.
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(
        kEntryName, kBrowser2StartUrl,
        {{TabManager_TabMetrics::kWindowIdName, browser_2->session_id().id()}});
  }

  // Closing the window with 2 tabs means we log the backgrounded tab as closed.
  CloseBrowserSynchronously(browser_2);
  ExpectNewClosedEntry(kBrowser2StartUrl);

  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kEntryName));
  EXPECT_EQ(0, ukm_entry_checker_->NumNewEntriesRecorded(kFOCEntryName));
}

// Tests discarded tab is recorded correctly.
IN_PROC_BROWSER_TEST_F(TabActivityWatcherUkmTest,
                       DiscardedTabGetsPreviousSourceId) {
  ukm::SourceId ukm_source_id_for_tab_0 = 0;
  ukm::SourceId ukm_source_id_for_tab_1 = 0;

  ui_test_utils::NavigateToURL(browser(), test_urls_[0]);
  EXPECT_EQ(0u, ukm_entry_checker_->NumEntries(kEntryName));

  // Adding a new foreground tab logs the previously active tab.
  AddTabAtIndex(1, test_urls_[1], ui::PAGE_TRANSITION_LINK);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(
        kEntryName, test_urls_[0],
        {{TabManager_TabMetrics::kNavigationEntryCountName, 2}});

    ukm_source_id_for_tab_0 = ExpectNewEntryWithSourceId(
        test_urls_[0], kEntryName, 1,
        {{TabManager_TabMetrics::kNavigationEntryCountName, 2}});
  }

  // Discard the first tab.
  content::WebContents* first_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  resource_coordinator::TabLifecycleUnitSource::GetInstance()
      ->GetTabLifecycleUnitExternal(first_contents)
      ->DiscardTab();

  // Switching to first tab logs a forgrounded event for test_urls_[0]
  // and a backgrounded event for test_urls_[1].
  browser()->tab_strip_model()->ActivateTabAt(0, kIsUserGesture);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_->ExpectNewEntry(kEntryName, test_urls_[1],
                                       kBasicMetricValues);

    ukm_source_id_for_tab_1 = ExpectNewEntryWithSourceId(
        test_urls_[1], kEntryName, 2, kBasicMetricValues);

    ExpectNewEntryWithSourceId(
        test_urls_[0], kFOCEntryName, 1,
        {{TabManager_Background_ForegroundedOrClosed::kIsForegroundedName, 1},
         {TabManager_Background_ForegroundedOrClosed::kIsDiscardedName, 1}},
        ukm_source_id_for_tab_0);
  }

  // Discard the second tab.
  content::WebContents* second_content =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  resource_coordinator::TabLifecycleUnitSource::GetInstance()
      ->GetTabLifecycleUnitExternal(second_content)
      ->DiscardTab();

  CloseBrowserSynchronously(browser());
  {
    SCOPED_TRACE("");

    ExpectNewEntryWithSourceId(
        test_urls_[1], kFOCEntryName, 2,
        {{TabManager_Background_ForegroundedOrClosed::kIsForegroundedName, 0},
         {TabManager_Background_ForegroundedOrClosed::kIsDiscardedName, 1}},
        ukm_source_id_for_tab_1);
  }
}

}  // namespace resource_coordinator
