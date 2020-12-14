// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario_data_store.h"

#include <tuple>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);
}  // namespace

class UsageScenarioDataStoreTest : public testing::Test {
 public:
  UsageScenarioDataStoreTest() = default;
  ~UsageScenarioDataStoreTest() override = default;
  UsageScenarioDataStoreTest(const UsageScenarioDataStoreTest& other) = delete;
  UsageScenarioDataStoreTest& operator=(const UsageScenarioDataStoreTest&) =
      delete;

  UsageScenarioDataStoreImpl* data_store() { return &data_store_; }
  const UsageScenarioDataStore::IntervalData& interval_data() {
    return data_store_.GetIntervalDataForTesting();
  }
  UsageScenarioDataStore::IntervalData ResetIntervalData() {
    return data_store_.ResetIntervalData();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  UsageScenarioDataStoreImpl data_store_;
};

TEST_F(UsageScenarioDataStoreTest, Uptime) {
  auto data = ResetIntervalData();
  EXPECT_TRUE(data.uptime_at_interval_end.is_zero());
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.uptime_at_interval_end);
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(2 * kShortDelay, data.uptime_at_interval_end);
}

TEST_F(UsageScenarioDataStoreTest, TabCount) {
  EXPECT_EQ(0U, interval_data().max_tab_count);
  data_store()->OnTabAdded();
  EXPECT_EQ(1U, interval_data().max_tab_count);
  data_store()->OnTabAdded();
  EXPECT_EQ(2U, interval_data().max_tab_count);
  data_store()->OnTabClosed();
  EXPECT_EQ(2U, interval_data().max_tab_count);

  auto data = ResetIntervalData();
  EXPECT_EQ(2U, data.max_tab_count);
  data_store()->OnTabClosed();

  data = ResetIntervalData();
  EXPECT_EQ(1U, data.max_tab_count);
}

TEST_F(UsageScenarioDataStoreTest, TabClosedDuringInterval) {
  EXPECT_EQ(0U, interval_data().max_tab_count);
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  EXPECT_EQ(4U, interval_data().max_tab_count);
  data_store()->OnTabClosed();
  data_store()->OnTabClosed();
  data_store()->OnTabClosed();
  EXPECT_EQ(4U, interval_data().max_tab_count);
  EXPECT_EQ(3U, interval_data().tabs_closed_during_interval);

  auto data = ResetIntervalData();
  EXPECT_EQ(4U, data.max_tab_count);
  EXPECT_EQ(3U, data.tabs_closed_during_interval);
  data_store()->OnTabClosed();

  data = ResetIntervalData();
  EXPECT_EQ(1U, data.max_tab_count);
  EXPECT_EQ(1U, data.tabs_closed_during_interval);
}

TEST_F(UsageScenarioDataStoreTest, VisibleWindowCount) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  EXPECT_EQ(0U, interval_data().max_visible_window_count);
  data_store()->OnWindowVisible();
  EXPECT_EQ(1U, interval_data().max_visible_window_count);
  data_store()->OnWindowVisible();
  EXPECT_EQ(2U, interval_data().max_visible_window_count);
  data_store()->OnWindowHidden();
  EXPECT_EQ(2U, interval_data().max_visible_window_count);

  auto data = ResetIntervalData();
  EXPECT_EQ(2U, data.max_visible_window_count);
  data_store()->OnWindowHidden();

  data = ResetIntervalData();
  EXPECT_EQ(1U, data.max_visible_window_count);
}

TEST_F(UsageScenarioDataStoreTest, TopLevelNavigation) {
  EXPECT_EQ(0U, interval_data().top_level_navigation_count);
  data_store()->OnTopLevelNavigation();
  EXPECT_EQ(1U, interval_data().top_level_navigation_count);
  data_store()->OnTopLevelNavigation();
  EXPECT_EQ(2U, interval_data().top_level_navigation_count);

  auto data = ResetIntervalData();
  EXPECT_EQ(2U, data.top_level_navigation_count);

  data = ResetIntervalData();
  EXPECT_EQ(0U, data.top_level_navigation_count);
}

