// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/pre_signin_policy_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::WithArg;
using ::testing::WithArgs;
using ::testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

// Dummy URLs to distinguish between cached and fresh policy.
const char kCachedHomepage[] = "http://cached.test";
const char kFreshHomepage[] = "http://fresh.test";

class PreSigninPolicyFetcherTestBase : public testing::Test {
 protected:
  PreSigninPolicyFetcherTestBase() = default;

  void SetUp() override {
    // Unmount calls will succeed (currently, PreSigninPolicyFetcher only logs
    // if they fail, so there is no point in testing that).
    cryptohome_client_ = std::make_unique<chromeos::FakeCryptohomeClient>();
    cryptohome_client_->set_unmount_result(true);

    // Create a temporary directory where the user policy keys will live (these
    // are shared between session_manager and chrome through files) and set it
    // into PathService, so PreSigninPolicyFetcher will use it.
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    base::PathService::Override(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                                user_policy_keys_dir());

    auto cloud_policy_client = std::make_unique<MockCloudPolicyClient>();
    cloud_policy_client_ = cloud_policy_client.get();
    pre_signin_policy_fetcher_ = std::make_unique<PreSigninPolicyFetcher>(
        cryptohome_client_.get(), &session_manager_client_,
        std::move(cloud_policy_client), IsActiveDirectoryManaged(),
        GetAccountId(), cryptohome_key_);
    cached_policy_.payload().mutable_homepagelocation()->set_value(
        kCachedHomepage);
    cached_policy_.Build();

    fresh_policy_.payload().mutable_homepagelocation()->set_value(
        kFreshHomepage);
    fresh_policy_.Build();
  }

