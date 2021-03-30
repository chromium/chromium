// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <memory>
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
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace metrics {

namespace {

class TestTabStatsObserver : public TabStatsObserver {
 public:
  // Functions used to update the window/tab count.
  void OnWindowAdded() override { ++window_count_; }
  void OnWindowRemoved() override {
    EXPECT_GT(window_count_, 0U);
    --window_count_;
  }
  void OnTabAdded(content::WebContents* web_contents) override { ++tab_count_; }
  void OnTabRemoved(content::WebContents* web_contents) override {
    EXPECT_GT(tab_count_, 0U);
    --tab_count_;
  }

  size_t tab_count() { return tab_count_; }

  size_t window_count() { return window_count_; }

 private:
  size_t tab_count_ = 0;
  size_t window_count_ = 0;
};

using TabsStats = TabStatsDataStore::TabsStats;
using TabLifecycleObserver = resource_coordinator::TabLifecycleObserver;

void EnsureTabStatsMatchExpectations(const TabsStats& expected,
                                     const TabsStats& actual) {
  EXPECT_EQ(expected.total_tab_count, actual.total_tab_count);
  EXPECT_EQ(expected.total_tab_count_max, actual.total_tab_count_max);
  EXPECT_EQ(expected.max_tab_per_window, actual.max_tab_per_window);
  EXPECT_EQ(expected.window_count, actual.window_count);
  EXPECT_EQ(expected.window_count_max, actual.window_count_max);
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
  TabStatsTrackerBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
    ASSERT_TRUE(tab_stats_tracker_ != nullptr);
  }

 protected:
  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  TabStatsTracker* tab_stats_tracker_{nullptr};
  std::vector<std::unique_ptr<TestTabStatsObserver>> test_tab_stats_observers_;

  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerBrowserTest);
};

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
                       AdditionalTabStatsObserverGetsInitiliazed) {
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

  test_tab_stats_observers_.push_back(std::make_unique<TestTabStatsObserver>());
  TestTabStatsObserver* first_observer = test_tab_stats_observers_.back().get();
  tab_stats_tracker_->AddObserverAndSetInitialState(first_observer);

  // Observer is initialized properly.
  DCHECK_EQ(first_observer->tab_count(), expected_stats.total_tab_count);
  DCHECK_EQ(first_observer->window_count(), expected_stats.window_count);

  // Add some tabs and windows to increase the counts.
  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  ++expected_stats.total_tab_count;
  ++expected_stats.window_count;

  AddTabAtIndexToBrowser(browser, 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ++expected_stats.total_tab_count;

  test_tab_stats_observers_.push_back(std::make_unique<TestTabStatsObserver>());
  TestTabStatsObserver* second_observer =
      test_tab_stats_observers_.back().get();
  tab_stats_tracker_->AddObserverAndSetInitialState(second_observer);

  // Observer is initialized properly.
  DCHECK_EQ(second_observer->tab_count(), expected_stats.total_tab_count);
  DCHECK_EQ(second_observer->window_count(), expected_stats.window_count);
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

namespace {

class LenientMockTabStatsObserver : public TabStatsObserver {
 public:
  LenientMockTabStatsObserver() = default;
  ~LenientMockTabStatsObserver() override = default;
  LenientMockTabStatsObserver(const LenientMockTabStatsObserver& other) =
      delete;
  LenientMockTabStatsObserver& operator=(const LenientMockTabStatsObserver&) =
      delete;

  MOCK_METHOD0(OnWindowAdded, void());
  MOCK_METHOD0(OnWindowRemoved, void());
  MOCK_METHOD1(OnTabAdded, void(content::WebContents*));
  MOCK_METHOD1(OnTabRemoved, void(content::WebContents*));
  MOCK_METHOD2(OnTabReplaced,
               void(content::WebContents*, content::WebContents*));
  MOCK_METHOD1(OnMainFrameNavigationCommitted, void(content::WebContents*));
  MOCK_METHOD1(OnTabInteraction, void(content::WebContents*));
  MOCK_METHOD1(OnTabIsAudibleChanged, void(content::WebContents*));
  MOCK_METHOD1(OnTabVisibilityChanged, void(content::WebContents*));
  MOCK_METHOD2(OnMediaEffectivelyFullscreenChanged,
               void(content::WebContents*, bool));
};
using MockTabStatsObserver = testing::StrictMock<LenientMockTabStatsObserver>;

}  // namespace

// TODO(1183746): Fix the flakiness on MacOS and re-enable the test.
#if defined(OS_MAC)
#define MAYBE_TabStatsObserverBasics DISABLED_TabStatsObserverBasics
#else
#define MAYBE_TabStatsObserverBasics TabStatsObserverBasics
#endif
IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       MAYBE_TabStatsObserverBasics) {
  MockTabStatsObserver mock_observer;
  TestTabStatsObserver count_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&count_observer);

  auto* window1_tab1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());

  // The browser starts with one window and one visible tab, the observer will
  // be notified immediately about those.
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(window1_tab1));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Mark the tab as hidden.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  window1_tab1->WasHidden();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Make it visible again.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  window1_tab1->WasShown();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Create a second browser window. This will cause one visible tab to be
  // created and its main frame will do a navigation.
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(::testing::_));
  EXPECT_CALL(mock_observer, OnMainFrameNavigationCommitted(::testing::_));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(::testing::_));
  Browser* window2 = CreateBrowser(ProfileManager::GetActiveUserProfile());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Make sure that the 2 windows don't overlap to avoid some unexpected
  // visibility change events because one tab occludes the other.
  // This resizes the two windows so they're right next to each other.
  const gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const gfx::Size size(work_area.width() / 3, work_area.height() / 2);
  gfx::Rect browser_rect(work_area.origin(), size);
  browser()->window()->SetBounds(browser_rect);
  browser_rect.set_x(browser_rect.right());
  window2->window()->SetBounds(browser_rect);

  auto* window2_tab1 = window2->tab_strip_model()->GetWebContentsAt(0);

  // Adding a tab to the second window will cause its previous frame to become
  // hidden.
  EXPECT_CALL(mock_observer, OnTabAdded(::testing::_));
  EXPECT_CALL(mock_observer, OnMainFrameNavigationCommitted(::testing::_));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
  AddTabAtIndexToBrowser(window2, 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  auto* window2_tab2 = window2->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(content::Visibility::HIDDEN, window2_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, window2_tab2->GetVisibility());

  // Make sure that the visibility change events are properly forwarded.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab2));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
  window2->tab_strip_model()->ActivateTabAt(0);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_EQ(content::Visibility::VISIBLE, window2_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::HIDDEN, window2_tab2->GetVisibility());
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
  EXPECT_CALL(mock_observer, OnTabRemoved(window2_tab1));
  EXPECT_CALL(mock_observer, OnTabRemoved(window2_tab2));
  EXPECT_CALL(mock_observer, OnWindowRemoved());
  CloseBrowserSynchronously(window2);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());
  EXPECT_CALL(mock_observer, OnTabRemoved(window1_tab1));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  EXPECT_CALL(mock_observer, OnWindowRemoved());
  CloseBrowserSynchronously(browser());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  tab_stats_tracker_->RemoveObserver(&mock_observer);
  tab_stats_tracker_->RemoveObserver(&count_observer);
  EXPECT_EQ(0U, count_observer.tab_count());
  EXPECT_EQ(0U, count_observer.window_count());
}

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest, TabSwitch) {
  MockTabStatsObserver mock_observer;
  TestTabStatsObserver count_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&count_observer);

  auto* window1_tab1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());

  // The browser starts with one window and one visible tab, the observer will
  // be notified immediately about those.
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(::testing::_));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_CALL(mock_observer, OnTabAdded(::testing::_));
  EXPECT_CALL(mock_observer, OnMainFrameNavigationCommitted(::testing::_));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  AddTabAtIndexToBrowser(browser(), 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_EQ(content::Visibility::HIDDEN, window1_tab1->GetVisibility());
  auto* window1_tab2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab2->GetVisibility());

  // A tab switch should cause 2 visibility change events. The "tab hidden"
  // notification should arrive before the "tab visible" one.
  {
    ::testing::InSequence s;
    EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab2));
    EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
    browser()->tab_strip_model()->ActivateTabAt(0);
  }

  tab_stats_tracker_->RemoveObserver(&mock_observer);
  tab_stats_tracker_->RemoveObserver(&count_observer);
  EXPECT_EQ(2U, count_observer.tab_count());
  EXPECT_EQ(1U, count_observer.window_count());
}

