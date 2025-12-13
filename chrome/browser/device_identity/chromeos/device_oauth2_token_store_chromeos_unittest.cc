// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/device_identity/chromeos/token_encryptor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::TestFuture;

// Helper class for tests to wait until the store's init procedure is completed.
class DeviceOAuth2TokenStoreInitWaiter : public TestFuture<bool, bool> {
 public:
  bool HasInitBeenCalled() const { return IsReady(); }
  bool GetInitResult() { return Get<0>(); }
  bool GetValidationRequired() { return Get<1>(); }
};

}  // namespace

class DeviceOAuth2TokenStoreChromeOSTest : public testing::Test {
 public:
  DeviceOAuth2TokenStoreChromeOSTest() = default;

  void SetUp() override {
    ash::CryptohomeMiscClient::InitializeFake();
    ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
    ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
        ash::FakeCryptohomeMiscClient::GetStubSystemSalt());

    ash::SystemSaltGetter::Initialize();

    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_(
        new ownership::MockOwnerKeyUtil());
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    ash::DeviceSettingsService::Get()->StartProcessing(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &session_manager_client_, owner_key_util_);
  }

  void TearDown() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    ash::DeviceSettingsService::Get()->StopProcessing();
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
  }

  std::string GetStubSaltAsString() {
    std::vector<uint8_t> bytes =
        ash::FakeCryptohomeMiscClient::GetStubSystemSalt();
    return std::string(bytes.begin(), bytes.end());
  }

  void SetUpDefaultValues() {
    StoreV2TokenInLocalState("device_refresh_token_4_test");
    SetRobotAccountId("service_acct@g.com");
  }

  void InitWithPendingSalt(chromeos::DeviceOAuth2TokenStoreChromeOS* store) {
    ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
        std::vector<uint8_t>());
    ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);
    store->Init(base::BindLambdaForTesting([](bool, bool) {}));
    SetUpDefaultValues();
  }

  void InitStore(chromeos::DeviceOAuth2TokenStoreChromeOS* store) {
    ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
        std::vector<uint8_t>());
    ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);

    DeviceOAuth2TokenStoreInitWaiter init_waiter;
    store->Init(init_waiter.GetCallback());

    // Make the system salt available.
    ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
        ash::FakeCryptohomeMiscClient::GetStubSystemSalt());
    ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);

    // Wait for init to complete before continuing with the test.
    EXPECT_TRUE(init_waiter.Wait());
  }

  void SetRobotAccountId(const std::string& account_id) {
    device_policy_.policy_data().set_service_account_identity(account_id);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());
    ash::DeviceSettingsService::Get()->Load();
    content::RunAllTasksUntilIdle();
  }

  base::FilePath GetTempPath() const {
    base::FilePath temp_path = base::MakeAbsoluteFilePath(temp_dir_.GetPath());
    return temp_path.Append("device_refresh_token_test");
  }

  void StoreV1TokenInLocalState(const std::string& token) {
    ash::CryptohomeTokenEncryptor encryptor(GetStubSaltAsString());
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
        prefs::kDeviceRobotAnyApiRefreshTokenV1,
        std::make_unique<base::Value>(
            encryptor.WeakEncryptWithSystemSalt(token)));
  }

  void StoreV2TokenInLocalState(const std::string& token) {
    ash::CryptohomeTokenEncryptor encryptor(GetStubSaltAsString());
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
        prefs::kDeviceRobotAnyApiRefreshTokenV2,
        std::make_unique<base::Value>(encryptor.EncryptWithSystemSalt(token)));
  }

  void StoreV3Token(const std::string& token) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chrome::FILE_CHROME_OS_DEVICE_REFRESH_TOKEN, GetTempPath(), true,
        false));
    ASSERT_TRUE(base::WriteFile(GetTempPath(), token));
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
        prefs::kDeviceRefreshTokenAnyApiIsV3Used,
        std::make_unique<base::Value>(true));
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestDeviceSettingsService scoped_device_settings_service_;
  ash::CrosSettingsHolder cros_settings_holder_{
      ash::DeviceSettingsService::Get(),
      TestingBrowserProcess::GetGlobal()->local_state()};
  ash::FakeSessionManagerClient session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, InitSuccessful) {
  ash::FakeCryptohomeMiscClient::Get()->set_system_salt(std::vector<uint8_t>());
  ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);

  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  EXPECT_TRUE(store.GetAccountId().empty());
  EXPECT_TRUE(store.GetRefreshToken().empty());

  DeviceOAuth2TokenStoreInitWaiter init_waiter;
  store.Init(init_waiter.GetCallback());

  EXPECT_FALSE(init_waiter.HasInitBeenCalled());

  // Make the system salt available.
  ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
      ash::FakeCryptohomeMiscClient::GetStubSystemSalt());
  ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  ASSERT_TRUE(init_waiter.Wait());

  EXPECT_TRUE(init_waiter.HasInitBeenCalled());
  EXPECT_TRUE(init_waiter.GetInitResult());
  EXPECT_TRUE(init_waiter.GetValidationRequired());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, LoadV1Token) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  StoreV1TokenInLocalState("test-token");
  InitStore(&store);

  EXPECT_EQ("test-token", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, LoadV2Token) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  StoreV2TokenInLocalState("test-token");
  InitStore(&store);

  EXPECT_EQ("test-token", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, LoadV3Token) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chromeos::kRefreshTokenV3Feature);
  StoreV3Token("test-token");
  InitStore(&store);

  EXPECT_EQ("test-token", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, LoadPrefersV2Token) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(chromeos::kRefreshTokenV3Feature);
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  StoreV1TokenInLocalState("test-token-v1");
  StoreV2TokenInLocalState("test-token-v2");
  StoreV3Token("test-token-v3");
  InitStore(&store);

  EXPECT_EQ("test-token-v2", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, SaveToken) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  InitStore(&store);

  store.SetAndSaveRefreshToken("test-token",
                               DeviceOAuth2TokenStore::StatusCallback());
  EXPECT_EQ("test-token", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, SaveEncryptedTokenEarly) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());

  // Set a new refresh token without the system salt available.
  InitWithPendingSalt(&store);

  store.SetAndSaveRefreshToken("test-token",
                               DeviceOAuth2TokenStore::StatusCallback());
  EXPECT_EQ("test-token", store.GetRefreshToken());

  // Make the system salt available.
  ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
      ash::FakeCryptohomeMiscClient::GetStubSystemSalt());
  ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();

  // The original token should still be present.
  EXPECT_EQ("test-token", store.GetRefreshToken());

  // Reloading shouldn't change the token either.
  chromeos::DeviceOAuth2TokenStoreChromeOS other_store(
      TestingBrowserProcess::GetGlobal()->local_state());
  InitStore(&other_store);

  EXPECT_EQ("test-token", other_store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, DoNotAnnounceTokenWithoutAccountID) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      TestingBrowserProcess::GetGlobal()->local_state());
  InitStore(&store);

  class StoreObserver : public DeviceOAuth2TokenStore::Observer {
   public:
    using Callback = base::RepeatingClosure;
    explicit StoreObserver(Callback callback)
        : callback_(std::move(callback)) {}

    void OnRefreshTokenAvailable() override { callback_.Run(); }

    Callback callback_;
  };

  auto callback_that_should_not_be_called =
      base::BindRepeating([]() { FAIL(); });
  StoreObserver observer_not_called(
      std::move(callback_that_should_not_be_called));
  store.SetObserver(&observer_not_called);

  // Make a token available during enrollment. Verify that the token is not
  // announced yet.
  store.SetAndSaveRefreshToken("test-token",
                               DeviceOAuth2TokenStore::StatusCallback());

  base::RunLoop run_loop;
  auto callback_that_should_be_called_once =
      base::BindRepeating([](base::RunLoop* loop) { loop->Quit(); }, &run_loop);
  StoreObserver observer_called_once(
      std::move(callback_that_should_be_called_once));
  store.SetObserver(&observer_called_once);

  // Also make the robot account ID available. Verify that the token is
  // announced now.
  SetRobotAccountId("robot@example.com");
  run_loop.Run();
}
