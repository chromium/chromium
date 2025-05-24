// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_store.h"

#include <cstddef>
#include <memory>

#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome_device_policy.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kTestAccountId[] = "test_public_account_id";

// Creates a valid `UserPolicyBuilder` configured for device local accounts.
std::unique_ptr<policy::UserPolicyBuilder> CreateDeviceLocalAccountPolicy() {
  auto device_local_account_policy =
      std::make_unique<policy::UserPolicyBuilder>();
  device_local_account_policy->policy_data().set_policy_type(
      dm_protocol::kChromePublicAccountPolicyType);
  device_local_account_policy->policy_data().set_settings_entity_id(
      kTestAccountId);
  device_local_account_policy->policy_data().set_username(kTestAccountId);
  device_local_account_policy->Build();
  return device_local_account_policy;
}

// Creates an invalid `UserPolicyBuilder`, such that policy validation for
// device local accounts should fail.
std::unique_ptr<policy::UserPolicyBuilder>
CreateInvalidDeviceLocalAccountPolicy() {
  auto policy = CreateDeviceLocalAccountPolicy();
  policy->policy_data().set_policy_type(
      dm_protocol::kChromeRemoteCommandPolicyType);
  policy->Build();
  return policy;
}

// Installs `device_local_account_policy` and `device_policy` into
// `session_manager`.
void InstallPolicies(base::test::TaskEnvironment& environment,
                     ash::FakeSessionManagerClient& session_manager,
                     DevicePolicyBuilder& device_policy,
                     UserPolicyBuilder& device_local_account_policy) {
  session_manager.set_device_local_account_policy(
      kTestAccountId, device_local_account_policy.GetBlob());

  em::DeviceLocalAccountInfoProto* account =
      device_policy.payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(kTestAccountId);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);

  device_policy.Build();
  session_manager.set_device_policy(device_policy.GetBlob());
  ash::DeviceSettingsService::Get()->OwnerKeySet(true);
  environment.RunUntilIdle();
}

// Runs `runner` until idle considering `environment` may post tasks back to it.
void RunUntilIdle(base::test::TaskEnvironment& environment,
                  scoped_refptr<base::TestSimpleTaskRunner> runner) {
  while (runner->HasPendingTask()) {
    runner->RunUntilIdle();
    environment.RunUntilIdle();
  }
}

DeviceLocalAccountPolicyStore CreatePolicyStore(
    scoped_refptr<base::TestSimpleTaskRunner> background_runner,
    scoped_refptr<base::TestSimpleTaskRunner> first_load_runner) {
  return DeviceLocalAccountPolicyStore(
      kTestAccountId, ash::SessionManagerClient::Get(),
      ash::DeviceSettingsService::Get(), background_runner, first_load_runner);
}

}  // namespace

class DeviceLocalAccountPolicyStoreTest : public testing::Test {
 public:
  DeviceLocalAccountPolicyStoreTest() {
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        device_policy_.GetSigningKey());
    ash::OwnerSettingsServiceAshFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    ash::DeviceSettingsService::Get()->SetSessionManager(
        &fake_session_manager_client_, owner_key_util_);
  }
  DeviceLocalAccountPolicyStoreTest(const DeviceLocalAccountPolicyStoreTest&) =
      delete;
  DeviceLocalAccountPolicyStoreTest& operator=(
      const DeviceLocalAccountPolicyStoreTest&) = delete;
  ~DeviceLocalAccountPolicyStoreTest() override = default;

  policy::DevicePolicyBuilder device_policy_;
  ash::FakeSessionManagerClient fake_session_manager_client_;
  base::test::TaskEnvironment environment_;

 private:
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestDeviceSettingsService scoped_test_device_settings_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
};

TEST_F(DeviceLocalAccountPolicyStoreTest,
       UsesFirstLoadRunnerWhenThereAreNoPolicies) {
  auto background_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto first_load_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto policy_store = CreatePolicyStore(background_runner, first_load_runner);

  auto bad_policy = CreateInvalidDeviceLocalAccountPolicy();

  InstallPolicies(environment_, fake_session_manager_client_, device_policy_,
                  *bad_policy);

  ASSERT_FALSE(policy_store.has_policy());

  // Call `Store()` multiple times with the bad policy, and verify
  // `first_load_runner` is used every time.
  for (size_t i = 0; i < 5; i++) {
    policy_store.Store(bad_policy->policy());
    environment_.RunUntilIdle();

    ASSERT_FALSE(background_runner->HasPendingTask());
    ASSERT_TRUE(first_load_runner->HasPendingTask());
    RunUntilIdle(environment_, first_load_runner);

    ASSERT_FALSE(policy_store.has_policy());
  }
}

TEST_F(DeviceLocalAccountPolicyStoreTest,
       UsesBackgroundRunnerWhenThereArePolicies) {
  auto background_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto first_load_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto policy_store = CreatePolicyStore(background_runner, first_load_runner);

  auto policy = CreateDeviceLocalAccountPolicy();

  InstallPolicies(environment_, fake_session_manager_client_, device_policy_,
                  *policy);

  // First `Store()` uses `first_load_runner`.
  policy_store.Store(policy->policy());
  environment_.RunUntilIdle();

  ASSERT_FALSE(background_runner->HasPendingTask());
  ASSERT_TRUE(first_load_runner->HasPendingTask());
  RunUntilIdle(environment_, first_load_runner);

  // Now `policy_store` has policies, verify several following calls use
  // `background_runner`.
  ASSERT_TRUE(policy_store.has_policy());
  for (size_t i = 0; i < 5; i++) {
    policy_store.Store(policy->policy());
    environment_.RunUntilIdle();

    ASSERT_TRUE(background_runner->HasPendingTask());
    ASSERT_FALSE(first_load_runner->HasPendingTask());
    RunUntilIdle(environment_, background_runner);
  }
}

}  // namespace policy
