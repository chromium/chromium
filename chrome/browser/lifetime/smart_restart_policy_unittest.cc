// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_policy.h"

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace smart_restart {

class SmartRestartPolicyTest : public testing::Test {
 public:
  SmartRestartPolicyTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

namespace {
#if BUILDFLAG(IS_MAC)
void SetupTestingLocalStatePref() {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  if (!local_state->FindPreference(prefs::kUpdateOnZeroWindowEnabled)) {
    local_state->registry()->RegisterBooleanPref(
        prefs::kUpdateOnZeroWindowEnabled, true);
  }
}
#endif  // BUILDFLAG(IS_MAC)
}  // namespace

TEST_F(SmartRestartPolicyTest, ProceedWhenSafe) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;

  EXPECT_EQ(ExecutionOutcome::kExecuted,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST_F(SmartRestartPolicyTest, BlockWhenWindowExists) {
  RestartabilityState state;
  state.total_browser_count_is_zero = false;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST_F(SmartRestartPolicyTest, BlockWhenDownloading) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.download_count = 1;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST_F(SmartRestartPolicyTest, BlockWhenMediaPlaying) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.is_audio_playing = true;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST_F(SmartRestartPolicyTest, BlockWhenIncognitoOpen) {
  RestartabilityState state;
  state.total_browser_count_is_zero = true;
  state.has_incognito = true;

  EXPECT_EQ(ExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::ShouldRestart(state));
}

TEST_F(SmartRestartPolicyTest, BlockLockScreenWhenManaged) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  ExtendedRestartabilityState state;
  EXPECT_EQ(ExtendedExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::CanLockScreenRestartProceed(state));
}

#if BUILDFLAG(IS_MAC)
TEST_F(SmartRestartPolicyTest, ProceedZeroWindowWhenManagedByDefault) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  EXPECT_TRUE(SmartRestartPolicy::CanZeroWindowRestartProceed());
}

TEST_F(SmartRestartPolicyTest, ExplicitPolicyDisabledBlocksZeroWindow) {
  // Explicitly mark device as unmanaged to avoid crash in IsManaged check.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);

  SetupTestingLocalStatePref();
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetManagedPref(
      prefs::kUpdateOnZeroWindowEnabled, base::Value(false));

  EXPECT_FALSE(SmartRestartPolicy::CanZeroWindowRestartProceed());

  // Lock screen should not be affected by zero window policy (it checks
  // IsManaged which is false here).
  ExtendedRestartabilityState state;
  EXPECT_EQ(ExtendedExecutionOutcome::kExecuted,
            SmartRestartPolicy::CanLockScreenRestartProceed(state));

  // Clean up
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->RemoveManagedPref(
      prefs::kUpdateOnZeroWindowEnabled);
}

TEST_F(SmartRestartPolicyTest,
       ExplicitPolicyEnabledAllowsZeroWindowWhenManaged) {
  SetupTestingLocalStatePref();

  // Mark device as managed.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  // Enable policy explicitly.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetManagedPref(
      prefs::kUpdateOnZeroWindowEnabled, base::Value(true));

  // Zero window should proceed because policy overrides management status.
  EXPECT_TRUE(SmartRestartPolicy::CanZeroWindowRestartProceed());

  // Lock screen should still be blocked because it only checks IsManaged and
  // ignores this policy.
  ExtendedRestartabilityState state;
  EXPECT_EQ(ExtendedExecutionOutcome::kBlockedByPolicy,
            SmartRestartPolicy::CanLockScreenRestartProceed(state));

  // Clean up
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->RemoveManagedPref(
      prefs::kUpdateOnZeroWindowEnabled);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace smart_restart
