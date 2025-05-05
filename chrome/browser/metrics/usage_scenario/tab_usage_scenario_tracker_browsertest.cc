// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40751070): More tests should be added to cover all possible
// scenarios. E.g. a test closing the visible tab in a window should be added.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

constexpr base::TimeDelta kInterval = base::Minutes(2);

void DiscardTab(content::WebContents* contents) {
  resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
      contents)
      ->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);
}

// A WebContentsObserver that allows waiting for some media to start or stop
// playing fullscreen.
class FullscreenEventsWaiter : public content::WebContentsObserver {
 public:
  explicit FullscreenEventsWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    playing_media_fullscreen_ =
        web_contents->IsFullscreen() &&
        web_contents->HasActiveEffectivelyFullscreenVideo();
  }
  FullscreenEventsWaiter(const FullscreenEventsWaiter& rhs) = delete;
  FullscreenEventsWaiter& operator=(const FullscreenEventsWaiter& rhs) = delete;
  ~FullscreenEventsWaiter() override = default;

  void MediaEffectivelyFullscreenChanged(bool value) override {
    playing_media_fullscreen_ = value;
    if (run_loop_) {
      EXPECT_TRUE(playing_media_fullscreen_expected_value_.has_value());
      if (playing_media_fullscreen_ ==
          playing_media_fullscreen_expected_value_.value()) {
        playing_media_fullscreen_expected_value_.reset();
        run_loop_->Quit();
      }
    }
  }

  // Wait for the current media playing fullscreen mode to be equal to
  // |expected_media_fullscreen_mode|.
  void Wait(bool expected_media_fullscreen_mode) {
    if (expected_media_fullscreen_mode == playing_media_fullscreen_)
      return;

    playing_media_fullscreen_expected_value_ = expected_media_fullscreen_mode;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool playing_media_fullscreen_ = false;
  std::optional<bool> playing_media_fullscreen_expected_value_ = false;
};

class MediaWaiter : public content::WebContentsObserver {
 public:
  explicit MediaWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override {
    started_media_id_ = id;
    media_started_playing_loop_.Quit();
  }
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override {
    EXPECT_EQ(id, started_media_id_);
    media_stopped_playing_loop_.Quit();
  }
  void OnAudioStateChanged(bool audible) override {
    if (audible) {
      audio_started_playing_loop_.Quit();
    } else {
      audio_stopped_playing_loop_.Quit();
    }
  }

  void WaitForMediaStartedPlaying() { media_started_playing_loop_.Run(); }
  void WaitForMediaStoppedPlaying() { media_stopped_playing_loop_.Run(); }

  void WaitForAudioStartedPlaying() { audio_started_playing_loop_.Run(); }
  void WaitForAudioStoppedPlaying() { audio_stopped_playing_loop_.Run(); }

 private:
  std::optional<content::MediaPlayerId> started_media_id_;

  base::RunLoop media_started_playing_loop_;
  base::RunLoop media_stopped_playing_loop_;

  base::RunLoop audio_started_playing_loop_;
  base::RunLoop audio_stopped_playing_loop_;
};

}  // namespace

class TabUsageScenarioTrackerBrowserTest : public InProcessBrowserTest {
 public:
  TabUsageScenarioTrackerBrowserTest() : data_store_(&tick_clock_) {
    // Ensure that |tick_clock_.NowTicks()| doesn't return 0 the first time it
    // gets called.
    tick_clock_.SetNowTicks(base::TimeTicks::Now());
  }
  TabUsageScenarioTrackerBrowserTest(
      const TabUsageScenarioTrackerBrowserTest& rhs) = delete;
  TabUsageScenarioTrackerBrowserTest& operator=(
      const TabUsageScenarioTrackerBrowserTest& rhs) = delete;
  ~TabUsageScenarioTrackerBrowserTest() override = default;

  void SetUp() override {
    // This is required for the fullscreen video tests.
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
    ASSERT_TRUE(tab_stats_tracker_);
    tab_usage_scenario_tracker_ =
        std::make_unique<TabUsageScenarioTracker>(&data_store_);
    tab_stats_tracker_->AddObserverAndSetInitialState(
        tab_usage_scenario_tracker_.get());
  }