namespace {

// Observes a WebContents and waits until it becomes audible.
// both indicate that they are audible.
class AudioStartObserver : public content::WebContentsObserver {
 public:
  AudioStartObserver(content::WebContents* web_contents,
                     base::OnceClosure quit_closure)
      : content::WebContentsObserver(web_contents),
        quit_closure_(std::move(quit_closure)) {
    DCHECK(!web_contents->IsCurrentlyAudible());
  }
  ~AudioStartObserver() override = default;

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    DCHECK(audible);
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest, AddObserverAudibleTab) {
  // Set up the embedded test server to serve the test javascript file.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the test JS file in the only WebContents.
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/webaudio_oscillator.html")));

  // Start the audio.
  base::RunLoop run_loop;
  AudioStartObserver audio_start_observer(web_contents, run_loop.QuitClosure());
  EXPECT_EQ("OK", EvalJsWithManualReply(web_contents, "StartOscillator();"));
  run_loop.Run();

  // Adding an observer now should receive the OnTabIsAudibleChanged() call.
  MockTabStatsObserver mock_observer;
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(web_contents));
  EXPECT_CALL(mock_observer, OnTabIsAudibleChanged(web_contents));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);

  // Clean up.
  tab_stats_tracker_->RemoveObserver(&mock_observer);
}

}  // namespace metrics
