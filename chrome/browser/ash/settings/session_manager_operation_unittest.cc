// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/session_manager_operation.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;
using testing::Mock;

namespace ash {
namespace {

class ObservableFakeSessionManagerClient : public FakeSessionManagerClient {
 public:
  void SetOnRetrievePolicyCalled(const base::RepeatingClosure& closure) {
    on_retrieve_policy_called_ = closure;
  }

  // SessionManagerClient override:
  void RetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                      RetrievePolicyCallback callback) override {
    FakeSessionManagerClient::RetrievePolicy(descriptor, std::move(callback));

    // Run the task just after the |callback| is invoked.
    if (!on_retrieve_policy_called_.is_null()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, on_retrieve_policy_called_);
    }
  }

 private:
  base::RepeatingClosure on_retrieve_policy_called_;
};

}  // namespace

class SessionManagerOperationTest : public testing::Test {
 public:
  SessionManagerOperationTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        validated_(false) {
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
  }

  SessionManagerOperationTest(const SessionManagerOperationTest&) = delete;
  SessionManagerOperationTest& operator=(const SessionManagerOperationTest&) =
      delete;

  void SetUp() override {
    policy_.payload().mutable_user_allowlist()->add_user_allowlist(
        "fake-allowlist");
    policy_.Build();

    profile_ = std::make_unique<TestingProfile>();
    service_ =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  }

  MOCK_METHOD2(OnOperationCompleted,
               void(SessionManagerOperation*, DeviceSettingsService::Status));

  void CheckSuccessfulValidation(
      policy::DeviceCloudPolicyValidator* validator) {
    EXPECT_TRUE(validator->success());
    EXPECT_TRUE(validator->payload().get());
    EXPECT_EQ(validator->payload()->SerializeAsString(),
              policy_.payload().SerializeAsString());
    validated_ = true;
  }

  void CheckPublicKeyLoaded(SessionManagerOperation* op) {
    ASSERT_TRUE(op->public_key().get());
    ASSERT_FALSE(op->public_key()->is_empty());
    std::vector<uint8_t> public_key;
    ASSERT_TRUE(policy_.GetSigningKey()->ExportPublicKey(&public_key));
    EXPECT_EQ(public_key, op->public_key()->data());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  policy::DevicePolicyBuilder policy_;
  ObservableFakeSessionManagerClient session_manager_client_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  user_manager::TypedScopedUserManager<FakeChromeUserManager> user_manager_{
      std::make_unique<FakeChromeUserManager>()};

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<OwnerSettingsServiceAsh> service_;

  bool validated_;
};

TEST_F(SessionManagerOperationTest, LoadNoPolicyNoKey) {
  LoadSettingsOperation op(
      false /* force_key_load */, false /* force_immediate_load */,
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)));

  EXPECT_CALL(*this, OnOperationCompleted(
                         &op, DeviceSettingsService::STORE_KEY_UNAVAILABLE));
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_FALSE(op.policy_data().get());
  EXPECT_FALSE(op.device_settings().get());
  ASSERT_TRUE(op.public_key().get());
  EXPECT_TRUE(op.public_key()->is_empty());
}

TEST_F(SessionManagerOperationTest, LoadOwnerKey) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  LoadSettingsOperation op(
      false /* force_key_load */, false /* force_immediate_load */,
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)));

  EXPECT_CALL(
      *this, OnOperationCompleted(&op, DeviceSettingsService::STORE_NO_POLICY));
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  CheckPublicKeyLoaded(&op);
}

TEST_F(SessionManagerOperationTest, LoadPolicy) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  session_manager_client_.set_device_policy(policy_.GetBlob());
  LoadSettingsOperation op(
      false /* force_key_load */, false /* force_immediate_load */,
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(&op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, LoadImmediately) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  session_manager_client_.set_device_policy(policy_.GetBlob());
  LoadSettingsOperation op(
      false /* force_key_load */, true /* force_immediate_load */,
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(&op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, RestartLoad) {
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(policy_.GetSigningKey());
  session_manager_client_.set_device_policy(policy_.GetBlob());
  LoadSettingsOperation op(
      false /* force_key_load */, false /* force_immediate_load */,
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)));

  // Just after the first RetrievePolicy() completion,
  // verify the state, install a different key, then RestartLoad().
  session_manager_client_.SetOnRetrievePolicyCalled(base::BindRepeating(
      [](SessionManagerOperationTest* test, policy::DevicePolicyBuilder* policy,
         ObservableFakeSessionManagerClient* session_manager_client,
         scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util,
         LoadSettingsOperation* op) {
        // Reset this callback to avoid infinite loop.
        session_manager_client->SetOnRetrievePolicyCalled(
            base::RepeatingClosure());

        // Verify the public_key() is properly set, but the callback is
        // not yet called.
        EXPECT_TRUE(op->public_key().get());
        EXPECT_FALSE(op->public_key()->is_empty());
        Mock::VerifyAndClearExpectations(test);

        // Now install a different key and policy.
        policy->SetSigningKey(
            *policy::PolicyBuilder::CreateTestOtherSigningKey());
        policy->payload().mutable_metrics_enabled()->set_metrics_enabled(true);
        policy->Build();
        session_manager_client->set_device_policy(policy->GetBlob());
        owner_key_util->ImportPrivateKeyAndSetPublicKey(
            policy->GetSigningKey());

        // And restart the operation.
        EXPECT_CALL(*test, OnOperationCompleted(
                               op, DeviceSettingsService::STORE_SUCCESS));
        op->RestartLoad(true);
      },
      this, &policy_, &session_manager_client_, owner_key_util_, &op));

  EXPECT_CALL(*this, OnOperationCompleted(&op, _)).Times(0);
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  // Check that the new keys have been loaded.
  CheckPublicKeyLoaded(&op);

  // Verify the new policy.
  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, StoreSettings) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  StoreSettingsOperation op(
      base::BindOnce(&SessionManagerOperationTest::OnOperationCompleted,
                     base::Unretained(this)),
      policy_.GetCopy());

  EXPECT_CALL(*this,
              OnOperationCompleted(&op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&session_manager_client_, owner_key_util_, nullptr);
  content::RunAllTasksUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_EQ(session_manager_client_.device_policy(), policy_.GetBlob());
  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

}  // namespace ash
