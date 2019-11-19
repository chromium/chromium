// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using RetrievePolicyResponseType =
    chromeos::SessionManagerClient::RetrievePolicyResponseType;

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

namespace policy {

namespace {

const char kDefaultHomepage[] = "http://chromium.org";

base::FilePath GetUserPolicyKeyFile(
    const base::FilePath& user_policy_dir,
    const cryptohome::AccountIdentifier& cryptohome_id) {
  const std::string sanitized_username =
      chromeos::CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
  return user_policy_dir.AppendASCII(sanitized_username)
      .AppendASCII("policy.pub");
}

bool StoreUserPolicyKey(const base::FilePath& user_policy_dir,
                        const cryptohome::AccountIdentifier& cryptohome_id,
                        const std::string& public_key) {
  base::FilePath user_policy_key_file =
      GetUserPolicyKeyFile(user_policy_dir, cryptohome_id);
  if (!base::CreateDirectory(user_policy_key_file.DirName()))
    return false;
  int result = base::WriteFile(user_policy_key_file, public_key.data(),
                               public_key.size());
  return result == static_cast<int>(public_key.size());
}

// For detailed test for UserCloudPOlicyStoreChromeOS, this supports
// public key file update emulation.
class FakeSessionManagerClient : public chromeos::FakeSessionManagerClient {
 public:
  explicit FakeSessionManagerClient(const base::FilePath& user_policy_dir)
      : user_policy_dir_(user_policy_dir) {}

  // SessionManagerClient override:
  void StorePolicyForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                          const std::string& policy_blob,
                          chromeos::VoidDBusMethodCallback callback) override {
    chromeos::FakeSessionManagerClient::StorePolicyForUser(
        cryptohome_id, policy_blob,
        base::BindOnce(&FakeSessionManagerClient::OnStorePolicyForUser,
                       weak_ptr_factory_.GetWeakPtr(), cryptohome_id,
                       std::move(callback)));
  }

  void set_user_public_key(const cryptohome::AccountIdentifier& cryptohome_id,
                           const std::string& public_key) {
    public_key_map_[cryptohome_id] = public_key;
  }

 private:
  void OnStorePolicyForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                            chromeos::VoidDBusMethodCallback callback,
                            bool result) {
    if (result) {
      auto iter = public_key_map_.find(cryptohome_id);
      if (iter != public_key_map_.end())
        StoreUserPolicyKey(user_policy_dir_, cryptohome_id, iter->second);
    }
    std::move(callback).Run(result);
  }

  const base::FilePath user_policy_dir_;
  std::map<cryptohome::AccountIdentifier, std::string> public_key_map_;

  base::WeakPtrFactory<FakeSessionManagerClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FakeSessionManagerClient);
};

class UserCloudPolicyStoreChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyStoreChromeOSTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    session_manager_client_ =
        std::make_unique<FakeSessionManagerClient>(user_policy_dir());
    store_.reset(new UserCloudPolicyStoreChromeOS(
        &cryptohome_client_, session_manager_client_.get(),
        base::ThreadTaskRunnerHandle::Get(), account_id_, user_policy_dir(),
        false /* is_active_directory */));
    store_->AddObserver(&observer_);

    // Install the initial public key, so that by default the validation of
    // the stored/loaded policy blob succeeds.
    std::string public_key = policy_.GetPublicSigningKeyAsString();
    ASSERT_FALSE(public_key.empty());
    ASSERT_TRUE(
        StoreUserPolicyKey(user_policy_dir(), cryptohome_id_, public_key));

    policy_.payload().mutable_homepagelocation()->set_value(kDefaultHomepage);
    policy_.Build();
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
  }

  // Install an expectation on |observer_| for an error code.
  // This method should be called after a store->Store()/Load() call has been
  // initiated. The expected OnStoreError call will be initiated asynchronously
  // from this run_loop's iteration.
  void RunLoopAndExpectError(CloudPolicyStore::Status error) {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnStoreError(AllOf(
                               Eq(store_.get()),
                               Property(&CloudPolicyStore::status, Eq(error)))))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer_);
  }

  // Install an expectation on |observer_| for a successful load operation.
  // This method should be called after a store->Store()/Load() call has been
  // initiated. The expected OnStoreLoaded call will be initiated asynchronously
  // from this run_loop's iteration.
  void RunLoopAndExpectLoaded() {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get()))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer_);
  }

  // Triggers a store_->Load() operation, handles the expected call to
  // |session_manager_client_| and sends |response|.
  void PerformPolicyLoad(const std::string& response) {
    // Issue a load command.
    session_manager_client_->set_user_policy(cryptohome_id_, response);
    store_->Load();
  }

  // Verifies that store_->policy_map() has the HomepageLocation entry with
  // the |expected_value|.
  void VerifyPolicyMap(const char* expected_value) {
    EXPECT_EQ(1U, store_->policy_map().size());
    const PolicyMap::Entry* entry =
        store_->policy_map().Get(key::kHomepageLocation);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::Value(expected_value).Equals(entry->value.get()));
  }

  // Stores the current |policy_| and verifies that it is published.
  // If |previous_value| is set then a previously existing policy with that
  // value will be expected; otherwise no previous policy is expected.
  // If |new_value| is set then a new policy with that value is expected after
  // storing the |policy_| blob.
  void PerformStorePolicy(const char* previous_value, const char* new_value) {
    const CloudPolicyStore::Status initial_status = store_->status();

    store_->Store(policy_.policy());

    // The new policy shouldn't be present yet.
    PolicyMap previous_policy;
    EXPECT_EQ(previous_value != nullptr, store_->policy() != nullptr);
    if (previous_value) {
      previous_policy.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                          std::make_unique<base::Value>(previous_value),
                          nullptr);
    }
    EXPECT_TRUE(previous_policy.Equals(store_->policy_map()));
    EXPECT_EQ(initial_status, store_->status());

    RunLoopAndExpectLoaded();
    ASSERT_TRUE(store_->policy());
    EXPECT_EQ(policy_.policy_data().SerializeAsString(),
              store_->policy()->SerializeAsString());
    VerifyPolicyMap(new_value);
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  }

  void VerifyStoreHasValidationError() {
    EXPECT_FALSE(store_->policy());
    EXPECT_TRUE(store_->policy_map().empty());
    EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  }

  base::FilePath user_policy_dir() {
    return tmp_dir_.GetPath().AppendASCII("var_run_user_policy");
  }

  base::FilePath user_policy_key_file() {
    return GetUserPolicyKeyFile(user_policy_dir(), cryptohome_id_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  chromeos::FakeCryptohomeClient cryptohome_client_;
  std::unique_ptr<FakeSessionManagerClient> session_manager_client_;
  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  std::unique_ptr<UserCloudPolicyStoreChromeOS> store_;
  const AccountId account_id_ =
      AccountId::FromUserEmail(PolicyBuilder::kFakeUsername);
  const cryptohome::AccountIdentifier cryptohome_id_ =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);

 private:
  base::ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOSTest);
};

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStore) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));

  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  session_manager_client_->set_user_public_key(cryptohome_id_, new_public_key);
  ASSERT_NO_FATAL_FAILURE(PerformStorePolicy(nullptr, kDefaultHomepage));
  EXPECT_EQ(new_public_key, store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreValidationFail) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature_deprecated() =
      "garbage";

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreMissingSignatureFailure) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature_deprecated();

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithExistingKey) {
  ASSERT_NO_FATAL_FAILURE(PerformStorePolicy(nullptr, kDefaultHomepage));
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotation) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  session_manager_client_->set_user_public_key(cryptohome_id_, new_public_key);
  ASSERT_NO_FATAL_FAILURE(PerformStorePolicy(nullptr, kDefaultHomepage));
  EXPECT_EQ(new_public_key, store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest,
       StoreWithRotationMissingSignatureError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature_deprecated();

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotationValidationError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature_deprecated() =
      "garbage";

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreFail) {
  // Let store policy fail.
  session_manager_client_->ForceStorePolicyFailure(true);

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_STORE_ERROR);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreValueValidationError) {
  std::string onc_policy = chromeos::onc::test_utils::ReadTestData(
      "toplevel_with_unknown_fields.onc");
  policy_.payload().mutable_opennetworkconfiguration()->set_value(onc_policy);
  policy_.Build();

  store_->Store(policy_.policy());
  RunLoopAndExpectLoaded();

  const CloudPolicyValidatorBase::ValidationResult* validation_result =
      store_->validation_result();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  ASSERT_TRUE(validation_result);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_OK, validation_result->status);
  EXPECT_EQ(3u, validation_result->value_validation_issues.size());
  EXPECT_EQ(policy_.policy_data().policy_token(),
            validation_result->policy_token);
  EXPECT_EQ(policy_.policy().policy_data_signature(),
            validation_result->policy_data_signature);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithoutPolicyKey) {
  // Make the dbus call to cryptohome fail.
  cryptohome_client_.SetServiceIsAvailable(false);

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");

  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MultipleStoresWithRotation) {
  // Store initial policy signed with the initial public key.
  ASSERT_NO_FATAL_FAILURE(PerformStorePolicy(nullptr, kDefaultHomepage));
  const std::string initial_public_key = policy_.GetPublicSigningKeyAsString();
  EXPECT_EQ(initial_public_key, store_->policy_signature_public_key());

  // Try storing an invalid policy signed with the new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy
  store_->Store(policy_.policy());
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);

  // Still the initial public key is exposed.
  EXPECT_EQ(initial_public_key, store_->policy_signature_public_key());

  // Store the correct policy signed with the new public key.
  policy_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_.Build();
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  session_manager_client_->set_user_public_key(cryptohome_id_, new_public_key);
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(kDefaultHomepage, kDefaultHomepage));
  EXPECT_EQ(policy_.GetPublicNewSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, Load) {
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  RunLoopAndExpectLoaded();

  // Verify that the policy has been loaded.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoPolicy) {
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  RunLoopAndExpectLoaded();

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidPolicy) {
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad("invalid"));
  RunLoopAndExpectError(CloudPolicyStore::STATUS_PARSE_ERROR);

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoKey) {
  // The loaded policy can't be verified without the public key.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  RunLoopAndExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediately) {
  session_manager_client_->set_user_policy(cryptohome_id_, policy_.GetBlob());

  EXPECT_FALSE(store_->policy());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->LoadImmediately();
  // Note: verify that the |observer_| got notified synchronously, without
  // having to spin the current loop. TearDown() will flush the loop so this
  // must be done within the test.
  Mock::VerifyAndClearExpectations(&observer_);

  // The policy should become available without having to spin any loops.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoPolicy) {
  EXPECT_FALSE(store_->policy());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyInvalidBlob) {
  session_manager_client_->set_user_policy(cryptohome_id_, "le blob");

  EXPECT_FALSE(store_->policy());

  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyDBusFailure) {
  session_manager_client_->set_user_policy(cryptohome_id_, policy_.GetBlob());

  // Make the dbus call to cryptohome fail.
  cryptohome_client_.SetServiceIsAvailable(false);

  EXPECT_FALSE(store_->policy());

  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoUserPolicyKey) {
  session_manager_client_->set_user_policy(cryptohome_id_, policy_.GetBlob());

  // Ensure no policy data.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  EXPECT_FALSE(store_->policy());

  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

}  // namespace

}  // namespace policy
