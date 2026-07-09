// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_policy.h"

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
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

TEST(SmartRestartPolicyTest, BlockLockScreenWhenManaged) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  ExtendedRestartabilityState state;
  EXPECT_EQ(ExtendedExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::CanLockScreenRestartProceed(state));
}

#if BUILDFLAG(IS_MAC)
TEST(SmartRestartPolicyTest, BlockZeroWindowWhenManaged) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  EXPECT_FALSE(SmartRestartPolicy::CanZeroWindowRestartProceed());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace smart_restart