  void TearDownOnMainThread() override {
    tab_stats_tracker_->RemoveObserver(tab_usage_scenario_tracker_.get());
    tab_usage_scenario_tracker_.reset();
    tab_stats_tracker_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::SimpleTestTickClock tick_clock_;
  raw_ptr<TabStatsTracker> tab_stats_tracker_{nullptr};
  UsageScenarioDataStoreImpl data_store_;
  std::unique_ptr<TabUsageScenarioTracker> tab_usage_scenario_tracker_;
};

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest, BasicNavigations) {
  // Test with only one visible tab and one top level navigation.
  auto* content0 = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(content::NavigateToURL(
      content0, embedded_test_server()->GetURL("/title1.html")));
  tick_clock_.Advance(kInterval);
  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  // The navigation via content::NavigateToURL counts as an interaction.
  EXPECT_EQ(1U, interval_data.user_interaction_count);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetPageUkmSourceId(),
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Add a second tab that will become the visible one.
  tick_clock_.Advance(kInterval);
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/title2.html"),
                            ui::PAGE_TRANSITION_LINK));
  auto* contents1 = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(
      content::Visibility::VISIBLE,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibility());
  EXPECT_EQ(content::Visibility::HIDDEN,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetVisibility());
  tick_clock_.Advance(kInterval * 2);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(0U, interval_data.user_interaction_count);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(contents1->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Activate the first tab and close it.
  browser()->tab_strip_model()->ActivateTabAt(0);
  tick_clock_.Advance(kInterval * 2);
  auto expected_source_id = browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(1U, interval_data.tabs_closed_during_interval);
  // TODO(sebmarchand): Check if closing a tab via CLOSE_USER_GESTURE counts as
  // a user interaction.
  EXPECT_EQ(0U, interval_data.user_interaction_count);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);

  // There's only one visible tab remaining.
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(0U, interval_data.user_interaction_count);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(contents1->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest, TabCrash) {
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/title2.html"),
                            ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(content::Visibility::VISIBLE,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibility());
  tick_clock_.Advance(kInterval);
  auto expected_source_id = browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Induce a crash in the active tab.
  tick_clock_.Advance(kInterval);
  content::CrashTab(browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_TRUE(browser()->tab_strip_model()->GetWebContentsAt(1)->IsCrashed());
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

class TabUsageScenarioTrackerDiscardBrowserTest
    : public TabUsageScenarioTrackerBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  TabUsageScenarioTrackerDiscardBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabUsageScenarioTrackerDiscardBrowserTest, TabDiscard) {
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/title2.html"),
                            ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(content::Visibility::VISIBLE,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibility());
  tick_clock_.Advance(kInterval);

  auto interval_data = data_store_.ResetIntervalData();
  auto expected_source_id = browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Induce a discard of the active tab.
  tick_clock_.Advance(kInterval * 2);
  DiscardTab(browser()->tab_strip_model()->GetWebContentsAt(1));
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Do a navigation on the discarded tab.
  EXPECT_TRUE(
      content::NavigateToURL(browser()->tab_strip_model()->GetWebContentsAt(1),
                             embedded_test_server()->GetURL("/title2.html")));
  expected_source_id = browser()
                           ->tab_strip_model()
                           ->GetWebContentsAt(1)
                           ->GetPrimaryMainFrame()
                           ->GetPageUkmSourceId();
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Same tests but with this time the discarded tab is hidden.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(content::Visibility::VISIBLE,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetVisibility());
  tick_clock_.Advance(kInterval * 2);
  DiscardTab(browser()->tab_strip_model()->GetWebContentsAt(1));
  tick_clock_.Advance(kInterval);
  expected_source_id = browser()
                           ->tab_strip_model()
                           ->GetWebContentsAt(0)
                           ->GetPrimaryMainFrame()
                           ->GetPageUkmSourceId();
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 3,
            interval_data.source_id_for_longest_visible_origin_duration);

  // Do a navigation on the discarded tab.
  EXPECT_TRUE(
      content::NavigateToURL(browser()->tab_strip_model()->GetWebContentsAt(1),
                             embedded_test_server()->GetURL("/title2.html")));
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_P(TabUsageScenarioTrackerDiscardBrowserTest,
                       VisibleTabVideoDiscarded) {
  // Start a video in a tab and discard it while it's playing, ensure that
  // things are tracked properly.
  EXPECT_TRUE(
      content::NavigateToURL(browser()->tab_strip_model()->GetWebContentsAt(0),
                             embedded_test_server()->GetURL("/title2.html")));
  tick_clock_.Advance(kInterval);
  ASSERT_TRUE(AddTabAtIndex(
      1, embedded_test_server()->GetURL("/media/session/media-session.html"),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(content::Visibility::VISIBLE,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibility());

  auto* media_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  MediaWaiter media_waiter(media_contents);
  EXPECT_TRUE(content::ExecJs(
      media_contents, "document.getElementById('long-video-loop').play();"));
  media_waiter.WaitForMediaStartedPlaying();
  EXPECT_TRUE(data_store_.TrackingPlayingVideoInActiveTabForTesting());

  auto expected_source_id =
      media_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Discard the tab, the data store's visible tab video timer should be reset
  // by the tracker.
  tick_clock_.Advance(kInterval * 2);
  DiscardTab(browser()->tab_strip_model()->GetWebContentsAt(1));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !data_store_.TrackingPlayingVideoInActiveTabForTesting();
  }));

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(2U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_EQ(kInterval * 2, interval_data.time_playing_video_in_visible_tab);
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);
}

