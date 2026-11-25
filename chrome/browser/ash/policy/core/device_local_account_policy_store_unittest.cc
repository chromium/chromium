// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_store.h"

#include <cstddef>
#include <memory>

#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome_device_policy.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/keypair.h"
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

// Creates a store that runs all its tasks on the main thread.
DeviceLocalAccountPolicyStore CreateMainThreadPolicyStore() {
  auto runner = base::SequencedTaskRunner::GetCurrentDefault();
  return DeviceLocalAccountPolicyStore(
      kTestAccountId, ash::SessionManagerClient::Get(),
      ash::DeviceSettingsService::Get(), runner, runner);
}

// Waits for the next store notification.
class StoreObserver : public CloudPolicyStore::Observer {
 public:
  explicit StoreObserver(CloudPolicyStore& store) {
    observation_.Observe(&store);
  }

  void Wait() { run_loop_.Run(); }

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override { run_loop_.Quit(); }
  void OnStoreError(CloudPolicyStore* store) override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<CloudPolicyStore, CloudPolicyStore::Observer>
      observation_{this};
};

// Counts store notifications.
class StoreNotificationCounter : public CloudPolicyStore::Observer {
 public:
  explicit StoreNotificationCounter(CloudPolicyStore& store) {
    observation_.Observe(&store);
  }

  int loaded_count() const { return loaded_count_; }
  int error_count() const { return error_count_; }
  int total_count() const { return loaded_count_ + error_count_; }

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override { ++loaded_count_; }
  void OnStoreError(CloudPolicyStore* store) override { ++error_count_; }

 private:
  int loaded_count_ = 0;
  int error_count_ = 0;
  base::ScopedObservation<CloudPolicyStore, CloudPolicyStore::Observer>
      observation_{this};
};

// Waits for the next DeviceSettingsService update.
class DeviceSettingsWaiter : public ash::DeviceSettingsService::Observer {
 public:
  DeviceSettingsWaiter() {
    observation_.Observe(ash::DeviceSettingsService::Get());
  }

  void Wait() { run_loop_.Run(); }

  // ash::DeviceSettingsService::Observer:
  void DeviceSettingsUpdated() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ash::DeviceSettingsService,
                          ash::DeviceSettingsService::Observer>
      observation_{this};
};

}  // namespace

class DeviceLocalAccountPolicyStoreTest : public testing::Test {
 public:
  DeviceLocalAccountPolicyStoreTest() {
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        *device_policy_.GetSigningKey());
    ash::OwnerSettingsServiceAshFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    ash::DeviceSettingsService::Get()->StartProcessing(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &fake_session_manager_client_, owner_key_util_);
  }
  DeviceLocalAccountPolicyStoreTest(const DeviceLocalAccountPolicyStoreTest&) =
      delete;
  DeviceLocalAccountPolicyStoreTest& operator=(
      const DeviceLocalAccountPolicyStoreTest&) = delete;
  ~DeviceLocalAccountPolicyStoreTest() override = default;

  // Simulates session_manager persisting the rotated owner key.
  void RotateOwnerKey(const crypto::keypair::PrivateKey& new_key) {
    device_policy_.SetSigningKey(new_key);
    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());
    owner_key_util_->SetPublicKeyFromPrivateKey(new_key);
    ash::DeviceSettingsService::Get()->OwnerKeySet(true);
  }

  // Stores a policy signed with the other key.
  void StorePolicySignedWithOtherKey(DeviceLocalAccountPolicyStore& store) {
    auto old_policy = CreateDeviceLocalAccountPolicy();
    InstallPolicies(environment_, fake_session_manager_client_, device_policy_,
                    *old_policy);
    auto policy = CreateDeviceLocalAccountPolicy();
    policy->SetSigningKey(PolicyBuilder::CreateTestOtherSigningKey());
    policy->Build();
    StoreObserver store_observer(store);
    store.Store(policy->policy());
    store_observer.Wait();
  }

  policy::DevicePolicyBuilder device_policy_;
  ash::FakeSessionManagerClient fake_session_manager_client_;
  base::test::TaskEnvironment environment_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};

 private:
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestDeviceSettingsService scoped_test_device_settings_;
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

