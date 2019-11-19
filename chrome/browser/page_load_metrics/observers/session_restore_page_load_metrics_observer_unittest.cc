// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using WebContents = content::WebContents;

class SessionRestorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<SessionRestorePageLoadMetricsObserver>());
  }

 protected:
  using UkmEntry = ukm::builders::
      TabManager_Experimental_SessionRestore_ForegroundTab_PageLoad;

  SessionRestorePageLoadMetricsObserverTest() {}

  void SetUp() override {
    PageLoadMetricsObserverTestHarness::SetUp();

    // Add a default web contents.
    AddForegroundTabWithTester();

    // Create the tab manager to register its SessionRestoreObserver before
    // session restore starts.
    g_browser_process->GetTabManager();

    PopulateFirstPaintTimings();
  }

  void TearDown() override {
    // Must be delete tabs before calling TearDown() which cleans up all the
    // testing environment.
    tabs_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Populate first paint and first [contentful,meaningful] paint timings.
  void PopulateFirstPaintTimings() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromDoubleT(1);
    // Should be large enough (e.g., >20 ms) for some tests to be able to hide
    // foreground tabs before the first pains.
    timing_.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromSeconds(1);
    PopulateRequiredTimingFields(&timing_);
  }

  WebContents* AddForegroundTabWithTester() {
    tabs_.emplace_back(CreateTestWebContents());
    WebContents* contents = tabs_.back().get();
    auto tester =
        std::make_unique<page_load_metrics::PageLoadMetricsObserverTester>(
            contents, this,
            base::BindRepeating(
                &SessionRestorePageLoadMetricsObserverTest::RegisterObservers,
                base::Unretained(this)));
    testers_[contents] = std::move(tester);
    contents->WasShown();
    return contents;
  }

  // Return the default tab.
  WebContents* web_contents() { return tabs_.front().get(); }

  std::vector<std::unique_ptr<WebContents>>& tabs() { return tabs_; }

  void ExpectFirstPaintMetricsTotalCount(int expected_total_count) const {
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstContentfulPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint,
        expected_total_count);
  }

  void RestoreTab(WebContents* contents) {
    SessionRestore::OnWillRestoreTab(contents);

    // Create a restored navigation entry.
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    std::unique_ptr<content::NavigationEntry> entry(
        content::NavigationController::CreateNavigationEntry(
            GetTestURL(), content::Referrer(), base::nullopt,
            ui::PAGE_TRANSITION_RELOAD, false, std::string(), browser_context(),
            nullptr /* blob_url_loader_factory */));
    entries.emplace_back(std::move(entry));

    content::NavigationController& controller = contents->GetController();
    controller.Restore(0, content::RestoreType::LAST_SESSION_EXITED_CLEANLY,
                       &entries);
    ASSERT_EQ(0u, entries.size());
    ASSERT_EQ(1, controller.GetEntryCount());

    EXPECT_TRUE(controller.NeedsReload());
    controller.LoadIfNecessary();
    content::WebContentsTester::For(contents)->CommitPendingNavigation();
  }

  void SimulateTimingUpdateForTab(WebContents* contents) {
    ASSERT_TRUE(base::Contains(testers_, contents));
    testers_[contents]->SimulateTimingUpdate(timing_);
  }

  GURL GetTestURL() const { return GURL("https://google.com"); }

 private:
  base::HistogramTester histogram_tester_;

  page_load_metrics::mojom::PageLoadTiming timing_;
  std::vector<std::unique_ptr<WebContents>> tabs_;
  std::unordered_map<
      WebContents*,
      std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester>>
      testers_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestorePageLoadMetricsObserverTest);
};

TEST_F(SessionRestorePageLoadMetricsObserverTest, NoMetrics) {
  ExpectFirstPaintMetricsTotalCount(0);
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       FirstPaintsOutOfSessionRestore) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GetTestURL(), web_contents()->GetMainFrame());
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(0);
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, RestoreSingleForegroundTab) {
  // Restore one tab which finishes loading in foreground.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(web_contents()));
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(1);
  EXPECT_EQ(1ul, tester()->test_ukm_recorder().entries_count());
  ukm::TestUkmRecorder::ExpectEntryMetric(
      tester()->test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kSessionRestoreTabCountName, 1);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       RestoreMultipleForegroundTabs) {
  AddForegroundTabWithTester();

  // Restore each tab separately.
  for (size_t i = 0; i < tabs().size(); ++i) {
    WebContents* contents = tabs()[i].get();
    ASSERT_NO_FATAL_FAILURE(RestoreTab(contents));
    ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(contents));
    ExpectFirstPaintMetricsTotalCount(i + 1);
    EXPECT_EQ(i + 1, tester()->test_ukm_recorder().entries_count());
    ukm::TestUkmRecorder::ExpectEntryMetric(
        tester()->test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)[i],
        UkmEntry::kSessionRestoreTabCountName, i + 1);
  }
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, RestoreBackgroundTab) {
  // Set the tab to background before the PageLoadMetricsObserver was created.
  web_contents()->WasHidden();

  // Load the restored tab in background.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(web_contents()));
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));

  // No paint timings recorded for tabs restored in background.
  ExpectFirstPaintMetricsTotalCount(0);
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, HideTabBeforeFirstPaints) {
  // Start loading the tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(web_contents()));

  // Hide the tab before any paints.
  web_contents()->WasHidden();

  // No paint timings recorded because tab was hidden before the first paints.
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       SwitchInitialRestoredForegroundTab) {
  // Create 2 tabs: tab 0 is foreground, tab 1 is background.
  AddForegroundTabWithTester();
  tabs()[0]->WasShown();
  tabs()[1]->WasHidden();

  // Restore both tabs.
  for (size_t i = 0; i < tabs().size(); ++i)
    ASSERT_NO_FATAL_FAILURE(RestoreTab(tabs()[i].get()));

  // Switch to tab 1 before any paint events occur.
  tabs()[0]->WasHidden();
  tabs()[1]->WasShown();

  // No paint timings recorded because the initial foreground tab was hidden.
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(0);
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, MultipleSessionRestores) {
  size_t number_of_session_restores = 3;
  for (size_t i = 1; i <= number_of_session_restores; ++i) {
    // NavigationController needs to be unused to restore.
    WebContents* contents = AddForegroundTabWithTester();
    ASSERT_NO_FATAL_FAILURE(RestoreTab(contents));
    ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(contents));

    // Number of paint timings should match the number of session restores.
    ExpectFirstPaintMetricsTotalCount(i);
    EXPECT_EQ(i, tester()->test_ukm_recorder().entries_count());
    ukm::TestUkmRecorder::ExpectEntryMetric(
        tester()->test_ukm_recorder().GetEntriesByName(
            UkmEntry::kEntryName)[i - 1],
        UkmEntry::kSessionRestoreTabCountName, i);
  }
}
