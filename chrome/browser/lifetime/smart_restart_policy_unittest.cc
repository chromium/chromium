// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_policy.h"

#include "chrome/browser/lifetime/restartability_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace smart_restart {

TEST(SmartRestartPolicyTest, ProceedWhenSafe) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;

  EXPECT_EQ(ExecutionOutcome::kExecuted,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST(SmartRestartPolicyTest, BlockWhenWindowExists) {
  RestartabilityState state;
  state.total_browser_count_is_zero = false;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST(SmartRestartPolicyTest, BlockWhenDownloading) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.download_count = 1;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST(SmartRestartPolicyTest, BlockWhenMediaPlaying) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.is_audio_playing = true;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST(SmartRestartPolicyTest, BlockWhenIncognitoOpen) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.has_incognito = true;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

}  // namespace smart_restart