TEST_F(DeviceLocalAccountPolicyStoreTest,
       RetriesStoreWhenOwnerKeyRotatesAfterSignatureFailure) {
  auto policy_store = CreateMainThreadPolicyStore();
  StorePolicySignedWithOtherKey(policy_store);

  // The error is reported and the retry is pending.
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, policy_store.status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            policy_store.validation_status());
  EXPECT_FALSE(policy_store.has_policy());
  EXPECT_TRUE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());

  // An update with the same key doesn't trigger the retry.
  DeviceSettingsWaiter same_key_waiter;
  ash::DeviceSettingsService::Get()->Load();
  same_key_waiter.Wait();
  EXPECT_TRUE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());

  StoreObserver store_observer(policy_store);
  RotateOwnerKey(PolicyBuilder::CreateTestOtherSigningKey());
  store_observer.Wait();

  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, policy_store.status());
  EXPECT_TRUE(policy_store.has_policy());
}

TEST_F(DeviceLocalAccountPolicyStoreTest,
       RetriesRightAwayWhenOwnerKeyRotatesDuringValidation) {
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto policy_store = CreatePolicyStore(runner, runner);
  auto old_policy = CreateDeviceLocalAccountPolicy();
  InstallPolicies(environment_, fake_session_manager_client_, device_policy_,
                  *old_policy);
  auto policy = CreateDeviceLocalAccountPolicy();
  policy->SetSigningKey(PolicyBuilder::CreateTestOtherSigningKey());
  policy->Build();
  StoreNotificationCounter counter(policy_store);

  policy_store.Store(policy->policy());
  ASSERT_TRUE(base::test::RunUntil([&] { return runner->HasPendingTask(); }));

  // The owner key rotates while the validation is pending.
  DeviceSettingsWaiter waiter;
  RotateOwnerKey(PolicyBuilder::CreateTestOtherSigningKey());
  waiter.Wait();

  while (counter.total_count() == 0) {
    runner->RunPendingTasks();
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return runner->HasPendingTask() || counter.total_count() > 0; }));
  }

  EXPECT_EQ(0, counter.error_count());
  EXPECT_EQ(1, counter.loaded_count());
  EXPECT_FALSE(runner->HasPendingTask());
  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, policy_store.status());
  EXPECT_TRUE(policy_store.has_policy());
}

TEST_F(DeviceLocalAccountPolicyStoreTest, NewerStoreDropsPendingRetry) {
  auto policy_store = CreateMainThreadPolicyStore();
  StorePolicySignedWithOtherKey(policy_store);
  ASSERT_TRUE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());

  // A newer policy supersedes the pending retry.
  StoreObserver store_observer(policy_store);
  policy_store.Store(CreateDeviceLocalAccountPolicy()->policy());
  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
  store_observer.Wait();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, policy_store.status());
}

TEST_F(DeviceLocalAccountPolicyStoreTest, FailedRetryIsNotRearmed) {
  auto policy_store = CreateMainThreadPolicyStore();
  StorePolicySignedWithOtherKey(policy_store);
  ASSERT_TRUE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());

  // The owner key rotates to a key that doesn't match the policy.
  StoreObserver store_observer(policy_store);
  RotateOwnerKey(crypto::keypair::PrivateKey::GenerateRsa2048());
  store_observer.Wait();

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, policy_store.status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            policy_store.validation_status());
  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
  EXPECT_FALSE(policy_store.has_policy());
}

TEST_F(DeviceLocalAccountPolicyStoreTest, NoRetryForOtherValidationErrors) {
  auto policy_store = CreateMainThreadPolicyStore();
  auto bad_policy = CreateInvalidDeviceLocalAccountPolicy();
  InstallPolicies(environment_, fake_session_manager_client_, device_policy_,
                  *bad_policy);

  StoreObserver store_observer(policy_store);
  policy_store.Store(bad_policy->policy());
  store_observer.Wait();

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, policy_store.status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE,
            policy_store.validation_status());
  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
}

TEST_F(DeviceLocalAccountPolicyStoreTest, NoRetryWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kDeviceLocalAccountPolicyRetryOnOwnerKeyRotation);
  auto policy_store = CreateMainThreadPolicyStore();
  StorePolicySignedWithOtherKey(policy_store);
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, policy_store.status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            policy_store.validation_status());
  EXPECT_FALSE(policy_store.HasPendingOwnerKeyRotationRetryForTesting());
  EXPECT_FALSE(policy_store.has_policy());
}

}  // namespace policy
