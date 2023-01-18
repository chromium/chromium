// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SizeIs;

namespace policy {
namespace {

// Waits for a PolicyService to notify its observers that initialization of a
// PolicyDomain has finished.
class PolicyServiceInitializedWaiter : PolicyService::Observer {
 public:
  // Instantiates a PolicyServiceInitializedWaiter which will wait for
  // |policy_service| to signal that |policy_domain| has completed
  // initialization. |policy_service| must outlive this object.
  PolicyServiceInitializedWaiter(PolicyService* policy_service,
                                 PolicyDomain policy_domain)
      : policy_service_(policy_service), policy_domain_(policy_domain) {
    policy_service_->AddObserver(policy_domain_, this);
  }
  PolicyServiceInitializedWaiter(const PolicyServiceInitializedWaiter&) =
      delete;
  PolicyServiceInitializedWaiter& operator=(
      const PolicyServiceInitializedWaiter&) = delete;

  ~PolicyServiceInitializedWaiter() override {
    policy_service_->RemoveObserver(policy_domain_, this);
  }

  // Waits for the PolicyService to signal that the PolicyDomain has completed
  // initialization. If initialization of the PolicyDomain is already complete
  // at the time Wait() is called, returns immediately.
  void Wait() {
    if (policy_service_->IsInitializationComplete(policy_domain_))
      return;
    run_loop_.Run();
  }

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override {}

  // PolicyService::Observer:
  void OnPolicyServiceInitialized(PolicyDomain domain) override {
    run_loop_.Quit();
  }

 private:
  raw_ptr<PolicyService> policy_service_;
  PolicyDomain policy_domain_;
  base::RunLoop run_loop_;
};

void UpdateChromePolicyToMockProviderAndVerify(
    MockConfigurationPolicyProvider* mock_policy_provider,
    const ProfilePolicyConnector& connector) {
  PolicyMap map;
  map.Set(key::kAutofillAddressEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  mock_policy_provider->UpdateChromePolicy(map);
  EXPECT_FALSE(connector.IsProfilePolicy(key::kAutofillAddressEnabled));
  const base::Value* value =
      connector.policy_service()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue(key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value(true), *value);
}

}  // namespace

class ProfilePolicyConnectorTest : public testing::Test {
 protected:
  ProfilePolicyConnectorTest() {}
  ~ProfilePolicyConnectorTest() override {}

  void SetUp() override {
    const auto task_runner = task_environment_.GetMainThreadTaskRunner();
    cloud_policy_manager_ = std::make_unique<CloudPolicyManager>(
        std::string(), std::string(), &cloud_policy_store_, task_runner,
        network::TestNetworkConnectionTracker::CreateGetter());
    cloud_policy_manager_->Init(&schema_registry_);
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // Some tests override the policy service via this global singleton. Unset
    // it here to make sure the cleanup happens.
    BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);

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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SchemaRegistry schema_registry_;
  MockCloudPolicyStore cloud_policy_store_;
  std::unique_ptr<CloudPolicyManager> cloud_policy_manager_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedStubInstallAttributes test_install_attributes_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(ProfilePolicyConnectorTest, IsManagedForManagedUsers) {
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(), false);
  EXPECT_FALSE(connector.IsManaged());

  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_username("test@testdomain.com");
  policy->set_state(enterprise_management::PolicyData::ACTIVE);
  cloud_policy_store_.set_policy_data_for_testing(std::move(policy));
  EXPECT_TRUE(connector.IsManaged());

