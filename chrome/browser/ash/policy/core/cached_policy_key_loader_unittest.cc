// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/cached_policy_key_loader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kDummyKey1[] = "dummy-key-1";
const char kDummyKey2[] = "dummy-key-2";
const char kTestUserName[] = "test-user@example.com";

class CachedPolicyKeyLoaderTest : public testing::Test {
 protected:
  CachedPolicyKeyLoaderTest() = default;

  CachedPolicyKeyLoaderTest(const CachedPolicyKeyLoaderTest&) = delete;
  CachedPolicyKeyLoaderTest& operator=(const CachedPolicyKeyLoaderTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    cached_policy_key_loader_ = std::make_unique<CachedPolicyKeyLoader>(
        &cryptohome_misc_client_, task_environment_.GetMainThreadTaskRunner(),
        account_id_, user_policy_keys_dir());
  }

  void StoreUserPolicyKey(const std::string& public_key) {
    ASSERT_TRUE(base::CreateDirectory(user_policy_key_file().DirName()));
    ASSERT_TRUE(base::WriteFile(user_policy_key_file(), public_key));
  }

  base::FilePath user_policy_keys_dir() const {
    return tmp_dir_.GetPath().AppendASCII("var_run_user_policy");
  }

  base::FilePath user_policy_key_file() const {
    const std::string sanitized_username =
        ash::UserDataAuthClient::GetStubSanitizedUsername(cryptohome_id_);
    return user_policy_keys_dir()
        .AppendASCII(sanitized_username)
        .AppendASCII("policy.pub");
  }

  void OnPolicyKeyLoaded() { ++policy_key_loaded_callback_invocations_; }

  void CallEnsurePolicyKeyLoaded() {
    cached_policy_key_loader_->EnsurePolicyKeyLoaded(base::BindOnce(
        &CachedPolicyKeyLoaderTest::OnPolicyKeyLoaded, base::Unretained(this)));
  }

  void CallReloadPolicyKey() {
    cached_policy_key_loader_->ReloadPolicyKey(base::BindOnce(
        &CachedPolicyKeyLoaderTest::OnPolicyKeyLoaded, base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  ash::FakeCryptohomeMiscClient cryptohome_misc_client_;
  const AccountId account_id_ = AccountId::FromUserEmail(kTestUserName);
  const cryptohome::AccountIdentifier cryptohome_id_ =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);

  std::unique_ptr<CachedPolicyKeyLoader> cached_policy_key_loader_;

  // Counts how many times OnPolicyKeyLoaded() has been invoked.
  int policy_key_loaded_callback_invocations_ = 0;

 private:
  base::ScopedTempDir tmp_dir_;
};

// Loads an existing key file using EnsurePolicyKeyLoaded.
TEST_F(CachedPolicyKeyLoaderTest, Basic) {
  StoreUserPolicyKey(kDummyKey1);

  CallEnsurePolicyKeyLoaded();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());
}

// Tries to load key using EnsurePolicyKeyLoaded, but the key is missing.
TEST_F(CachedPolicyKeyLoaderTest, KeyFileMissing) {
  CallEnsurePolicyKeyLoaded();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(std::string(), cached_policy_key_loader_->cached_policy_key());
}

// Loads an existing key file using EnsurePolicyKeyLoaded. While the load is in
// progress, EnsurePolicyKeyLoaded is called again.
TEST_F(CachedPolicyKeyLoaderTest, EnsureCalledTwice) {
  StoreUserPolicyKey(kDummyKey1);

  CallEnsurePolicyKeyLoaded();
  CallEnsurePolicyKeyLoaded();

  EXPECT_EQ(0, policy_key_loaded_callback_invocations_);

  task_environment_.RunUntilIdle();

  // We expect that the callback was called for each EnsurePolicyKeyLoaded
  // invocation.
  EXPECT_EQ(2, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());
}

// After a successful load, changes the policy key file and calls
// EnsurePolicyKeyLoaded.
TEST_F(CachedPolicyKeyLoaderTest, EnsureAfterSuccessfulLoad) {
  StoreUserPolicyKey(kDummyKey1);

  CallEnsurePolicyKeyLoaded();
  EXPECT_EQ(0, policy_key_loaded_callback_invocations_);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());

  // Change the policy key file.
  StoreUserPolicyKey(kDummyKey2);

  CallEnsurePolicyKeyLoaded();

  task_environment_.RunUntilIdle();

  // We expect that the callback was invoked, but that the cached policy key is
  // still the old one. EnsurePolicyKeyLoaded is not supposed to reload the key.
  EXPECT_EQ(2, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());
}

// After a successful load, changes the policy key file and calls
// ReloadPolicyKey.
TEST_F(CachedPolicyKeyLoaderTest, ReloadAfterEnsure) {
  StoreUserPolicyKey(kDummyKey1);

  CallEnsurePolicyKeyLoaded();
  EXPECT_EQ(0, policy_key_loaded_callback_invocations_);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());

  // Change the policy key file.
  StoreUserPolicyKey(kDummyKey2);

  CallReloadPolicyKey();

  task_environment_.RunUntilIdle();

  // We expect that the callback was invoked, and that the policy key file has
  // been reloded, so the cached policy key is now the new policy key.
  EXPECT_EQ(2, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey2, cached_policy_key_loader_->cached_policy_key());
}

// During a load, ReloadPolicyKeyFile is invoked.
TEST_F(CachedPolicyKeyLoaderTest, ReloadWhileLoading) {
  StoreUserPolicyKey(kDummyKey1);

  CallEnsurePolicyKeyLoaded();
  CallReloadPolicyKey();
  EXPECT_EQ(0, policy_key_loaded_callback_invocations_);

  task_environment_.RunUntilIdle();

  // We expect that the callback was called for both the EnsurePolicyKeyLoaded
  // and ReloadPolicyKey invocation.
  EXPECT_EQ(2, policy_key_loaded_callback_invocations_);
  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());
}

// Synchronous load on the caller's thread.
TEST_F(CachedPolicyKeyLoaderTest, LoadImmediately) {
  StoreUserPolicyKey(kDummyKey1);

  cached_policy_key_loader_->LoadPolicyKeyImmediately();

  EXPECT_EQ(kDummyKey1, cached_policy_key_loader_->cached_policy_key());
}

}  // namespace

}  // namespace policy