// TODO(crbug.com/368253760): Fix the flakiness on Windows and re-enable the
// test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FullScreenVideoDiscarded DISABLED_FullScreenVideoDiscarded
#else
#define MAYBE_FullScreenVideoDiscarded FullScreenVideoDiscarded
#endif
IN_PROC_BROWSER_TEST_P(TabUsageScenarioTrackerDiscardBrowserTest,
                       MAYBE_FullScreenVideoDiscarded) {
  // Play full screen video in a tab and discard it while it's playing, ensure
  // that things are tracked properly.
  EXPECT_TRUE(
      content::NavigateToURL(browser()->tab_strip_model()->GetWebContentsAt(0),
                             embedded_test_server()->GetURL("/title2.html")));
  tick_clock_.Advance(kInterval);
  ASSERT_TRUE(
      AddTabAtIndex(1, embedded_test_server()->GetURL("/media/fullscreen.html"),
                    ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(content::Visibility::VISIBLE,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibility());

  auto* fullscreen_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  FullscreenEventsWaiter fullscreen_waiter(fullscreen_contents);
  EXPECT_TRUE(
      content::ExecJs(fullscreen_contents, "makeFullscreen('small_video')"));
  fullscreen_waiter.Wait(true);
  EXPECT_TRUE(
      data_store_.TrackingPlayingFullScreenVideoSingleMonitorForTesting());

  auto expected_source_id =
      fullscreen_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Discard the tab, the data store's full screen video timer should be reset
  // by the tracker.
  tick_clock_.Advance(kInterval * 2);
  DiscardTab(browser()->tab_strip_model()->GetWebContentsAt(1));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !data_store_.TrackingPlayingFullScreenVideoSingleMonitorForTesting();
  }));

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(2U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(kInterval * 2,
            interval_data.time_playing_video_full_screen_single_monitor);
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest, FullScreenVideo) {
  // Play fullscreen video in a tab, ensure that things are tracked properly.
  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  FullscreenEventsWaiter waiter(contents);
  EXPECT_TRUE(content::NavigateToURL(
      contents, embedded_test_server()->GetURL("/media/fullscreen.html")));
  EXPECT_TRUE(content::ExecJs(contents, "makeFullscreen('small_video')"));
  waiter.Wait(true);
  tick_clock_.Advance(kInterval);
  EXPECT_TRUE(content::ExecJs(contents, "exitFullscreen()"));
  waiter.Wait(false);
  auto expected_source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(kInterval,
            interval_data.time_playing_video_full_screen_single_monitor);
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  // The |time_playing_video_in_visible_tab| value is not currently being
  // tracked.
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest, VisibleTabVideo) {
  // Play video in a tab, ensure that things are tracked properly.
  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  MediaWaiter waiter(contents);
  EXPECT_TRUE(content::NavigateToURL(
      contents,
      embedded_test_server()->GetURL("/media/session/media-session.html")));
  EXPECT_TRUE(content::ExecJs(
      contents, "document.getElementById('long-video-loop').play();"));
  waiter.WaitForMediaStartedPlaying();
  tick_clock_.Advance(kInterval);
  EXPECT_TRUE(content::ExecJs(
      contents, "document.getElementById('long-video-loop').pause();"));
  waiter.WaitForMediaStoppedPlaying();
  auto expected_source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_EQ(kInterval, interval_data.time_playing_video_in_visible_tab);
  EXPECT_TRUE(interval_data.time_playing_audio.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest, TabAudio) {
  // Play audio in a tab, ensure that things are tracked properly.
  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  MediaWaiter waiter(contents);
  EXPECT_TRUE(content::NavigateToURL(
      contents,
      embedded_test_server()->GetURL("/media/session/media-session.html")));
  EXPECT_TRUE(content::ExecJs(contents,
                              "document.getElementById('long-audio').play();"));
  waiter.WaitForAudioStartedPlaying();
  tick_clock_.Advance(kInterval);
  EXPECT_TRUE(content::ExecJs(
      contents, "document.getElementById('long-audio').pause();"));
  waiter.WaitForAudioStoppedPlaying();
  auto expected_source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(kInterval, interval_data.time_playing_audio);
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

// TODO(crbug.com/40752198): Fix the flakiness on MacOS and re-enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FullScreenVideoClosed DISABLED_FullScreenVideoClosed
#else
#define MAYBE_FullScreenVideoClosed FullScreenVideoClosed
#endif
IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest,
                       MAYBE_FullScreenVideoClosed) {
  // Play fullscreen video in a tab and close it while it's playing, ensure that
  // things are tracked properly.
  EXPECT_TRUE(
      content::NavigateToURL(browser()->tab_strip_model()->GetWebContentsAt(0),
                             embedded_test_server()->GetURL("/title2.html")));
  tick_clock_.Advance(kInterval);
  ASSERT_TRUE(
      AddTabAtIndex(1, embedded_test_server()->GetURL("/media/fullscreen.html"),
                    ui::PAGE_TRANSITION_LINK));
  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  FullscreenEventsWaiter waiter(contents);
  EXPECT_TRUE(content::ExecJs(contents, "makeFullscreen('small_video')"));
  waiter.Wait(true);
  tick_clock_.Advance(kInterval * 2);
  auto expected_source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(2U, interval_data.top_level_navigation_count);
  EXPECT_EQ(1U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(kInterval * 2,
            interval_data.time_playing_video_full_screen_single_monitor);
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval * 2,
            interval_data.source_id_for_longest_visible_origin_duration);

  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  expected_source_id = browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetPrimaryMainFrame()
                           ->GetPageUkmSourceId();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

// TODO(crbug.com/40752198): Fix the flakiness on MacOS and re-enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FullScreenVideoCrash DISABLED_FullScreenVideoCrash
#else
#define MAYBE_FullScreenVideoCrash FullScreenVideoCrash
#endif
IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest,
                       MAYBE_FullScreenVideoCrash) {
  // Play fullscreen video in a tab and make the tab crash, ensure that things
  // are tracked properly.
  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(content::NavigateToURL(
      contents, embedded_test_server()->GetURL("/media/fullscreen.html")));
  FullscreenEventsWaiter waiter(contents);
  EXPECT_TRUE(content::ExecJs(contents, "makeFullscreen('small_video')"));
  waiter.Wait(true);
  tick_clock_.Advance(kInterval);
  auto expected_source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  content::CrashTab(contents);

  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(kInterval,
            interval_data.time_playing_video_full_screen_single_monitor);
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);

  EXPECT_TRUE(content::NavigateToURL(
      contents, embedded_test_server()->GetURL("/title2.html")));
  expected_source_id = contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  tick_clock_.Advance(kInterval);
  interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(1U, interval_data.max_tab_count);
  EXPECT_EQ(1U, interval_data.max_visible_window_count);
  EXPECT_EQ(1U, interval_data.top_level_navigation_count);
  EXPECT_EQ(0U, interval_data.tabs_closed_during_interval);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(expected_source_id,
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

IN_PROC_BROWSER_TEST_F(TabUsageScenarioTrackerBrowserTest,
                       InitialVisibleNotification) {
  // This test causes a WebContents::OnVisibilityChanged(VISIBLE) signal to be
  // emitted for a tab that was already visible when adding it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser2 = BrowserList::GetInstance()->get(1);

  int previous_browser1_tab_count = browser()->tab_strip_model()->count();
  int previous_browser2_tab_count = browser2->tab_strip_model()->count();
  browser2->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_browser1_tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(previous_browser2_tab_count - 1,
            browser2->tab_strip_model()->count());

  tick_clock_.Advance(kInterval);
  auto interval_data = data_store_.ResetIntervalData();
  EXPECT_EQ(2U, interval_data.max_tab_count);
  EXPECT_EQ(2U, interval_data.max_visible_window_count);
  EXPECT_EQ(0U, interval_data.top_level_navigation_count);
  EXPECT_EQ(1U, interval_data.tabs_closed_during_interval);
  EXPECT_EQ(0U, interval_data.user_interaction_count);
  EXPECT_TRUE(
      interval_data.time_playing_video_full_screen_single_monitor.is_zero());
  EXPECT_TRUE(interval_data.time_with_open_webrtc_connection.is_zero());
  EXPECT_TRUE(interval_data.time_playing_video_in_visible_tab.is_zero());
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetPageUkmSourceId(),
            interval_data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kInterval,
            interval_data.source_id_for_longest_visible_origin_duration);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TabUsageScenarioTrackerDiscardBrowserTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        TabUsageScenarioTrackerDiscardBrowserTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

}  // namespace metrics
