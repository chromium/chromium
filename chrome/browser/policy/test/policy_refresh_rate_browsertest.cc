// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// Waits until `CloudPolicyCore::StartRefreshScheduler()` has run on `core`.
class RefreshSchedulerStartedWaiter : public CloudPolicyCore::Observer {
 public:
  explicit RefreshSchedulerStartedWaiter(CloudPolicyCore* core) : core_(*core) {
    observation_.Observe(&*core_);
  }
  ~RefreshSchedulerStartedWaiter() override = default;

  [[nodiscard]] bool Wait() {
    if (core_->refresh_scheduler()) {
      return true;
    }
    return future_.Wait();
  }

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override {}
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override {
    observation_.Reset();
    future_.SetValue();
  }
  void OnCoreDisconnecting(CloudPolicyCore* core) override {}

 private:
  const raw_ref<CloudPolicyCore> core_;
  base::ScopedObservation<CloudPolicyCore, CloudPolicyCore::Observer>
      observation_{this};
  base::test::TestFuture<void> future_;
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

// Verifies that `PolicyRefreshRate` updates `local_state` and
// `CloudPolicyRefreshScheduler` across unset, custom, dynamic, clamped, and
// invalid-type policy states.
class PolicyRefreshRateBrowserTest : public PolicyTest {
 protected:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    Profile* profile = chrome_test_utils::GetProfile(this);
    CloudPolicyManager* manager = profile->GetCloudPolicyManager();
    ASSERT_TRUE(manager);

#if BUILDFLAG(IS_CHROMEOS)
    // `UserCloudPolicyManagerAsh` starts the refresh scheduler and tracks the
    // `local_state` refresh rate pref once its initial policy load is done.
    ASSERT_TRUE(manager->core()->client());
    ASSERT_TRUE(RefreshSchedulerStartedWaiter(manager->core()).Wait());
#else
    UserCloudPolicyManager* user_manager = profile->GetUserCloudPolicyManager();
    ASSERT_TRUE(user_manager);
    ASSERT_FALSE(user_manager->core()->client());
    // Leave `MockCloudPolicyClient` unregistered so connecting does not post an
    // immediate policy fetch or extension store load.
    user_manager->Connect(
        g_browser_process->local_state(),
        std::make_unique<testing::NiceMock<MockCloudPolicyClient>>());
#endif
    ASSERT_TRUE(manager->core()->refresh_scheduler());
  }

  CloudPolicyRefreshScheduler* GetRefreshScheduler() {
    CloudPolicyManager* manager =
        chrome_test_utils::GetProfile(this)->GetCloudPolicyManager();
    return manager ? manager->core()->refresh_scheduler() : nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(PolicyRefreshRateBrowserTest, Unset) {
  PrefService* local_state = g_browser_process->local_state();
  const PrefService::Preference* pref =
      local_state->FindPreference(policy_prefs::kUserPolicyRefreshRate);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_EQ(local_state->GetInteger(policy_prefs::kUserPolicyRefreshRate),
            CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  ASSERT_TRUE(GetRefreshScheduler());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(),
            CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

IN_PROC_BROWSER_TEST_F(PolicyRefreshRateBrowserTest, CustomRefreshRate) {
  PrefService* local_state = g_browser_process->local_state();
  PolicyMap policies;
  SetPolicy(&policies, key::kPolicyRefreshRate, base::Value(3600000));
  UpdateProviderPolicy(policies);
  WaitForPrefValue(local_state, policy_prefs::kUserPolicyRefreshRate,
                   base::Value(3600000));

  const PrefService::Preference* pref =
      local_state->FindPreference(policy_prefs::kUserPolicyRefreshRate);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_EQ(local_state->GetInteger(policy_prefs::kUserPolicyRefreshRate),
            3600000);
  ASSERT_TRUE(GetRefreshScheduler());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(), 3600000);

  // Dynamically update the policy.
  SetPolicy(&policies, key::kPolicyRefreshRate, base::Value(7200000));
  UpdateProviderPolicy(policies);
  WaitForPrefValue(local_state, policy_prefs::kUserPolicyRefreshRate,
                   base::Value(7200000));
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(), 7200000);

  // Clear the policy and verify dynamic reset back to the default delay.
  policies.Clear();
  UpdateProviderPolicy(policies);
  WaitForPrefValue(local_state, policy_prefs::kUserPolicyRefreshRate,
                   base::Value(static_cast<int>(
                       CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs)));
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(),
            CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

IN_PROC_BROWSER_TEST_F(PolicyRefreshRateBrowserTest, ClampedRefreshRate) {
  PrefService* local_state = g_browser_process->local_state();
  PolicyMap policies;
  SetPolicy(&policies, key::kPolicyRefreshRate, base::Value(300000));
  UpdateProviderPolicy(policies);
  WaitForPrefValue(local_state, policy_prefs::kUserPolicyRefreshRate,
                   base::Value(300000));

  EXPECT_EQ(local_state->GetInteger(policy_prefs::kUserPolicyRefreshRate),
            300000);
  ASSERT_TRUE(GetRefreshScheduler());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(),
            CloudPolicyRefreshScheduler::kRefreshDelayMinMs);

  SetPolicy(&policies, key::kPolicyRefreshRate, base::Value(700000000));
  UpdateProviderPolicy(policies);
  WaitForPrefValue(local_state, policy_prefs::kUserPolicyRefreshRate,
                   base::Value(700000000));
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(),
            CloudPolicyRefreshScheduler::kRefreshDelayMaxMs);
}

IN_PROC_BROWSER_TEST_F(PolicyRefreshRateBrowserTest, InvalidTypeIgnored) {
  PolicyMap policies;
  SetPolicy(&policies, key::kPolicyRefreshRate, base::Value("not_an_int"));
  UpdateProviderPolicy(policies);

  PrefService* local_state = g_browser_process->local_state();
  const PrefService::Preference* pref =
      local_state->FindPreference(policy_prefs::kUserPolicyRefreshRate);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_EQ(local_state->GetInteger(policy_prefs::kUserPolicyRefreshRate),
            CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  ASSERT_TRUE(GetRefreshScheduler());
  EXPECT_EQ(GetRefreshScheduler()->GetActualRefreshDelay(),
            CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

}  // namespace policy