  void TearDown() override {
    cryptohome_client_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Returns true for Active Directory test, false otherwise.
  virtual bool IsActiveDirectoryManaged() const = 0;

  // Returns the AccountId to be used during the test. This will differ between
  // regular gaia user and AD user.
  virtual const AccountId& GetAccountId() const = 0;

  cryptohome::AccountIdentifier GetCryptohomeAccountIdentifier() const {
    return cryptohome::CreateAccountIdentifierFromAccountId(GetAccountId());
  }

  void StoreUserPolicyKey(const std::string& public_key) {
    ASSERT_TRUE(base::CreateDirectory(user_policy_key_file().DirName()));
    ASSERT_EQ(static_cast<int>(public_key.size()),
              base::WriteFile(user_policy_key_file(), public_key.data(),
                              public_key.size()));
  }

  base::FilePath user_policy_keys_dir() const {
    return tmp_dir_.GetPath().AppendASCII("var_run_user_policy");
  }

  base::FilePath user_policy_key_file() const {
    const std::string sanitized_username =
        chromeos::CryptohomeClient::GetStubSanitizedUsername(
            GetCryptohomeAccountIdentifier());
    return user_policy_keys_dir()
        .AppendASCII(sanitized_username)
        .AppendASCII("policy.pub");
  }

  // Sets up expectations on |cloud_policy_client_|, expecting a fresh policy
  // fetch call sequence.
  void ExpectFreshPolicyFetchOnClient(const std::string& dm_token,
                                      const std::string& client_id) {
    EXPECT_CALL(*cloud_policy_client_,
                SetupRegistration(dm_token, client_id,
                                  PolicyBuilder::GetUserAffiliationIds()));
    EXPECT_CALL(*cloud_policy_client_, FetchPolicy());

    expecting_fresh_policy_fetch_ = true;
  }

  // Sets up expectations on |cloud_policy_client_|, expecting that no fresh
  // policy fetch will be invoked.
  void ExpectNoFreshPolicyFetchOnClient() {
    EXPECT_CALL(*cloud_policy_client_, FetchPolicy()).Times(0);

    expecting_fresh_policy_fetch_ = false;
  }

  void VerifyExpectationsOnClient() {
    // Verify that the expected method calls happened.
    Mock::VerifyAndClearExpectations(cloud_policy_client_);

    if (expecting_fresh_policy_fetch_) {
      // Verify that the public key version from the cached policy has been
      // passed to CloudPolicyClient for the fresh request.
      EXPECT_TRUE(cloud_policy_client_->public_key_version_valid_);
      EXPECT_EQ(cached_policy_.policy_data().public_key_version(),
                cloud_policy_client_->public_key_version_);
    }
  }

  void OnPolicyRetrieved(
      PreSigninPolicyFetcher::PolicyFetchResult result,
      std::unique_ptr<em::CloudPolicySettings> policy_payload) {
    ASSERT_FALSE(policy_retrieved_called_);
    policy_retrieved_called_ = true;
    obtained_policy_fetch_result_ = result;
    obtained_policy_payload_ = std::move(policy_payload);
  }

  void ExecuteFetchPolicy() {
    pre_signin_policy_fetcher_->FetchPolicy(
        base::Bind(&PreSigninPolicyFetcherTestBase::OnPolicyRetrieved,
                   base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<chromeos::FakeCryptohomeClient> cryptohome_client_;
  chromeos::FakeSessionManagerClient session_manager_client_;
  UserPolicyBuilder cached_policy_;
  UserPolicyBuilder fresh_policy_;
  const cryptohome::KeyDefinition cryptohome_key_ =
      cryptohome::KeyDefinition::CreateForPassword("secret",
                                                   std::string() /* label */,
                                                   cryptohome::PRIV_DEFAULT);

  MockCloudPolicyClient* cloud_policy_client_ = nullptr;
  std::unique_ptr<PreSigninPolicyFetcher> pre_signin_policy_fetcher_;

  bool policy_retrieved_called_ = false;
  PreSigninPolicyFetcher::PolicyFetchResult obtained_policy_fetch_result_;
  std::unique_ptr<em::CloudPolicySettings> obtained_policy_payload_;

 private:
  base::ScopedTempDir tmp_dir_;

  bool expecting_fresh_policy_fetch_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreSigninPolicyFetcherTestBase);
};

// Tests for PreSigninPolicyFetcher with a regular gaia account.
class PreSigninPolicyFetcherTest : public PreSigninPolicyFetcherTestBase {
 protected:
  bool IsActiveDirectoryManaged() const override { return false; }

  const AccountId& GetAccountId() const override { return account_id_; }

 private:
  const AccountId account_id_ =
      AccountId::FromUserEmail(PolicyBuilder::kFakeUsername);
};

// Test that we successfully determine that the user has no policy (unmanaged
// user). The cached policy fetch succeeds with NO_POLICY.
// PreSigninPolicyFetcher does not attempt to fetch fresh policy.
TEST_F(PreSigninPolicyFetcherTest, NoPolicy) {
  // session_manager's RetrievePolicy* methods signal that there is no policy by
  // passing an empty string as policy blob.
  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), std::string());

  ExpectNoFreshPolicyFetchOnClient();
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::NO_POLICY,
            obtained_policy_fetch_result_);
  EXPECT_FALSE(obtained_policy_payload_);
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Test that PreSigninPolicyFetcher signals an error when the temporary
// cryptohome mount fails.
TEST_F(PreSigninPolicyFetcherTest, CryptohomeTemporaryMountError) {
  cryptohome_client_->set_cryptohome_error(
      cryptohome::CryptohomeErrorCode::
          CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED);

  ExecuteFetchPolicy();

  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::ERROR,
            obtained_policy_fetch_result_);
  EXPECT_FALSE(obtained_policy_payload_);
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Break the signature of cached policy. We expect that the cached policy
// fails to validate as a consequence and thus we get a
// PolicyFetchResult::ERROR as response. PreSigninPolicyFetcher will not
// attempt to fetch fresh policy in this case.
TEST_F(PreSigninPolicyFetcherTest, CachedPolicyFailsToValidate) {
  cached_policy_.policy().mutable_policy_data_signature()->append("garbage");

  StoreUserPolicyKey(cached_policy_.GetPublicSigningKeyAsString());

  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectNoFreshPolicyFetchOnClient();
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::ERROR,
            obtained_policy_fetch_result_);
  EXPECT_FALSE(obtained_policy_payload_);
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Don't call StoreUserPolicyKey - chrome won't find a cached policy key. We
// expect that the cached policy fails to validate and thus we get a
// PolicyFetchResult::ERROR as response. PreSigninPolicyFetcher will not
// attempt to fetch fresh policy in this case.
TEST_F(PreSigninPolicyFetcherTest, NoCachedPolicyKeyAccessible) {
  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectNoFreshPolicyFetchOnClient();
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::ERROR,
            obtained_policy_fetch_result_);
  EXPECT_FALSE(obtained_policy_payload_);
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Cached policy is available and validates. However, fresh policy fetch fails
// with a CloudPolicyClient error. Expect that PreSigninPolicyFetcher will
// report a PolicyFetchResult::SUCCESS and pass the cached policy to the
// callback.
TEST_F(PreSigninPolicyFetcherTest, FreshPolicyFetchFails) {
  StoreUserPolicyKey(cached_policy_.GetPublicSigningKeyAsString());
  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectFreshPolicyFetchOnClient(PolicyBuilder::kFakeToken,
                                 PolicyBuilder::kFakeDeviceId);
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  // Fresh policy fetch fails with a CloudPolicyClient error.
  cloud_policy_client_->NotifyClientError();

  // Expect that we still get PolicyFetchResult::SUCCESS with the cached policy.
  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::SUCCESS,
            obtained_policy_fetch_result_);
  EXPECT_TRUE(obtained_policy_payload_);
  EXPECT_EQ(kCachedHomepage,
            obtained_policy_payload_->homepagelocation().value());
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Cached policy is available and validates. However, fresh policy fetch fails
// with timeout. Expect that PreSigninPolicyFetcher will report a
// PolicyFetchResult::SUCCESS and pass the cached policy to the callback.
TEST_F(PreSigninPolicyFetcherTest, FreshPolicyFetchTimeout) {
  StoreUserPolicyKey(cached_policy_.GetPublicSigningKeyAsString());

  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectFreshPolicyFetchOnClient(PolicyBuilder::kFakeToken,
                                 PolicyBuilder::kFakeDeviceId);
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  // Fresh policy fetch times out.
  EXPECT_TRUE(pre_signin_policy_fetcher_->ForceTimeoutForTesting());
  // Expect that we still get PolicyFetchResult::SUCCESS with the cached policy.
  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::SUCCESS,
            obtained_policy_fetch_result_);
  EXPECT_TRUE(obtained_policy_payload_);
  EXPECT_EQ(kCachedHomepage,
            obtained_policy_payload_->homepagelocation().value());
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Cached policy is available and validates. Fresh policy fetch is also
// successful, but the fresh policy fails to validate. Expect that
// PreSigninPolicyFetcher will report a PolicyFetchResult::SUCCESS and pass
// the cached policy to the callback.
TEST_F(PreSigninPolicyFetcherTest, FreshPolicyFetchFailsToValidate) {
  StoreUserPolicyKey(cached_policy_.GetPublicSigningKeyAsString());

  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectFreshPolicyFetchOnClient(PolicyBuilder::kFakeToken,
                                 PolicyBuilder::kFakeDeviceId);
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  // Fresh policy fetch is successful but returns a policy blob with a broken
  // signature, so the fresh policy fails to validate.
  fresh_policy_.policy().mutable_policy_data_signature()->append("garbage");
  cloud_policy_client_->SetPolicy(dm_protocol::kChromeUserPolicyType,
                                  std::string() /* settings_entity_id */,
                                  fresh_policy_.policy());
  cloud_policy_client_->NotifyPolicyFetched();
  task_environment_.RunUntilIdle();

  // Expect that we still get a PolicyFetchResult::SUCCESS with cached policy.
  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::SUCCESS,
            obtained_policy_fetch_result_);
  EXPECT_TRUE(obtained_policy_payload_);
  EXPECT_EQ(kCachedHomepage,
            obtained_policy_payload_->homepagelocation().value());
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Cached policy is available and validates. Fresh policy fetch is also
// successful and the fresh policy validates. Expect that
// PreSigninPolicyFetcher will report a PolicyFetchResult::SUCCESS and pass
// the fresh policy to the callback.
TEST_F(PreSigninPolicyFetcherTest, FreshPolicyFetchSuccess) {
  StoreUserPolicyKey(cached_policy_.GetPublicSigningKeyAsString());

  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectFreshPolicyFetchOnClient(PolicyBuilder::kFakeToken,
                                 PolicyBuilder::kFakeDeviceId);
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  // Fresh policy fetch is successful and validates.
  cloud_policy_client_->SetPolicy(dm_protocol::kChromeUserPolicyType,
                                  std::string() /* settings_entity_id */,
                                  fresh_policy_.policy());
  cloud_policy_client_->NotifyPolicyFetched();
  task_environment_.RunUntilIdle();

  // Expect that we get PolicyFetchResult::SUCCESS with fresh policy.
  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::SUCCESS,
            obtained_policy_fetch_result_);
  EXPECT_TRUE(obtained_policy_payload_);
  EXPECT_EQ(kFreshHomepage,
            obtained_policy_payload_->homepagelocation().value());
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

// Tests for PreSigninPolicyFetcher with an Active Directory account.
class PreSigninPolicyFetcherTestAD : public PreSigninPolicyFetcherTestBase {
 protected:
  bool IsActiveDirectoryManaged() const override { return true; }

