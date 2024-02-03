// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"

#include "base/base64.h"
#include "base/test/bind.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/policy/core/common/policy_pref_names.h"
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
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~DeviceOAuth2TokenStoreDesktopTest() override = default;

  ScopedTestingLocalState* scoped_testing_local_state() {
    return &scoped_testing_local_state_;
  }

  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

 private:
  ScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(DeviceOAuth2TokenStoreDesktopTest, InitWithoutSavedToken) {
  DeviceOAuth2TokenStoreDesktop store(scoped_testing_local_state()->Get());

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
  scoped_testing_local_state()->Get()->SetString(kCBCMServiceAccountEmail,
                                                 kTestRobotEmail);

  std::string token = "test_token";
  std::string encrypted_token;
  OSCrypt::EncryptString(token, &encrypted_token);

  std::string encoded = base::Base64Encode(encrypted_token);

  scoped_testing_local_state()->Get()->SetString(
      kCBCMServiceAccountRefreshToken, encoded);

  DeviceOAuth2TokenStoreDesktop store(scoped_testing_local_state()->Get());

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
  scoped_testing_local_state()->Get()->SetString(kCBCMServiceAccountEmail,
                                                 kTestRobotEmail);

  std::string token = "test_token";
  std::string encrypted_token;
  OSCrypt::EncryptString(token, &encrypted_token);

  std::string encoded = base::Base64Encode(encrypted_token);

  scoped_testing_local_state()->Get()->SetString(
      kCBCMServiceAccountRefreshToken, encoded);

  DeviceOAuth2TokenStoreDesktop store(scoped_testing_local_state()->Get());

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

  DeviceOAuth2TokenStoreDesktop store(scoped_testing_local_state()->Get());
  store.Init(base::BindOnce([](bool, bool) {}));

  EXPECT_TRUE(store.GetRefreshToken().empty());

  bool callback_success = false;
  store.SetAndSaveRefreshToken(
      token, base::BindLambdaForTesting([&callback_success](bool success) {
        callback_success = success;
      }));

  EXPECT_TRUE(callback_success);

  std::string persisted_token = scoped_testing_local_state()->Get()->GetString(
      kCBCMServiceAccountRefreshToken);

  std::string decoded;
  base::Base64Decode(persisted_token, &decoded);
  std::string decrypted;
  OSCrypt::DecryptString(decoded, &decrypted);

  EXPECT_EQ(token, store.GetRefreshToken());
  EXPECT_EQ(token, decrypted);
}
