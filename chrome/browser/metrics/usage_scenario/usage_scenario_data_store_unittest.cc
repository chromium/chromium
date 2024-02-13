// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"

#include <tuple>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta kShortDelay = base::Seconds(1);
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

TEST_F(UsageScenarioDataStoreTest, TimeSinceLastInteractionWithBrowser) {
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_since_last_user_interaction_with_browser);

  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(2 * kShortDelay,
            data.time_since_last_user_interaction_with_browser);

  data_store()->OnUserInteraction();
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_since_last_user_interaction_with_browser);
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

TEST_F(UsageScenarioDataStoreTest, VisibleSourceIDsDuringIntervalSingleURL) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.com"));

  // Interval with no visible SourceID.
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();
  EXPECT_EQ(ukm::kInvalidSourceId, data.source_id_for_longest_visible_origin);
  EXPECT_TRUE(data.source_id_for_longest_visible_origin_duration.is_zero());

  // Interval with one SourceID visible the entire time.
  const ukm::SourceId kSource1 = 42;
  data_store()->OnUkmSourceBecameVisible(kSource1, kOrigin, {});
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kSource1, data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kShortDelay, data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(kShortDelay, data.longest_visible_origin_duration);

  // Interval with 2 different visible SourceID, |kSource1| is visible the
  // longest.
  const ukm::SourceId kSource2 = 43;
  task_environment_.FastForwardBy(2 * kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kSource1, kOrigin);
  data_store()->OnUkmSourceBecameVisible(kSource2, kOrigin, {});
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kSource1, data.source_id_for_longest_visible_origin);
  EXPECT_EQ(2 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(3 * kShortDelay, data.longest_visible_origin_duration);

  // Interval with 3 different visible SourceID, |kSource1| and |kSource2| are
  // visible for the same amount of time.
  data_store()->OnUkmSourceBecameVisible(kSource1, kOrigin, {});
  const ukm::SourceId kSource3 = 44;
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameVisible(kSource3, kOrigin, {});
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_TRUE(data.source_id_for_longest_visible_origin == kSource1 ||
              data.source_id_for_longest_visible_origin == kSource2);
  EXPECT_EQ(2 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(5 * kShortDelay, data.longest_visible_origin_duration);

  // Interval with only |kSource3| being visible.
  data_store()->OnUkmSourceBecameHidden(kSource1, kOrigin);
  data_store()->OnUkmSourceBecameHidden(kSource2, kOrigin);
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kSource3, data.source_id_for_longest_visible_origin);
  EXPECT_EQ(kShortDelay, data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(kShortDelay, data.longest_visible_origin_duration);

  // Back to no visible SourceID.
  data_store()->OnUkmSourceBecameHidden(kSource3, kOrigin);
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(ukm::kInvalidSourceId, data.source_id_for_longest_visible_origin);
  EXPECT_TRUE(data.source_id_for_longest_visible_origin_duration.is_zero());
}

TEST_F(UsageScenarioDataStoreTest, SourceIDVisibleMultipleTimesDuringInterval) {
  data_store()->OnTabAdded();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.com"));

  const ukm::SourceId kSource1 = 42;
  data_store()->OnUkmSourceBecameVisible(kSource1, kOrigin, {});
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kSource1, kOrigin);
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameVisible(kSource1, kOrigin, {});
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kSource1, kOrigin);
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameVisible(kSource1, kOrigin, {});
  auto data = ResetIntervalData();
  EXPECT_EQ(kSource1, data.source_id_for_longest_visible_origin);
  EXPECT_EQ(2 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(2 * kShortDelay, data.longest_visible_origin_duration);
}

