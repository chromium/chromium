// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestRobotEmail[] = "foo@system.gserviceaccount.com";
const char kTestOtherRobotEmail[] = "bar@system.gserviceaccount.com";

class TestObserver : public DeviceOAuth2TokenStore::Observer {
 public:
  int called_count() const { return called_count_; }

 private:
  void OnRefreshTokenAvailable() override { ++called_count_; }

  int called_count_ = 0;
};
}  // namespace

class DeviceOAuth2TokenStoreDesktopTest : public testing::Test {
 public:
  DeviceOAuth2TokenStoreDesktopTest()
      : os_crypt_async_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

  ~DeviceOAuth2TokenStoreDesktopTest() override = default;

  std::optional<os_crypt_async::Encryptor> GetTestEncryptorForTesting() {
    std::optional<os_crypt_async::Encryptor> encryptor;
    os_crypt_async_->GetInstance(base::BindLambdaForTesting(
        [&](os_crypt_async::Encryptor new_encryptor) {
          encryptor = std::move(new_encryptor);
        }));
    return encryptor;
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  os_crypt_async::OSCryptAsync* os_crypt_async() {
    return os_crypt_async_.get();
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

TEST_F(DeviceOAuth2TokenStoreDesktopTest, InitWithoutSavedToken) {
  DeviceOAuth2TokenStoreDesktop store(local_state(), os_crypt_async());

  EXPECT_TRUE(store.GetAccountId().empty());
  EXPECT_TRUE(store.GetRefreshToken().empty());

  store.Init(base::BindOnce([](bool, bool) {}));

  TestObserver observer;
  store.SetObserver(&observer);

  // Observer shouldn't have been called because there's no account ID available
  // yet.
  EXPECT_EQ(0, observer.called_count());
  EXPECT_TRUE(store.GetAccountId().empty());
  EXPECT_TRUE(store.GetRefreshToken().empty());
}

TEST_F(DeviceOAuth2TokenStoreDesktopTest, InitWithSavedToken) {
  local_state()->SetString(kCBCMServiceAccountEmail, kTestRobotEmail);

  std::string token = "test_token";
  std::string encrypted_token;
  auto encryptor = GetTestEncryptorForTesting();
  ASSERT_TRUE(encryptor.has_value());
  EXPECT_TRUE(encryptor->EncryptString(token, &encrypted_token));

  std::string encoded = base::Base64Encode(encrypted_token);

  local_state()->SetString(kCBCMServiceAccountRefreshToken, encoded);

  DeviceOAuth2TokenStoreDesktop store(local_state(), os_crypt_async());

  EXPECT_TRUE(store.GetRefreshToken().empty());

  TestObserver observer;
  store.SetObserver(&observer);

  store.Init(base::BindOnce([](bool, bool) {}));

  EXPECT_EQ(1, observer.called_count());
  EXPECT_EQ(store.GetAccountId(),
            CoreAccountId::FromRobotEmail(kTestRobotEmail));
  EXPECT_EQ(store.GetRefreshToken(), token);
}

TEST_F(DeviceOAuth2TokenStoreDesktopTest, ObserverNotifiedWhenAccountChanges) {
  local_state()->SetString(kCBCMServiceAccountEmail, kTestRobotEmail);

  std::string token = "test_token";
  std::string encrypted_token;
  auto encryptor = GetTestEncryptorForTesting();
  ASSERT_TRUE(encryptor.has_value());
  EXPECT_TRUE(encryptor->EncryptString(token, &encrypted_token));

  std::string encoded = base::Base64Encode(encrypted_token);

  local_state()->SetString(kCBCMServiceAccountRefreshToken, encoded);

  DeviceOAuth2TokenStoreDesktop store(local_state(), os_crypt_async());

  TestObserver test_observer;
  store.SetObserver(&test_observer);

  EXPECT_TRUE(store.GetRefreshToken().empty());

  store.Init(base::BindOnce([](bool, bool) {}));

  EXPECT_EQ(1, test_observer.called_count());

  EXPECT_EQ(store.GetAccountId(),
            CoreAccountId::FromRobotEmail(kTestRobotEmail));
  EXPECT_EQ(store.GetRefreshToken(), token);

  store.SetAccountEmail(kTestOtherRobotEmail);

  EXPECT_EQ(2, test_observer.called_count());
}

TEST_F(DeviceOAuth2TokenStoreDesktopTest, SaveToken) {
  std::string token = "test_token";

  DeviceOAuth2TokenStoreDesktop store(local_state(), os_crypt_async());
  store.Init(base::BindOnce([](bool, bool) {}));

  EXPECT_TRUE(store.GetRefreshToken().empty());

  bool callback_success = false;
  store.SetAndSaveRefreshToken(
      token, base::BindLambdaForTesting([&callback_success](bool success) {
        callback_success = success;
      }));

  EXPECT_TRUE(callback_success);

  std::string persisted_token =
      local_state()->GetString(kCBCMServiceAccountRefreshToken);

  std::string decoded;
  base::Base64Decode(persisted_token, &decoded);
  std::string decrypted;
  auto encryptor = GetTestEncryptorForTesting();
  ASSERT_TRUE(encryptor.has_value());
  EXPECT_TRUE(encryptor->DecryptString(decoded, &decrypted));

  EXPECT_EQ(token, store.GetRefreshToken());
  EXPECT_EQ(token, decrypted);
}

TEST_F(DeviceOAuth2TokenStoreDesktopTest, SaveTokenBeforeInit) {
  std::string token = "test_token";

  DeviceOAuth2TokenStoreDesktop store(local_state(), os_crypt_async());

  EXPECT_TRUE(store.GetRefreshToken().empty());

  bool callback_success = false;
  bool callback_called = false;
  store.SetAndSaveRefreshToken(token,
                               base::BindLambdaForTesting([&](bool success) {
                                 callback_success = success;
                                 callback_called = true;
                               }));

  // Callback should not be called yet because Init hasn't been called.
  EXPECT_FALSE(callback_called);

  store.Init(base::DoNothing());

  // Now it should be called.
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_success);

  std::string persisted_token =
      local_state()->GetString(kCBCMServiceAccountRefreshToken);

  std::string decoded;
  base::Base64Decode(persisted_token, &decoded);
  std::string decrypted;
  auto encryptor = GetTestEncryptorForTesting();
  ASSERT_TRUE(encryptor.has_value());
  EXPECT_TRUE(encryptor->DecryptString(decoded, &decrypted));

  EXPECT_EQ(token, store.GetRefreshToken());
  EXPECT_EQ(token, decrypted);
}
