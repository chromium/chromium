// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

constexpr base::TimeDelta kInterval = base::Minutes(2);

class MockTabUsageScenarioTracker : public TabUsageScenarioTracker {
 public:
  explicit MockTabUsageScenarioTracker(
      UsageScenarioDataStoreImpl* usage_scenario_data_store)
      : TabUsageScenarioTracker(usage_scenario_data_store) {}

  void SetNumDisplays(int num_displays) {
    num_displays_ = num_displays;
    OnNumDisplaysChanged();
  }

  void SetNumDisplaysWithoutNotification(int num_displays) {
    num_displays_ = num_displays;
  }

  int GetNumDisplays() override { return num_displays_; }

 private:
  int num_displays_ = 1;
};

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
    tab_usage_scenario_tracker_ = std::make_unique<MockTabUsageScenarioTracker>(
        &usage_scenario_data_store_);
  }

  void TearDown() override {
    tab_usage_scenario_tracker_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    ukm::InitializeSourceUrlRecorderForWebContents(contents.get());
    return contents;
  }

  void MakeTabVisible(content::WebContents* contents) {
    contents->WasShown();
    tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents);
  }

  void MakeTabHidden(content::WebContents* contents) {
    contents->WasHidden();
    tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents);
  }

  void MakeTabOccluded(content::WebContents* contents) {
    contents->WasOccluded();
    tab_usage_scenario_tracker_->OnTabVisibilityChanged(contents);
  }

  void NavigateAndCommitTab(content::WebContents* contents, const GURL& gurl) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(contents, gurl);
    tab_usage_scenario_tracker_->OnPrimaryMainFrameNavigationCommitted(
        contents);
  }

 protected:
  UsageScenarioDataStoreImpl usage_scenario_data_store_;
  std::unique_ptr<MockTabUsageScenarioTracker> tab_usage_scenario_tracker_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
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
  MakeTabHidden(contents.get());

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
  MakeTabVisible(contents1.get());
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
  MakeTabOccluded(contents1.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 0);

  // Make one content visible.
  MakeTabVisible(contents2.get());
  EXPECT_EQ(usage_scenario_data_store_.current_tab_count_for_testing(), 2);
  EXPECT_EQ(
      usage_scenario_data_store_.current_visible_window_count_for_testing(), 1);

  // Then make it occluded.
  MakeTabOccluded(contents2.get());
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

  task_environment()->FastForwardBy(kInterval);

  // Grab the interval data.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();

  // Ensure that the time playing a video fullscreen is properly recorded.
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            kInterval);

  // Add a second display, this should stop the fullscreen video on single
  // monitor session.
  task_environment()->FastForwardBy(kInterval);
  tab_usage_scenario_tracker_->SetNumDisplays(2);
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
  tab_usage_scenario_tracker_->SetNumDisplays(1);
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            kInterval);
}

// Regression test for crbug.com/1273251.
TEST_F(TabUsageScenarioTrackerTest,
       FullScreenVideoSingleMonitor_StopPlayingWithTwoMonitors) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // `contents` plays video in fullscreen.
  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents.get(), true);
  task_environment()->FastForwardBy(kInterval);

  // Add a second display, this should stop the fullscreen video on single
  // monitor session.
  tab_usage_scenario_tracker_->SetNumDisplays(2);
  task_environment()->FastForwardBy(kInterval);

  // Stop playing video in fullscreen in `contents` while there are 2 displays.
  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents.get(), false);
  task_environment()->FastForwardBy(kInterval);

  // Remove the second display.
  tab_usage_scenario_tracker_->SetNumDisplays(1);
  task_environment()->FastForwardBy(kInterval);

  // `contents2` starts playing video in fullscreen.
  auto contents2 = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());

  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents2.get(), true);
  task_environment()->FastForwardBy(kInterval);

  // Expect 2 * kInterval of video playback (kInterval for each WebContents).
  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            2 * kInterval);
}

