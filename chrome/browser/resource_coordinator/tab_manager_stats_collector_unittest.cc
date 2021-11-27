// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using base::Bucket;
using ::testing::ElementsAre;

using WebContents = content::WebContents;

namespace resource_coordinator {

using LoadingState = TabLoadTracker::LoadingState;

constexpr TabLoadTracker::LoadingState UNLOADED = LoadingState::UNLOADED;
constexpr TabLoadTracker::LoadingState LOADING = LoadingState::LOADING;
constexpr TabLoadTracker::LoadingState LOADED = LoadingState::LOADED;

class TabManagerStatsCollectorTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabManagerStatsCollectorTest()
      : scoped_context_(
            std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
                task_runner_)),
        scoped_set_tick_clock_for_testing_(task_runner_->GetMockTickClock()) {
    base::CurrentThread::Get()->SetTaskRunner(task_runner_);

    // Start with a non-zero time.
    task_runner_->FastForwardBy(base::Seconds(42));
  }

  TabManagerStatsCollectorTest(const TabManagerStatsCollectorTest&) = delete;
  TabManagerStatsCollectorTest& operator=(const TabManagerStatsCollectorTest&) =
      delete;

  ~TabManagerStatsCollectorTest() override = default;

  TabManagerStatsCollector* tab_manager_stats_collector() {
    return tab_manager()->stats_collector();
  }

  void StartSessionRestore() {
    tab_manager()->OnSessionRestoreStartedLoadingTabs();
    tab_manager_stats_collector()->OnSessionRestoreStartedLoadingTabs();
  }

  void FinishSessionRestore() {
    tab_manager()->OnSessionRestoreFinishedLoadingTabs();
    tab_manager_stats_collector()->OnSessionRestoreFinishedLoadingTabs();
  }

  TabManager::WebContentsData* GetWebContentsData(WebContents* contents) const {
    return tab_manager()->GetWebContentsData(contents);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Call the tab manager so that it is created right away.
    tab_manager();
  }

  void TearDown() override {
    task_runner_->RunUntilIdle();
    scoped_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext> scoped_context_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

 private:
  TabManager* tab_manager() const { return g_browser_process->GetTabManager(); }
};

class TabManagerStatsCollectorTabSwitchTest
    : public TabManagerStatsCollectorTest {
 public:
  TabManagerStatsCollectorTabSwitchTest(
      const TabManagerStatsCollectorTabSwitchTest&) = delete;
  TabManagerStatsCollectorTabSwitchTest& operator=(
      const TabManagerStatsCollectorTabSwitchTest&) = delete;

 protected:
  TabManagerStatsCollectorTabSwitchTest() = default;
  ~TabManagerStatsCollectorTabSwitchTest() override = default;

  void SetForegroundTabLoadingState(LoadingState state) {
    GetWebContentsData(foreground_tab_)->SetTabLoadingState(state);
  }

  void SetBackgroundTabLoadingState(LoadingState state) {
    GetWebContentsData(background_tab_)->SetTabLoadingState(state);
  }

  // Creating and destroying the WebContentses need to be done in the test
  // scope.
  void SetForegroundAndBackgroundTabs(WebContents* foreground_tab,
                                      WebContents* background_tab) {
    foreground_tab_ = foreground_tab;
    foreground_tab_->WasShown();
    background_tab_ = background_tab;
    background_tab_->WasHidden();
  }

  void FinishLoadingForegroundTab() {
    SetForegroundTabLoadingState(LOADED);
    tab_manager_stats_collector()->OnTabIsLoaded(foreground_tab_);
  }

  void FinishLoadingBackgroundTab() {
    SetBackgroundTabLoadingState(LOADED);
    tab_manager_stats_collector()->OnTabIsLoaded(background_tab_);
  }

  void SwitchToBackgroundTab() {
    tab_manager_stats_collector()->RecordSwitchToTab(foreground_tab_,
                                                     background_tab_);
    SetForegroundAndBackgroundTabs(background_tab_, foreground_tab_);
  }

 private:
  raw_ptr<WebContents> foreground_tab_;
  raw_ptr<WebContents> background_tab_;
};

TEST_F(TabManagerStatsCollectorTabSwitchTest, HistogramsSwitchToTab) {
  auto* histogram_name =
      TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab;

  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  StartSessionRestore();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(UNLOADED);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  histogram_tester_.ExpectTotalCount(histogram_name, 2);
  histogram_tester_.ExpectBucketCount(histogram_name, UNLOADED, 2);

  SetBackgroundTabLoadingState(LOADING);
  SetForegroundTabLoadingState(LOADING);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  histogram_tester_.ExpectTotalCount(histogram_name, 5);
  histogram_tester_.ExpectBucketCount(histogram_name, UNLOADED, 2);
  histogram_tester_.ExpectBucketCount(histogram_name, LOADING, 3);

  SetBackgroundTabLoadingState(LOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  histogram_tester_.ExpectTotalCount(histogram_name, 9);
  histogram_tester_.ExpectBucketCount(histogram_name, UNLOADED, 2);
  histogram_tester_.ExpectBucketCount(histogram_name, LOADING, 3);
  histogram_tester_.ExpectBucketCount(histogram_name, LOADED, 4);
}

TEST_F(TabManagerStatsCollectorTabSwitchTest, HistogramsTabSwitchLoadTime) {
  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  StartSessionRestore();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 1);

  SetBackgroundTabLoadingState(LOADING);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 2);

  // Metrics aren't recorded when the foreground tab has not finished loading
  // and the user switches to a different tab.
  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  // Foreground tab is currently loading and being tracked.
  SwitchToBackgroundTab();
  // The previous foreground tab is no longer tracked.
  FinishLoadingBackgroundTab();
  SwitchToBackgroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 2);

  // The count shouldn't change when we're no longer in a session restore or
  // background tab opening.
  FinishSessionRestore();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 2);
}

class TabManagerStatsCollectorPrerenderingTest
    : public TabManagerStatsCollectorTabSwitchTest {
 public:
  TabManagerStatsCollectorPrerenderingTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~TabManagerStatsCollectorPrerenderingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that prerendering loading doesn't remove its WebContents from
// `foreground_contents_switched_to_times_` and doesn't add the histogram
// since it's not a primary page.
TEST_F(TabManagerStatsCollectorPrerenderingTest,
       KeepingWebContentsMapInPrerendering) {
  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());

  GURL init_url("https://example1.test/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(tab2.get(),
                                                             init_url);

  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  StartSessionRestore();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();

  // Set prerendering loading.
  const GURL kPrerenderingUrl("https://example1.test/?prerendering");
  auto* prerender_rfh = content::WebContentsTester::For(tab2.get())
                            ->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  DCHECK_NE(prerender_rfh, nullptr);

  // Even though prerendering navigation is committed, TabManagerStatsCollector
  // should keep `tab2` at `foreground_contents_switched_to_times_`.
  EXPECT_TRUE(base::Contains(
      tab_manager_stats_collector()->foreground_contents_switched_to_times_,
      tab2.get()));
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 0);

  FinishLoadingForegroundTab();
  // `tab2` is removed from `foreground_contents_switched_to_times_` in
  // OnTabIsLoaded().
  EXPECT_FALSE(base::Contains(
      tab_manager_stats_collector()->foreground_contents_switched_to_times_,
      tab2.get()));
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime, 1);
}

}  // namespace resource_coordinator