  const AccountId& GetAccountId() const override { return account_id_; }

 private:
  const AccountId account_id_ =
      AccountId::AdFromUserEmailObjGuid(PolicyBuilder::kFakeUsername, "guid");
};

// For Active Directory, we only have unsigned cached policy. There is no policy
// key and no fresh policy fetch is attempted currently.
TEST_F(PreSigninPolicyFetcherTestAD, UnsignedCachedPolicyForActiveDirectory) {
  session_manager_client_.set_user_policy_without_session(
      GetCryptohomeAccountIdentifier(), cached_policy_.GetBlob());

  ExpectNoFreshPolicyFetchOnClient();
  ExecuteFetchPolicy();

  VerifyExpectationsOnClient();

  EXPECT_TRUE(policy_retrieved_called_);
  EXPECT_EQ(PreSigninPolicyFetcher::PolicyFetchResult::SUCCESS,
            obtained_policy_fetch_result_);
  EXPECT_TRUE(obtained_policy_payload_);
  EXPECT_EQ(kCachedHomepage,
            obtained_policy_payload_->homepagelocation().value());
  EXPECT_TRUE(cryptohome_client_->hidden_mount());
  // Expect regular user home (not public mount). Note that ARC
  // Kiosk apps will not run through PreSigninPolicyFetcher.
  EXPECT_FALSE(cryptohome_client_->public_mount());
}

}  // namespace
}  // namespace policy