TEST_F(UsageScenarioDataStoreTest, VisibleSourceIDsMultipleOrigins) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.com"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://baz.com"));

  const ukm::SourceId kOrigin1SourceId1 = 42;
  const ukm::SourceId kOrigin1SourceId2 = 43;
  const ukm::SourceId kOrigin2SourceId = 44;
  const ukm::SourceId kOrigin3SourceId = 45;

  // |kOrigin1SourceId1| and |kOrigin1SourceId2| visible for 2 time units each,
  // |kOrigin2SourceId| visible for 3 time units. The sourceID reported should
  // be one associated with |kOrigin1| as the cumulative visibility time for its
  // sourceIDs is the greatest.

  data_store()->OnUkmSourceBecameVisible(kOrigin1SourceId1, kOrigin1, {});
  data_store()->OnUkmSourceBecameVisible(kOrigin1SourceId2, kOrigin1, {});
  data_store()->OnUkmSourceBecameVisible(kOrigin2SourceId, kOrigin2, {});
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();
  EXPECT_TRUE(data.source_id_for_longest_visible_origin == kOrigin1SourceId1 ||
              data.source_id_for_longest_visible_origin == kOrigin1SourceId2);
  EXPECT_EQ(kShortDelay, data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(2 * kShortDelay, data.longest_visible_origin_duration);

  // All the sourceIDs associated with |kOrigin2| and |kOrigin3| visible for the
  // same time, which is greater than the cumulative visibility time for the
  // sourceIDs associated with |kOrigin1|.
  data_store()->OnUkmSourceBecameHidden(kOrigin1SourceId1, kOrigin1);
  data_store()->OnUkmSourceBecameVisible(kOrigin3SourceId, kOrigin3, {});
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kOrigin1SourceId2, kOrigin1);
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_TRUE(data.source_id_for_longest_visible_origin == kOrigin2SourceId ||
              data.source_id_for_longest_visible_origin == kOrigin3SourceId);
  EXPECT_EQ(2 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(2 * kShortDelay, data.longest_visible_origin_duration);

  // The sourceID associated with |kOrigin2| is visible for 5 time units, the
  // cumulative time for the source ID associated with |kOrigin1| is also equal
  // to 5 time units.
  data_store()->OnUkmSourceBecameHidden(kOrigin3SourceId, kOrigin3);
  data_store()->OnUkmSourceBecameVisible(kOrigin1SourceId1, kOrigin1, {});
  task_environment_.FastForwardBy(3 * kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kOrigin1SourceId1, kOrigin1);
  data_store()->OnUkmSourceBecameVisible(kOrigin1SourceId2, kOrigin1, {});
  task_environment_.FastForwardBy(2 * kShortDelay);
  data = ResetIntervalData();
  EXPECT_TRUE(data.source_id_for_longest_visible_origin == kOrigin2SourceId);
  EXPECT_EQ(5 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(5 * kShortDelay, data.longest_visible_origin_duration);
}

TEST_F(UsageScenarioDataStoreTest, VisibleSourceIDsMultipleTabsSameOrigin) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.com/a"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://foo.com/b"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://foo.com/c"));

  const ukm::SourceId kOrigin1SourceId = 42;
  const ukm::SourceId kOrigin2SourceId = 44;
  const ukm::SourceId kOrigin3SourceId = 45;

  data_store()->OnUkmSourceBecameVisible(kOrigin1SourceId, kOrigin1, {});
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameVisible(kOrigin2SourceId, kOrigin2, {});
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnUkmSourceBecameHidden(kOrigin1SourceId, kOrigin1);
  data_store()->OnUkmSourceBecameHidden(kOrigin2SourceId, kOrigin2);
  data_store()->OnUkmSourceBecameVisible(kOrigin3SourceId, kOrigin3, {});
  task_environment_.FastForwardBy(kShortDelay);
  auto data = ResetIntervalData();
  EXPECT_TRUE(data.source_id_for_longest_visible_origin == kOrigin1SourceId);
  EXPECT_EQ(2 * kShortDelay,
            data.source_id_for_longest_visible_origin_duration);
  EXPECT_EQ(4 * kShortDelay, data.longest_visible_origin_duration);
}

TEST_F(UsageScenarioDataStoreTest, PlayingVideoInVisibleTab) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();
  data_store()->OnWindowVisible();

  data_store()->OnVideoStartsInVisibleTab();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnWindowVisible();
  data_store()->OnVideoStartsInVisibleTab();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(2 * kShortDelay, data.time_playing_video_in_visible_tab);

  data_store()->OnWindowHidden();
  data_store()->OnVideoStopsInVisibleTab();
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_playing_video_in_visible_tab);

  data_store()->OnWindowHidden();
  data_store()->OnVideoStopsInVisibleTab();
  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(base::TimeDelta(), data.time_playing_video_in_visible_tab);
}

TEST_F(UsageScenarioDataStoreTest, PlayingAudio) {
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  task_environment_.FastForwardBy(kShortDelay);

  data_store()->OnAudioStarts();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_playing_audio);

  data_store()->OnAudioStarts();
  task_environment_.FastForwardBy(kShortDelay);

  data_store()->OnAudioStops();
  data_store()->OnAudioStops();
  task_environment_.FastForwardBy(kShortDelay);

  data = ResetIntervalData();
  EXPECT_EQ(kShortDelay, data.time_playing_audio);

  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(base::TimeDelta(), data.time_playing_audio);
}

TEST_F(UsageScenarioDataStoreTest, SleepEvents) {
  data_store()->OnTabAdded();

  task_environment_.FastForwardBy(kShortDelay);

  data_store()->OnSleepEvent();
  task_environment_.FastForwardBy(kShortDelay);

  auto data = ResetIntervalData();
  EXPECT_EQ(1, data.sleep_events);

  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnSleepEvent();
  task_environment_.FastForwardBy(kShortDelay);
  data_store()->OnSleepEvent();

  data = ResetIntervalData();
  EXPECT_EQ(2, data.sleep_events);

  task_environment_.FastForwardBy(kShortDelay);
  data = ResetIntervalData();
  EXPECT_EQ(0, data.sleep_events);
}