// Regression test for crbug.com/341488142.
TEST_F(TabUsageScenarioTrackerTest,
       FullScreenVideoSingleMonitor_NumDisplaysChangeWithoutNotification) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents.get());

  // Add a second display.
  tab_usage_scenario_tracker_->SetNumDisplays(2);

  // `contents` plays video in fullscreen. There are 2 displays so time isn't
  // accumulated in `time_playing_video_full_screen_single_monitor`.
  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents.get(), true);
  task_environment()->FastForwardBy(kInterval);

  // Remove the 2nd display without dispatching a notification yet, to mimic
  // `XDisplayManager`'s delayed notification
  // (https://source.chromium.org/chromium/chromium/src/+/main:ui/base/x/x11_display_manager.cc;l=130-135;drc=90cac1911508d3d682a67c97aa62483eb712f69a).
  tab_usage_scenario_tracker_->SetNumDisplaysWithoutNotification(1);
  task_environment()->FastForwardBy(kInterval);

  // Stop playing video in fullscreen in `contents` while there is 1 display,
  // but the display removal notification hasn't been dispatched yet. This
  // shouldn't crash.
  tab_usage_scenario_tracker_->OnMediaEffectivelyFullscreenChanged(
      contents.get(), false);

  // Expect no time playing video in fullscreen, since the display removal
  // notification was never dispatched.
  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_video_full_screen_single_monitor,
            base::TimeDelta());
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
  EXPECT_EQ(interval_data.time_playing_video_in_visible_tab, kInterval);
}

