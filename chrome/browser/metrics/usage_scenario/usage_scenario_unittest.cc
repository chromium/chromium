// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/usage_scenario.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::StrEq;

TEST(UsageScenarioTest, GetLongIntervalScenario_ZeroWindow) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 0;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kZeroWindow);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_AllTabsHidden_VideoCapture) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kAllTabsHiddenVideoCapture);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_AllTabsHidden_Audio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(0);
  interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kAllTabsHiddenAudio);
}

TEST(UsageScenarioTest,
     GetLongIntervalScenario_AllTabsHidden_NoVideoCaptureOrAudio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(0);
  interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kAllTabsHiddenNoVideoCaptureOrAudio);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_VideoCapture) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kVideoCapture);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_FullscreenVideo) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kFullscreenVideo);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_EmbeddedVideo_NoNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kEmbeddedVideoNoNavigation);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_EmbeddedVideo_WithNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kEmbeddedVideoWithNavigation);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_Audio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;
  interval_data.top_level_navigation_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario, Scenario::kAudio);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_Navigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kNavigation);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_Interaction) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 1;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kInteraction);
}

TEST(UsageScenarioTest, GetLongIntervalScenario_Passive) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 0;

  EXPECT_EQ(GetLongIntervalScenario(interval_data).scenario,
            Scenario::kPassive);
}

#if BUILDFLAG(IS_MAC)
TEST(UsageScenarioTest, GetShortIntervalScenarioParams_ZeroWindow) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 0;

  UsageScenarioDataStore::IntervalData long_interval_data;
  long_interval_data.max_tab_count = 0;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".ZeroWindow");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_ZeroWindow_Recent) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 0;

  UsageScenarioDataStore::IntervalData long_interval_data;
  long_interval_data.max_tab_count = 1;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".ZeroWindow_Recent");
}

TEST(UsageScenarioTest,
     GetShortIntervalScenarioParams_AllTabsHidden_VideoCapture) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".AllTabsHidden_VideoCapture");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_AllTabsHidden_Audio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".AllTabsHidden_Audio");
}

TEST(UsageScenarioTest,
     GetShortIntervalScenarioParams_AllTabsHidden_NoVideoCaptureOrAudio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix,
               ".AllTabsHidden_NoVideoCaptureOrAudio");
}

TEST(
    UsageScenarioTest,
    GetShortIntervalScenarioParams_AllTabsHidden_NoVideoCaptureOrAudio_Recent) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix,
               ".AllTabsHidden_NoVideoCaptureOrAudio_Recent");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_VideoCapture) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".VideoCapture");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_FullscreenVideo) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".FullscreenVideo");
}

TEST(UsageScenarioTest,
     GetShortIntervalScenarioParams_EmbeddedVideo_NoNavigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 0;
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".EmbeddedVideo_NoNavigation");
}

TEST(UsageScenarioTest,
     GetShortIntervalScenarioParams_EmbeddedVideo_WithNavigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix,
               ".EmbeddedVideo_WithNavigation");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_Audio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".Audio");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_Navigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".Navigation");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_Interaction) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 0;
  short_interval_data.user_interaction_count = 1;
  // Values below should be ignored.

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".Interaction");
}

TEST(UsageScenarioTest, GetShortIntervalScenarioParams_Passive) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 0;
  short_interval_data.user_interaction_count = 0;
  // Values below should be ignored.

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  const ScenarioParams scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  EXPECT_STREQ(scenario_params.histogram_suffix, ".Passive");
}

#endif  // BUILDFLAG(IS_MAC)
