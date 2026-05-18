// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/restartability_monitor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace smart_restart {

class RestartabilityMonitorTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RestartabilityMonitorTest, BasicCheck) {
  RestartabilityState state;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kNone,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, BlockedByDownload) {
  RestartabilityState state;
  state.download_count = 1;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kDownload,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, BlockedByMedia) {
  RestartabilityState state;
  state.is_audio_playing = true;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kMedia,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, BlockedByAppWindow) {
  RestartabilityState state;
  state.has_app_windows = true;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kAppWindow,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, BlockedByBeforeUnloadHandler) {
  RestartabilityState state;
  state.has_dirty_tabs = true;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kBeforeUnloadHandler,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, BlockedByIncognito) {
  RestartabilityState state;
  state.has_incognito = true;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kIncognito,
            state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, ZeroBrowserCount) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  EXPECT_EQ(
      RestartabilityState::SmartRestartStateFactor::kTotalBrowserCountZero,
      state.GetRestartabilityStateFactor());
}

TEST_F(RestartabilityMonitorTest, HasAnyActiveBlockers_True) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.download_count = 1;  // Blocker

  EXPECT_TRUE(state.HasAnyActiveBlockers());
}

TEST_F(RestartabilityMonitorTest, HasAnyActiveBlockers_False) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  // No other blockers

  EXPECT_FALSE(state.HasAnyActiveBlockers());
}

TEST_F(RestartabilityMonitorTest, HasAnyActiveBlockers_Multiple) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.has_incognito = true;     // Blocker
  state.is_audio_playing = true;  // Blocker

  EXPECT_TRUE(state.HasAnyActiveBlockers());
}

// Tests for ExtendedRestartabilityState
TEST_F(RestartabilityMonitorTest, ExtendedState_AddBlockerUpdatesLevel) {
  ExtendedRestartabilityState extended_state;
  using Blocker = ExtendedRestartabilityState::SmartRestartBlocker;
  using Level = ExtendedRestartabilityState::SmartRestartDisruptionLevel;

  // Verify it starts at kNoDisruption.
  EXPECT_EQ(Level::kNoDisruption, extended_state.max_disruption_level);

  // Upgrade to Level 1 (Protected)
  extended_state.AddBlocker(Blocker::kVisible);
  EXPECT_EQ(Level::kLowDisruption, extended_state.max_disruption_level);

  // Upgrade to Level 2 (Disallowed)
  extended_state.AddBlocker(Blocker::kCapturingVideo);
  EXPECT_EQ(Level::kHighDisruption, extended_state.max_disruption_level);

  // Adding a lower level blocker doesn't downgrade
  extended_state.AddBlocker(Blocker::kPinnedTab);
  EXPECT_EQ(Level::kHighDisruption, extended_state.max_disruption_level);
}

TEST_F(RestartabilityMonitorTest, ExtendedState_EnumSetDeduplication) {
  ExtendedRestartabilityState extended_state;
  using Blocker = ExtendedRestartabilityState::SmartRestartBlocker;

  extended_state.AddBlocker(Blocker::kVisible);
  extended_state.AddBlocker(Blocker::kVisible);  // Duplicate

  EXPECT_EQ(1u, extended_state.blockers.size());
}

}  // namespace smart_restart