TEST_F(TabUsageScenarioTrackerTest, VisibleTabPlayingVideoRemoved) {
  auto contents1 = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());

  // Pretend that |content1| is playing a video while being visible.
  tab_usage_scenario_tracker_->OnVideoStartedPlaying(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(kInterval, interval_data.time_playing_video_in_visible_tab);

  tab_usage_scenario_tracker_->OnTabRemoved(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
}

TEST_F(TabUsageScenarioTrackerTest, TabPlayingAudio) {
  // Create a tab and add it.
  auto web_contents = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(web_contents.get());

  // Pretend that web_contents is playing audio.
  content::WebContentsTester::For(web_contents.get())
      ->SetIsCurrentlyAudible(true);
  tab_usage_scenario_tracker_->OnTabIsAudibleChanged(web_contents.get());

  task_environment()->FastForwardBy(kInterval);

  // Grab the interval data and ensure that the time playing a video in the
  // visible tab is properly recorded.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(interval_data.time_playing_audio, kInterval);
}

TEST_F(TabUsageScenarioTrackerTest, UKMVisibility1tab) {
  const GURL kUrl1("https://foo.com/subfoo");
  const GURL kUrl2("https://bar.com/subbar");

  // Test case with only one tab with a navigation that has already been
  // committed when it starts being tracked.
  auto contents1 = CreateWebContents();
  EXPECT_EQ(content::Visibility::VISIBLE, contents1->GetVisibility());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(contents1.get(),
                                                             GURL(kUrl1));
  auto source_id_1 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id_1);

  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  MakeTabHidden(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(ukm::kInvalidSourceId,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(0U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Reloading the tab while it's not visible shouldn't change anything.
  content::NavigationSimulator::Reload(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(ukm::kInvalidSourceId,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(0U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Make the tab visible and navigate to a different URL.
  MakeTabVisible(contents1.get());
  NavigateAndCommitTab(contents1.get(), kUrl2);
  auto source_id_2 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_NE(source_id_1, source_id_2);
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_2, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Reloading the tab while it's visible shouldn't change anything.
  content::NavigationSimulator::Reload(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_2, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());
}

TEST_F(TabUsageScenarioTrackerTest, UKMVisibility1tabLateNavigation) {
  const GURL kUrl1("https://foo.com/subfoo");
  // Test case with only one tab with a navigation that gets committed after
  // starting to track the tab.
  auto contents1 = CreateWebContents();
  EXPECT_EQ(content::Visibility::VISIBLE, contents1->GetVisibility());
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());

  task_environment()->FastForwardBy(kInterval);
  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(ukm::kInvalidSourceId,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_TRUE(
      interval_data.source_id_for_longest_visible_origin_duration.is_zero());
  EXPECT_EQ(0U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  NavigateAndCommitTab(contents1.get(), kUrl1);
  auto source_id_1 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id_1);

  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());
}

TEST_F(TabUsageScenarioTrackerTest, UKMVisibilityMultipleTabs) {
  const GURL kUrl1("https://foo.com/subfoo");
  const GURL kUrl2("https://bar.com/subbar");
  const GURL kUrl3("https://foo.com/foo2");

  // Create 3 tabs: one is visible, one is hidden and one is occluded.
  auto contents1 = CreateWebContents();
  auto contents2 = CreateWebContents();
  auto contents3 = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents3.get());
  EXPECT_EQ(content::Visibility::VISIBLE, contents1->GetVisibility());
  MakeTabHidden(contents2.get());
  MakeTabOccluded(contents3.get());

  NavigateAndCommitTab(contents1.get(), kUrl1);
  NavigateAndCommitTab(contents2.get(), kUrl2);
  NavigateAndCommitTab(contents3.get(), kUrl3);

  task_environment()->FastForwardBy(kInterval);
  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  auto source_id_1 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Make a second tab visible and make the hidden one occluded.
  task_environment()->FastForwardBy(kInterval);
  MakeTabVisible(contents2.get());
  MakeTabHidden(contents3.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(2 * kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(2U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Mark the first tab as hidden.
  MakeTabOccluded(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  auto source_id_2 = contents2->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_EQ(source_id_2, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());

  // Finally, mark the third tab as visible.
  MakeTabHidden(contents2.get());
  MakeTabVisible(contents3.get());
  task_environment()->FastForwardBy(kInterval);
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  auto source_id_3 = contents3->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_EQ(source_id_3, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(1U,
            usage_scenario_data_store_.GetVisibleSourceIdsForTesting().size());
}

TEST_F(TabUsageScenarioTrackerTest, UKMVisibilityMultipleVisibilityEvents) {
  const GURL kUrl1("https://foo.com/subfoo");

  auto contents1 = CreateWebContents();
  EXPECT_EQ(content::Visibility::VISIBLE, contents1->GetVisibility());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(contents1.get(),
                                                             GURL(kUrl1));
  auto source_id_1 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id_1);
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());

  task_environment()->FastForwardBy(kInterval);
  MakeTabHidden(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  MakeTabVisible(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  MakeTabHidden(contents1.get());
  task_environment()->FastForwardBy(kInterval);

  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_1, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(2 * kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

TEST_F(TabUsageScenarioTrackerTest,
       UKMVisibilityMultipleVisibleTabsSameOrigin) {
  const GURL kUrl1("https://foo.com/subfoo");
  const GURL kUrl2("https://bar.com/subbar");
  const GURL kUrl3("https://foo.com/foo2");

  // Create 3 tabs: one is visible, one is hidden and one is occluded.
  auto contents1 = CreateWebContents();
  auto contents2 = CreateWebContents();
  auto contents3 = CreateWebContents();
  tab_usage_scenario_tracker_->OnTabAdded(contents1.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents2.get());
  tab_usage_scenario_tracker_->OnTabAdded(contents3.get());
  EXPECT_EQ(content::Visibility::VISIBLE, contents1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, contents2->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, contents3->GetVisibility());

  NavigateAndCommitTab(contents1.get(), kUrl1);
  NavigateAndCommitTab(contents2.get(), kUrl2);
  NavigateAndCommitTab(contents3.get(), kUrl3);

  auto source_id_1 = contents1->GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto source_id_2 = contents2->GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto source_id_3 = contents3->GetPrimaryMainFrame()->GetPageUkmSourceId();
  EXPECT_NE(source_id_1, source_id_2);
  EXPECT_NE(source_id_1, source_id_3);
  EXPECT_NE(source_id_2, source_id_3);

  task_environment()->FastForwardBy(kInterval * 2);
  MakeTabHidden(contents1.get());
  task_environment()->FastForwardBy(kInterval);
  MakeTabHidden(contents3.get());
  task_environment()->FastForwardBy(kInterval);
  MakeTabHidden(contents2.get());

  // contents1 visible for 2*kInterval
  // contents2 visible for 4*kInterval
  // contents3 visible for 3*kInterval
  // The origin foo.com was visible for the longest time (5*kInterval).
  // source_id_3 is the source id that was visible for the longest time for
  // origin foo.com. Therefore, expect:
  //     source_id_for_longest_visible_origin = source_id_3
  //     source_id_for_longest_visible_origin_duration = 3*kInterval

  auto interval_data = usage_scenario_data_store_.ResetIntervalData();
  EXPECT_EQ(source_id_3, interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(3 * kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

}  // namespace metrics
