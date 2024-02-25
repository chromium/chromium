// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/dm_token_storage.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace policy {
namespace {

using ::ash::FakeCryptohomeMiscClient;
using ::base::test::TestFuture;

}  // namespace

class DMTokenStorageTest : public testing::Test {
 public:
  DMTokenStorageTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~DMTokenStorageTest() override {}

  std::string GetStubSaltAsString() {
    std::vector<uint8_t> bytes = FakeCryptohomeMiscClient::GetStubSystemSalt();
    return std::string(bytes.begin(), bytes.end());
  }

  void SetSaltPending() {
    // Clear the cached salt.
    ash::SystemSaltGetter::Shutdown();
    FakeCryptohomeMiscClient::Get()->set_system_salt(std::vector<uint8_t>());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);
    ash::SystemSaltGetter::Initialize();
  }

  void SetSaltAvailable() {
    FakeCryptohomeMiscClient::Get()->set_system_salt(
        FakeCryptohomeMiscClient::GetStubSystemSalt());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  }

  void SetSaltError() {
    FakeCryptohomeMiscClient::Get()->set_system_salt(std::vector<uint8_t>());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  }

  void SetUp() override {
    ash::CryptohomeMiscClient::InitializeFake();
    SetSaltAvailable();

    ash::SystemSaltGetter::Initialize();
  }

  void TearDown() override {
    dm_token_storage_.reset();
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void CreateDMStorage() {
    dm_token_storage_ =
        std::make_unique<DMTokenStorage>(scoped_testing_local_state_.Get());
  }

  void StoreDMToken() {
    TestFuture<bool> store_result_future;
    dm_token_storage_->StoreDMToken("test-token",
                                    store_result_future.GetCallback());
    EXPECT_TRUE(store_result_future.Get());
    EXPECT_TRUE(scoped_testing_local_state_.Get()
                    ->GetString(prefs::kDeviceDMTokenV1)
                    .empty());
    EXPECT_FALSE(scoped_testing_local_state_.Get()
                     ->GetString(prefs::kDeviceDMTokenV2)
                     .empty());
  }

  void StoreV1Token(const std::string& token) {
    ash::CryptohomeTokenEncryptor encryptor(GetStubSaltAsString());
    scoped_testing_local_state_.Get()->SetString(
        prefs::kDeviceDMTokenV1, encryptor.WeakEncryptWithSystemSalt(token));
  }

  void StoreV2Token(const std::string& token) {
    ash::CryptohomeTokenEncryptor encryptor(GetStubSaltAsString());
    scoped_testing_local_state_.Get()->SetString(
        prefs::kDeviceDMTokenV2, encryptor.EncryptWithSystemSalt(token));
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<DMTokenStorage> dm_token_storage_;
};

TEST_F(DMTokenStorageTest, SaveEncryptedToken) {
  CreateDMStorage();
  StoreDMToken();

  TestFuture<std::string> token_future1;
  dm_token_storage_->RetrieveDMToken(
      token_future1.GetCallback<const std::string&>());
  EXPECT_EQ(token_future1.Get(), "test-token");

  // Reload shouldn't change the token.
  CreateDMStorage();
  TestFuture<std::string> token_future2;
  dm_token_storage_->RetrieveDMToken(
      token_future2.GetCallback<const std::string&>());
  EXPECT_EQ(token_future2.Get(), "test-token");

  // Subsequent retrieving DM token should succeed.
  TestFuture<std::string> token_future3;
  dm_token_storage_->RetrieveDMToken(
      token_future3.GetCallback<const std::string&>());
  EXPECT_EQ(token_future3.Get(), "test-token");
}

TEST_F(DMTokenStorageTest, RetrieveV1Token) {
  StoreV1Token("test-token");
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  EXPECT_EQ(token_future.Get(), "test-token");
}

TEST_F(DMTokenStorageTest, RetrieveV2Token) {
  StoreV2Token("test-token");
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  EXPECT_EQ(token_future.Get(), "test-token");
}

TEST_F(DMTokenStorageTest, RetrievePrefersV2Token) {
  StoreV1Token("test-token-v1");
  StoreV2Token("test-token-v2");
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  EXPECT_EQ(token_future.Get(), "test-token-v2");
}

TEST_F(DMTokenStorageTest, RetrieveEncryptedTokenWithPendingSalt) {
  CreateDMStorage();
  StoreDMToken();

  SetSaltPending();
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  SetSaltAvailable();
  EXPECT_EQ(token_future.Get(), "test-token");
}

TEST_F(DMTokenStorageTest, StoreEncryptedTokenWithPendingSalt) {
  SetSaltPending();
  CreateDMStorage();

  TestFuture<bool> store_result_future;
  dm_token_storage_->StoreDMToken("test-token",
                                  store_result_future.GetCallback());
  SetSaltAvailable();
  EXPECT_TRUE(store_result_future.Get());
}

TEST_F(DMTokenStorageTest, MultipleRetrieveTokenCalls) {
  CreateDMStorage();
  StoreDMToken();

  std::vector<TestFuture<std::string>> token_futures(3);
  for (TestFuture<std::string>& token_future : token_futures) {
    dm_token_storage_->RetrieveDMToken(
        token_future.GetCallback<const std::string&>());
  }
  for (TestFuture<std::string>& token_future : token_futures) {
    EXPECT_EQ(token_future.Get(), "test-token");
  }
}

TEST_F(DMTokenStorageTest, StoreWithSaltError) {
  SetSaltError();
  CreateDMStorage();

  TestFuture<bool> store_result_future;
  dm_token_storage_->StoreDMToken("test-token",
                                  store_result_future.GetCallback());
  EXPECT_FALSE(store_result_future.Get());
}

TEST_F(DMTokenStorageTest, RetrieveWithSaltError) {
  CreateDMStorage();
  StoreDMToken();
  SetSaltPending();
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  SetSaltError();
  EXPECT_EQ(token_future.Get(), "");
}

TEST_F(DMTokenStorageTest, RetrieveWithNoToken) {
  CreateDMStorage();

  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  EXPECT_EQ(token_future.Get(), "");
}

TEST_F(DMTokenStorageTest, RetrieveFailIfStoreRunning) {
  SetSaltPending();
  CreateDMStorage();

  TestFuture<bool> store_result_future;
  dm_token_storage_->StoreDMToken("test-token",
                                  store_result_future.GetCallback());
  TestFuture<std::string> token_future;
  dm_token_storage_->RetrieveDMToken(
      token_future.GetCallback<const std::string&>());
  SetSaltAvailable();
  EXPECT_TRUE(store_result_future.Get());
  EXPECT_EQ(token_future.Get(), "");
}

TEST_F(DMTokenStorageTest, StoreFailIfAnotherStoreRunning) {
  SetSaltPending();
  CreateDMStorage();

  TestFuture<bool> first_store_result_future;
  TestFuture<bool> later_store_result_future;
  dm_token_storage_->StoreDMToken("test-token",
                                  first_store_result_future.GetCallback());
  dm_token_storage_->StoreDMToken("test-token",
                                  later_store_result_future.GetCallback());
  SetSaltAvailable();
  EXPECT_TRUE(first_store_result_future.Get());
  EXPECT_FALSE(later_store_result_future.Get());
}

}  // namespace policy
