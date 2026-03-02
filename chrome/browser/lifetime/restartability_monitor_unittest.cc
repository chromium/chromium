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

TEST_F(RestartabilityMonitorTest, BlockedByUnloadHandler) {
  RestartabilityState state;
  state.has_dirty_tabs = true;
  EXPECT_EQ(RestartabilityState::SmartRestartStateFactor::kUnloadHandler,
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

}  // namespace smart_restart
