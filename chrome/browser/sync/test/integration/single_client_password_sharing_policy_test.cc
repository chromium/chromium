// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

namespace {

using policy::PolicyMap;
using testing::NiceMock;

class SingleClientPasswordSharingPolicyTest : public SyncTest {
 public:
  SingleClientPasswordSharingPolicyTest() : SyncTest(SINGLE_CLIENT) {
  }
  ~SingleClientPasswordSharingPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    PolicyMap policy_with_defaults = policy.Clone();
#if BUILDFLAG(IS_CHROMEOS)
    SetEnterpriseUsersDefaults(&policy_with_defaults);
#endif
    policy_provider_.UpdateChromePolicy(policy_with_defaults);
  }

 private:
  NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(SingleClientPasswordSharingPolicyTest,
                       ShouldDisablePasswordSharingDataTypes) {
  ASSERT_TRUE(SetupSync());

  // Verify that both sharing data types are enabled by default when the policy
  // is not set.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::OUTGOING_PASSWORD_SHARING_INVITATION));

  // Set a policy that explicitly enables password sharing.
  PolicyMap policies;
  policies.Set(policy::key::kPasswordSharingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Verify that both sharing data types are still enabled when the policy is
  // enabled.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::OUTGOING_PASSWORD_SHARING_INVITATION));

  // Set a policy that disables password sharing.
  policies.Set(policy::key::kPasswordSharingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // With the policy is disabled, both data types should be deactivated.
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::OUTGOING_PASSWORD_SHARING_INVITATION));
}

}  // namespace
