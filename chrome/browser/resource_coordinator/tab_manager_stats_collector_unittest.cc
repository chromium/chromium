// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(42));
  }

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

  void StartBackgroundTabOpeningSession() {
    tab_manager_stats_collector()->OnBackgroundTabOpeningSessionStarted();
  }

  void FinishBackgroundTabOpeningSession() {
    tab_manager_stats_collector()->OnBackgroundTabOpeningSessionEnded();
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

  DISALLOW_COPY_AND_ASSIGN(TabManagerStatsCollectorTest);
};

class TabManagerStatsCollectorParameterizedTest
    : public TabManagerStatsCollectorTest,
      public ::testing::WithParamInterface<
          std::pair<bool,   // should_test_session_restore
                    bool>>  // should_test_background_tab_opening
{
 protected:
  TabManagerStatsCollectorParameterizedTest() = default;
  ~TabManagerStatsCollectorParameterizedTest() override = default;

  void SetUp() override {
    std::tie(should_test_session_restore_,
             should_test_background_tab_opening_) = GetParam();
    TabManagerStatsCollectorTest::SetUp();
  }

  bool IsTestingOverlappedSession() {
    return should_test_session_restore_ && should_test_background_tab_opening_;
  }

  bool should_test_session_restore_;
  bool should_test_background_tab_opening_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabManagerStatsCollectorParameterizedTest);
};

class TabManagerStatsCollectorTabSwitchTest
    : public TabManagerStatsCollectorParameterizedTest {
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
  WebContents* foreground_tab_;
  WebContents* background_tab_;

  DISALLOW_COPY_AND_ASSIGN(TabManagerStatsCollectorTabSwitchTest);
};

TEST_P(TabManagerStatsCollectorTabSwitchTest, HistogramsSwitchToTab) {
  struct HistogramParameters {
    const char* histogram_name;
    bool enabled;
  };
  std::vector<HistogramParameters> histogram_parameters = {
      HistogramParameters{
          TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab,
          should_test_session_restore_},
      HistogramParameters{
          TabManagerStatsCollector::kHistogramBackgroundTabOpeningSwitchToTab,
          should_test_background_tab_opening_},
  };

  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(UNLOADED);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled && !IsTestingOverlappedSession()) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name, UNLOADED, 2);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }

  SetBackgroundTabLoadingState(LOADING);
  SetForegroundTabLoadingState(LOADING);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled && !IsTestingOverlappedSession()) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 5);
      histogram_tester_.ExpectBucketCount(param.histogram_name, UNLOADED, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name, LOADING, 3);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }

  SetBackgroundTabLoadingState(LOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled && !IsTestingOverlappedSession()) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 9);
      histogram_tester_.ExpectBucketCount(param.histogram_name, UNLOADED, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name, LOADING, 3);
      histogram_tester_.ExpectBucketCount(param.histogram_name, LOADED, 4);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }
}

TEST_P(TabManagerStatsCollectorTabSwitchTest, HistogramsTabSwitchLoadTime) {
  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ && !IsTestingOverlappedSession() ? 1 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 1
                                                                           : 0);

  SetBackgroundTabLoadingState(LOADING);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ && !IsTestingOverlappedSession() ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 2
                                                                           : 0);

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
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ && !IsTestingOverlappedSession() ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 2
                                                                           : 0);

  // The count shouldn't change when we're no longer in a session restore or
  // background tab opening.
  if (should_test_session_restore_)
    FinishSessionRestore();
  if (should_test_background_tab_opening_)
    FinishBackgroundTabOpeningSession();

  SetBackgroundTabLoadingState(UNLOADED);
  SetForegroundTabLoadingState(LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ && !IsTestingOverlappedSession() ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 2
                                                                           : 0);
}

TEST_P(TabManagerStatsCollectorParameterizedTest, HistogramsTabCount) {
  auto* stats_collector = tab_manager_stats_collector();

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  stats_collector->TrackNewBackgroundTab(1, 3);
  stats_collector->TrackBackgroundTabLoadAutoStarted();
  stats_collector->TrackBackgroundTabLoadAutoStarted();
  stats_collector->TrackBackgroundTabLoadUserInitiated();
  stats_collector->TrackPausedBackgroundTabs(1);

  if (should_test_session_restore_)
    FinishSessionRestore();
  if (should_test_background_tab_opening_)
    FinishBackgroundTabOpeningSession();

  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabCount,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 1
                                                                           : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabPausedCount,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 1
                                                                           : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningTabLoadAutoStartedCount,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 1
                                                                           : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningTabLoadUserInitiatedCount,
      should_test_background_tab_opening_ && !IsTestingOverlappedSession() ? 1
                                                                           : 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabManagerStatsCollectorTabSwitchTest,
    ::testing::Values(std::make_pair(false,   // Session restore
                                     false),  // Background tab opening
                      std::make_pair(true, false),
                      std::make_pair(false, true),
                      std::make_pair(true, true)));
INSTANTIATE_TEST_SUITE_P(
    All,
    TabManagerStatsCollectorParameterizedTest,
    ::testing::Values(std::make_pair(false,   // Session restore
                                     false),  // Background tab opening
                      std::make_pair(true, false),
                      std::make_pair(false, true),
                      std::make_pair(true, true)));

TEST_F(TabManagerStatsCollectorTest, HistogramsSessionOverlap) {
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore, 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionOverlapBackgroundTabOpening,
      0);

  // Non-overlapping session restore.
  StartSessionRestore();
  FinishSessionRestore();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1)));

  // Non-overlapping background tab opening.
  StartBackgroundTabOpeningSession();
  FinishBackgroundTabOpeningSession();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1)));

  // Overlapping session with session restore before background tab opening.
  StartSessionRestore();
  StartBackgroundTabOpeningSession();
  FinishSessionRestore();
  FinishBackgroundTabOpeningSession();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1), Bucket(true, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1), Bucket(true, 1)));

  // Overlapping session with background tab opening before session restore.
  StartBackgroundTabOpeningSession();
  StartSessionRestore();
  FinishBackgroundTabOpeningSession();
  FinishSessionRestore();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1), Bucket(true, 2)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1), Bucket(true, 2)));

  // Overlapping session with session restore inside background tab opening.
  StartBackgroundTabOpeningSession();
  StartSessionRestore();
  FinishSessionRestore();
  FinishBackgroundTabOpeningSession();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1), Bucket(true, 3)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1), Bucket(true, 3)));

  // Overlapping session with background tab opening inside session restore.
  StartSessionRestore();
  StartBackgroundTabOpeningSession();
  FinishBackgroundTabOpeningSession();
  FinishSessionRestore();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1), Bucket(true, 4)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1), Bucket(true, 4)));

  // Overlapping session with multiple background tab opening sessions
  // inside session restore.
  StartSessionRestore();
  StartBackgroundTabOpeningSession();
  FinishBackgroundTabOpeningSession();
  StartBackgroundTabOpeningSession();
  FinishBackgroundTabOpeningSession();
  FinishSessionRestore();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore),
      ElementsAre(Bucket(false, 1), Bucket(true, 5)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  TabManagerStatsCollector::
                      kHistogramSessionOverlapBackgroundTabOpening),
              ElementsAre(Bucket(false, 1), Bucket(true, 6)));
}

}  // namespace resource_coordinator