TEST_F(UsageScenarioDataStoreTest, UserInteraction) {
  EXPECT_EQ(0U, interval_data().user_interaction_count);
  data_store()->OnUserInteraction();
  EXPECT_EQ(1U, interval_data().user_interaction_count);
  data_store()->OnUserInteraction();
  EXPECT_EQ(2U, interval_data().user_interaction_count);

  auto data = ResetIntervalData();
  EXPECT_EQ(2U, data.user_interaction_count);

  data = ResetIntervalData();
  EXPECT_EQ(0U, data.user_interaction_count);
}

TEST_F(UsageScenarioDataStoreTest, FullScreenVideoOnSingleMonitorBasic) {
  data_store()->OnFullScreenVideoStartsOnSingleMonitor();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnFullScreenVideoEndsOnSingleMonitor();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();

  EXPECT_EQ(kShortDelay, data.time_playing_video_full_screen_single_monitor);
}

TEST_F(UsageScenarioDataStoreTest,
       FullScreenVideoOnSingleMonitorOverMultipleIntervals) {
  data_store()->OnFullScreenVideoStartsOnSingleMonitor();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_playing_video_full_screen_single_monitor);

  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_playing_video_full_screen_single_monitor);

  task_environment_.FastForwardBy(kShortDelay / 2);
  data_store()->OnFullScreenVideoEndsOnSingleMonitor();
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay / 2,
            data.time_playing_video_full_screen_single_monitor);
}

TEST_F(UsageScenarioDataStoreTest,
       FullScreenVideoOnSingleMonitorMultipleSessionsDuringInterval) {
  constexpr int kIterations = 2;
  for (int i = 0; i < kIterations; ++i) {
    data_store()->OnFullScreenVideoStartsOnSingleMonitor();
    task_environment_.FastForwardBy(kShortDelay);
    data_store()->OnFullScreenVideoEndsOnSingleMonitor();
    task_environment_.FastForwardBy(kShortDelay);
  }
  auto data = ResetIntervalData();
  task_environment_.FastForwardBy(kShortDelay);

  EXPECT_EQ(kIterations * kShortDelay,
            data.time_playing_video_full_screen_single_monitor);
}

TEST_F(UsageScenarioDataStoreTest, WebRTCUsageBasic) {
  data_store()->OnTabAdded();
  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionClosed();
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();

  EXPECT_EQ(kShortDelay, data.time_with_open_webrtc_connection);
}

TEST_F(UsageScenarioDataStoreTest, WebRTCUsageOverMultipleIntervals) {
  data_store()->OnTabAdded();
  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_with_open_webrtc_connection);

  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_with_open_webrtc_connection);

  task_environment_.FastForwardBy(kShortDelay / 2);
  data_store()->OnWebRTCConnectionClosed();
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay / 2, data.time_with_open_webrtc_connection);
}

TEST_F(UsageScenarioDataStoreTest, WebRTCUsageMultipleSessionsDuringInterval) {
  data_store()->OnTabAdded();
  constexpr int kIterations = 2;
  for (int i = 0; i < kIterations; ++i) {
    data_store()->OnWebRTCConnectionOpened();
    task_environment_.FastForwardBy(kShortDelay);
    data_store()->OnWebRTCConnectionClosed();
    task_environment_.FastForwardBy(kShortDelay);
  }
  auto data = ResetIntervalData();
  task_environment_.FastForwardBy(kShortDelay);

  EXPECT_EQ(kIterations * kShortDelay, data.time_with_open_webrtc_connection);
}

TEST_F(UsageScenarioDataStoreTest, WebRTCUsageInMultipleTabsSingleInterval) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionClosed();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionClosed();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(3 * kShortDelay, data.time_with_open_webrtc_connection);
}

TEST_F(UsageScenarioDataStoreTest, WebRTCUsageInMultipleTabsMultipleInterval) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionOpened();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionClosed();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(3 * kShortDelay, data.time_with_open_webrtc_connection);

  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWebRTCConnectionClosed();

  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_with_open_webrtc_connection);

  task_environment_.FastForwardBy(kShortDelay);

  data = ResetIntervalData();
  EXPECT_EQ(base::TimeDelta(), data.time_with_open_webrtc_connection);
}
