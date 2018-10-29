// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // defined(OS_CHROMEOS)

using testing::Return;
using testing::_;

namespace policy {

class ProfilePolicyConnectorTest : public testing::Test {
 protected:
  ProfilePolicyConnectorTest() {}
  ~ProfilePolicyConnectorTest() override {}

  void SetUp() override {
    // This must be set up before the TestingBrowserProcess is created.
    BrowserPolicyConnector::SetPolicyProviderForTesting(&mock_provider_);

    EXPECT_CALL(mock_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));

    cloud_policy_store_.NotifyStoreLoaded();
    const auto task_runner = scoped_task_environment_.GetMainThreadTaskRunner();
    cloud_policy_manager_.reset(new CloudPolicyManager(
        std::string(), std::string(), &cloud_policy_store_, task_runner,
        network::TestNetworkConnectionTracker::CreateGetter()));
    cloud_policy_manager_->Init(&schema_registry_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
    cloud_policy_manager_->Shutdown();
  }

  std::unique_ptr<user_manager::User> CreateRegularUser(
      const AccountId& account_id) const {
    return base::WrapUnique<user_manager::User>(
        user_manager::User::CreateRegularUser(account_id,
                                              user_manager::USER_TYPE_REGULAR));
  }

  // Needs to be the first member.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SchemaRegistry schema_registry_;
  MockConfigurationPolicyProvider mock_provider_;
  MockCloudPolicyStore cloud_policy_store_;
  std::unique_ptr<CloudPolicyManager> cloud_policy_manager_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
#endif  // defined(OS_CHROMEOS)
};

TEST_F(ProfilePolicyConnectorTest, IsManagedForManagedUsers) {
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_, false);
  EXPECT_FALSE(connector.IsManaged());

  cloud_policy_store_.policy_.reset(new enterprise_management::PolicyData());
  cloud_policy_store_.policy_->set_username("test@testdomain.com");
  cloud_policy_store_.policy_->set_state(
      enterprise_management::PolicyData::ACTIVE);
  EXPECT_TRUE(connector.IsManaged());

  // Cleanup.
  connector.Shutdown();
}

#if defined(OS_CHROMEOS)
TEST_F(ProfilePolicyConnectorTest, IsManagedForActiveDirectoryUsers) {
  user_manager::ScopedUserManager scoped_user_manager_enabler(
      std::make_unique<chromeos::FakeChromeUserManager>());
  ProfilePolicyConnector connector;
  const AccountId account_id =
      AccountId::AdFromUserEmailObjGuid("user@realm.example", "obj-guid");
  std::unique_ptr<user_manager::User> user = CreateRegularUser(account_id);
  connector.Init(user.get(), &schema_registry_, cloud_policy_manager_.get(),
                 &cloud_policy_store_, false);
  cloud_policy_store_.policy_.reset(new enterprise_management::PolicyData());
  cloud_policy_store_.policy_->set_state(
      enterprise_management::PolicyData::ACTIVE);
  EXPECT_TRUE(connector.IsManaged());

  // Policy username does not override management realm for Active Directory
  // user.
  cloud_policy_store_.policy_->set_username("test@testdomain.com");
  EXPECT_TRUE(connector.IsManaged());

  // Cleanup.
  connector.Shutdown();
}
#endif  // defined(OS_CHROMEOS)

TEST_F(ProfilePolicyConnectorTest, IsProfilePolicy) {
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_, false);

  // No policy is set initially.
  EXPECT_FALSE(
      connector.IsProfilePolicy(autofill::prefs::kAutofillProfileEnabled));
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  EXPECT_FALSE(connector.policy_service()->GetPolicies(chrome_ns).GetValue(
      key::kAutofillAddressEnabled));

  // Set the policy at the cloud provider.
  cloud_policy_store_.policy_map_.Set(
      key::kAutofillAddressEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connector.IsProfilePolicy(key::kAutofillAddressEnabled));
  const base::Value* value =
      connector.policy_service()->GetPolicies(chrome_ns).GetValue(
          key::kAutofillAddressEnabled);
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::Value(false).Equals(value));

  // Now test with a higher-priority provider also setting the policy.
  PolicyMap map;
  map.Set(key::kAutofillAddressEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(true), nullptr);
  mock_provider_.UpdateChromePolicy(map);
  EXPECT_FALSE(connector.IsProfilePolicy(key::kAutofillAddressEnabled));
  value = connector.policy_service()->GetPolicies(chrome_ns).GetValue(
      key::kAutofillAddressEnabled);
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::Value(true).Equals(value));

  // Cleanup.
  connector.Shutdown();
}

}  // namespace policy