  // Cleanup.
  connector.Shutdown();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfilePolicyConnectorTest, IsManagedForActiveDirectoryUsers) {
  user_manager::ScopedUserManager scoped_user_manager_enabler(
      std::make_unique<ash::FakeChromeUserManager>());
  ProfilePolicyConnector connector;
  const AccountId account_id =
      AccountId::AdFromUserEmailObjGuid("user@realm.example", "obj-guid");
  std::unique_ptr<user_manager::User> user = CreateRegularUser(account_id);
  connector.Init(user.get(), &schema_registry_, cloud_policy_manager_.get(),
                 &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(), false);
  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_state(enterprise_management::PolicyData::ACTIVE);
  cloud_policy_store_.set_policy_data_for_testing(std::move(policy));
  EXPECT_TRUE(connector.IsManaged());

  // Policy username does not override management realm for Active Directory
  // user.
  policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_state(enterprise_management::PolicyData::ACTIVE);
  policy->set_username("test@testdomain.com");
  cloud_policy_store_.set_policy_data_for_testing(std::move(policy));
  EXPECT_TRUE(connector.IsManaged());

  // Cleanup.
  connector.Shutdown();
}

TEST_F(ProfilePolicyConnectorTest, PrimaryUserPoliciesProxied) {
  auto user_manager_unique_ptr = std::make_unique<ash::FakeChromeUserManager>();
  ash::FakeChromeUserManager* user_manager = user_manager_unique_ptr.get();
  user_manager::ScopedUserManager scoped_user_manager_enabler(
      std::move(user_manager_unique_ptr));

  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_state(enterprise_management::PolicyData::ACTIVE);
  cloud_policy_store_.set_policy_data_for_testing(std::move(policy));
  cloud_policy_store_.policy_map_.Set(
      key::kAutofillAddressEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();

  ProfilePolicyConnector connector;
  const AccountId account_id =
      AccountId::AdFromUserEmailObjGuid("user@realm.example", "obj-guid");
  user_manager::User* user = user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  EXPECT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  connector.Init(user, &schema_registry_, cloud_policy_manager_.get(),
                 &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(), false);
  EXPECT_TRUE(connector.IsManaged());

  EXPECT_FALSE(connector.policy_service()->IsInitializationComplete(
      POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(connector.policy_service()->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_CHROME));

  PolicyServiceInitializedWaiter(connector.policy_service(),
                                 POLICY_DOMAIN_CHROME)
      .Wait();

  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  const base::Value* profile_policy_value =
      connector.policy_service()->GetPolicies(chrome_ns).GetValue(
          key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(profile_policy_value);
  EXPECT_FALSE(profile_policy_value->GetBool());

  const base::Value* proxied_policy_value =
      g_browser_process->policy_service()->GetPolicies(chrome_ns).GetValue(
          key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(proxied_policy_value);
  EXPECT_FALSE(proxied_policy_value->GetBool());

  // Cleanup.
  connector.Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ProfilePolicyConnectorTest, IsProfilePolicy) {
  NiceMock<MockConfigurationPolicyProvider> mock_platform_provider;
  NiceMock<MockPolicyService> mock_policy_service_;
  mock_platform_provider.SetDefaultReturns(
      true /* is_initialization_complete_return */,
      true /* is_first_policy_load_complete_return */);

  g_browser_process->browser_policy_connector()->SetPolicyProviderForTesting(
      &mock_platform_provider);
  // We don't need browser level PolicyService for this test. Setting an empty
  // mock class to prevent the real one being created which is going to observe
  // the local policy provider but never get destroyed until the very end. This
  // will cause DCHECK failure as the local policy provider observer list is not
  // clear.
  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(&mock_policy_service_);
  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(), false);

  // No policy is set initially.
  EXPECT_FALSE(
      connector.IsProfilePolicy(autofill::prefs::kAutofillProfileEnabled));
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  EXPECT_FALSE(connector.policy_service()->GetPolicies(chrome_ns).GetValue(
      key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN));

  // Set the policy at the cloud provider.
  cloud_policy_store_.policy_map_.Set(
      key::kAutofillAddressEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connector.IsProfilePolicy(key::kAutofillAddressEnabled));
  const base::Value* value =
      connector.policy_service()->GetPolicies(chrome_ns).GetValue(
          key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value(false), *value);

  // Now test with a higher-priority provider also setting the policy.
  UpdateChromePolicyToMockProviderAndVerify(&mock_platform_provider, connector);

  // Cleanup.
  connector.Shutdown();
  g_browser_process->browser_policy_connector()->Shutdown();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfilePolicyConnectorTest, MachineLevelUserCloudPolicyForProfile) {
  // Setup mock MachineLevelUserCloudPolicyManager.
  NiceMock<MockConfigurationPolicyProvider>
      mock_machine_level_cloud_policy_provider;
  mock_machine_level_cloud_policy_provider.SetDefaultReturns(
      true /* is_initialization_complete_return */,
      true /* is_first_policy_load_complete_return */);

  // Init and set ProxyPolicyProvider
  ProxyPolicyProvider proxy_policy_provider;
  proxy_policy_provider.SetDelegate(&mock_machine_level_cloud_policy_provider);
  proxy_policy_provider.Init(&schema_registry_);
  g_browser_process->browser_policy_connector()
      ->SetProxyPolicyProviderForTesting(&proxy_policy_provider);

  ProfilePolicyConnector connector;
  connector.Init(nullptr /* user */, &schema_registry_,
                 cloud_policy_manager_.get(), &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(), false);

  UpdateChromePolicyToMockProviderAndVerify(
      &mock_machine_level_cloud_policy_provider, connector);

  connector.Shutdown();
  g_browser_process->browser_policy_connector()->Shutdown();
  proxy_policy_provider.Shutdown();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Basic test for the Enterprise.TimeToFirstPolicyLoad.*" metrics.
TEST_F(ProfilePolicyConnectorTest, InitializationDurationUma) {
  constexpr base::TimeDelta kDelay = base::Seconds(1);
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("foo@bar.com", "fake-gaia-id");

  // Arrange.
  base::HistogramTester histogram_tester;
  user_manager::User* user = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, simulate user login as metric isn't reported otherwise.
  auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
  user = user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager::ScopedUserManager scoped_user_manager_enabler(
      std::move(user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ProfilePolicyConnector connector;
  connector.Init(user, &schema_registry_, cloud_policy_manager_.get(),
                 &cloud_policy_store_,
                 g_browser_process->browser_policy_connector(),
                 /*force_immediate_load=*/false);

  // Act. Simulate installation of policy after some delay.
  task_environment_.FastForwardBy(kDelay);
  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_state(enterprise_management::PolicyData::ACTIVE);
  cloud_policy_store_.set_policy_data_for_testing(std::move(policy));
  cloud_policy_store_.NotifyStoreLoaded();
  // Wait until the store status gets propagated to trigger the initialization.
  PolicyServiceInitializedWaiter(connector.policy_service(),
                                 POLICY_DOMAIN_CHROME)
      .Wait();

  // Assert. Note the recorded delay is exactly `kDelay`, since we're using
  // `MOCK_TIME` and we don't expect delayed tasks here.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Enterprise.TimeToFirstPolicyLoad.Profile."),
              SizeIs(1));
  histogram_tester.ExpectUniqueTimeSample(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      "Enterprise.TimeToFirstPolicyLoad.Profile.Managed.Existing"
#else
      "Enterprise.TimeToFirstPolicyLoad.Profile.Managed"
#endif
      ,
      kDelay, 1);

  // Cleanup.
  connector.Shutdown();
}

}  // namespace policy
