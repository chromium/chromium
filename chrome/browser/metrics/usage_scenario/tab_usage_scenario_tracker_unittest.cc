// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect.h"

namespace metrics {

namespace {

// Inherit from ChromeRenderViewHostTestHarness for access to test profile.
class TabUsageScenarioTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabUsageScenarioTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TabUsageScenarioTrackerTest() override = default;
  TabUsageScenarioTrackerTest(const TabUsageScenarioTrackerTest& other) =
      delete;
  TabUsageScenarioTrackerTest& operator=(const TabUsageScenarioTrackerTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    previous_screen_ = display::Screen::SetScreenInstance(&screen_);
    tab_usage_scenario_tracker_ =
        std::make_unique<TabUsageScenarioTracker>(&usage_scenario_data_store_);
  }

  void TearDown() override {
    tab_usage_scenario_tracker_.reset();
    display::Screen::SetScreenInstance(previous_screen_);
    previous_screen_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    return contents;
  }

 protected:
  display::test::TestScreen screen_;
  display::Screen* previous_screen_;
  UsageScenarioDataStoreImpl usage_scenario_data_store_;
  std::unique_ptr<TabUsageScenarioTracker> tab_usage_scenario_tracker_;
};

}  // namespace

TEST_F(TabUsageScenarioTrackerTest, NewVisibleTabMeansOneVisibleWindow) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // Only one WebContent was shown which means only one visible window.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.GetIntervalDataForTesting();
  EXPECT_EQ(interval_data.max_visible_window_count, 1);
}

TEST_F(TabUsageScenarioTrackerTest, VisibilityUpdateOnVisibleWindowIsNoop) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // Only one WebContent was shown which means only one visible window.
  // The call to OnVisibilityChanged should not create a visible window count
  // higher than the number of windows.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.GetIntervalDataForTesting();
  EXPECT_EQ(interval_data.max_visible_window_count, 1);
}

TEST_F(TabUsageScenarioTrackerTest, HidingWebContentsMakesWindowInvisible) {
  // WebContents starts out visible.
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // WebContents is hidden.
  contents->WasHidden();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents.get());

  // Grab the interval data.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();

  // One WebContents was shown for part of the interval so one visible window.
  EXPECT_EQ(interval_data.max_visible_window_count, 1);

  // End a new interval, no WebContents was shown for the duration so no visible
  // window.
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.max_visible_window_count, 0);
}

TEST_F(TabUsageScenarioTrackerTest, TrackingOfVisibleWebContents) {
  // Start with 2 hidden WebContents.
  auto contents1 = CreateWebContents();
  contents1->WasHidden();
  auto contents2 = CreateWebContents();
  contents2->WasHidden();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);

  // Make one contents visible.
  contents1->WasShown();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents1.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 1);

  // |contents1| is visible, removing it should update the number of currently
  // visible windows.
  tab_usage_scenario_tracker_->OnTabRemoved(contents1.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 1);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);

  tab_usage_scenario_tracker_->OnTabRemoved(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 0);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);
}

TEST_F(TabUsageScenarioTrackerTest, TrackingOfOccludedWebContents) {
  // Start with 2 hidden WebContents.
  auto contents1 = CreateWebContents();
  contents1->WasHidden();
  auto contents2 = CreateWebContents();
  contents2->WasHidden();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);

  // Make one contents occluded.
  contents1->WasOccluded();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents1.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);

  // Make one content visible.
  contents2->WasShown();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 1);

  // Then make it occluded.
  contents2->WasOccluded();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);
}

TEST_F(TabUsageScenarioTrackerTest, FullScreenVideoSingleMonitor) {
  // WebContents starts out visible.
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // WebContents is playing video fullscreen.
  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents.get(), true);

  static constexpr base::TimeDelta kInterval = base::TimeDelta::FromMinutes(2);
  task_environment()->FastForwardBy(kInterval);

  // Grab the interval data.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();

  // Ensure that the time playing a video fullscreen is properly recorded.
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            kInterval);

  // Add a second display, this should stop the fullscreen video on single
  // monitor session.
  int64_t kDisplayID = 42;
  task_environment()->FastForwardBy(kInterval);
  screen_.display_list().AddDisplay({kDisplayID, gfx::Rect(100, 100, 801, 802)},
                                    display::DisplayList::Type::NOT_PRIMARY);
  task_environment()->FastForwardBy(kInterval);

  interval_data = usage_scenario_data_store_.ResetIntervalData();

  // Ensure that the time playing a video fullscreen is properly recorded.
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            kInterval);

  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());

  // Removing the secondary display while there's still some video playing
  // fullscreen should resume the session.
  screen_.display_list().RemoveDisplay(kDisplayID);
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            kInterval);
}

TEST_F(TabUsageScenarioTrackerTest, VideoInVisibleTab) {
  // Create 2 tabs, one visible and one hidden.
  auto contents1 = CreateWebContents();
  auto contents2 = CreateWebContents();
  contents2->WasHidden();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());

  // Pretend that |content1| is playing a video while being visible.
  tab_usage_scenario_tracker_->OnVideoStartedPlaying(contents1.get());

  static constexpr base::TimeDelta kInterval = base::TimeDelta::FromMinutes(2);
  task_environment()->FastForwardBy(kInterval);

  // Grab the interval data and ensure that the time playing a video in the
  // visible tab is properly recorded.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_in_visible_tab, kInterval);

  // Start a video in the hidden tab, while another one is still playing in the
  // visible tab, this shouldn't change anything.
  tab_usage_scenario_tracker_->OnVideoStartedPlaying(contents2.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_in_visible_tab, kInterval);

  // Stop the video playing in the visible tab.
  tab_usage_scenario_tracker_->OnVideoStoppedPlaying(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());

  // There's still a video playing in the second tab, make it visible and ensure
  // that things are reported properly.
  contents2->WasShown();
  tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents2.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_in_visible_tab, kInterval);

  // Mark the tab as playing video for only half of the interval.
  task_environment()->FastForwardBy(kInterval);
  tab_usage_scenario_tracker_->OnVideoStoppedPlaying(contents2.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
}

TEST_F(TabUsageScenarioTrackerTest, VisibleTabPlayingVideoRemoved) {
  auto contents1 = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());

  // Pretend that |content1| is playing a video while being visible.
  tab_usage_scenario_tracker_->OnVideoStartedPlaying(contents1.get());
  static constexpr base::TimeDelta kInterval = base::TimeDelta::FromMinutes(2);
  task_environment()->FastForwardBy(kInterval);
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(kInterval, interval_data.time_playing_video_in_visible_tab);

  tab_usage_scenario_tracker_->OnTabRemoved(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
}

}  // namespace metrics
