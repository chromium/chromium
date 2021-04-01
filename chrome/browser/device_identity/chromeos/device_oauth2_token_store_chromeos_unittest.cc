// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Helper class for tests to wait until the store's init procedure is completed.
class DeviceOAuth2TokenStoreInitWaiter {
 public:
  DeviceOAuth2TokenStoreInitWaiter() = default;
  // The caller must ensure that the DeviceOAuth2TokenStoreInitWaiter outlives
  // the callback it returns.
  DeviceOAuth2TokenStore::InitCallback GetCallback() {
    return base::BindOnce(&DeviceOAuth2TokenStoreInitWaiter::OnInit,
                          base::Unretained(this));
  }
  void Wait() { run_loop_.Run(); }
  bool HasInitBeenCalled() { return init_called_; }
  bool GetInitResult() {
    CHECK(init_called_);
    return init_result_;
  }
  bool GetValidationRequired() {
    CHECK(init_called_);
    return validation_required_;
  }

 private:
  void OnInit(bool init_result, bool validation_required) {
    ASSERT_FALSE(init_called_);
    init_called_ = true;
    init_result_ = init_result;
    validation_required_ = validation_required;
    run_loop_.Quit();
  }
  base::RunLoop run_loop_;
  bool init_called_ = false;
  bool init_result_ = false;
  bool validation_required_ = false;
};
}  // namespace

class DeviceOAuth2TokenStoreChromeOSTest : public testing::Test {
 public:
  DeviceOAuth2TokenStoreChromeOSTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    chromeos::CryptohomeMiscClient::InitializeFake();
    chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
    chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
        chromeos::FakeCryptohomeMiscClient::GetStubSystemSalt());

    chromeos::SystemSaltGetter::Initialize();

    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_(
        new ownership::MockOwnerKeyUtil());
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    ash::DeviceSettingsService::Get()->SetSessionManager(
        &session_manager_client_, owner_key_util_);
  }

  void TearDown() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    ash::DeviceSettingsService::Get()->UnsetSessionManager();
    chromeos::SystemSaltGetter::Shutdown();
    chromeos::CryptohomeMiscClient::Shutdown();
  }

  void SetUpDefaultValues() {
    SetDeviceRefreshTokenInLocalState("device_refresh_token_4_test");
    SetRobotAccountId("service_acct@g.com");
  }

  void InitWithPendingSalt(chromeos::DeviceOAuth2TokenStoreChromeOS* store) {
    chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
        std::vector<uint8_t>());
    chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);
    store->Init(base::BindLambdaForTesting([](bool, bool) {}));
    SetUpDefaultValues();
  }

  void InitStore(chromeos::DeviceOAuth2TokenStoreChromeOS* store) {
    chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
        std::vector<uint8_t>());
    chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);

    DeviceOAuth2TokenStoreInitWaiter init_waiter;
    store->Init(init_waiter.GetCallback());

    // Make the system salt available.
    chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
        chromeos::FakeCryptohomeMiscClient::GetStubSystemSalt());
    chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);

    // Wait for init to complete before continuing with the test.
    init_waiter.Wait();
  }

  void SetRobotAccountId(const std::string& account_id) {
    device_policy_.policy_data().set_service_account_identity(account_id);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());
    ash::DeviceSettingsService::Get()->Load();
    content::RunAllTasksUntilIdle();
  }

  void SetDeviceRefreshTokenInLocalState(const std::string& refresh_token) {
    scoped_testing_local_state_.Get()->SetUserPref(
        prefs::kDeviceRobotAnyApiRefreshToken,
        std::make_unique<base::Value>(refresh_token));
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestDeviceSettingsService scoped_device_settings_service_;
  ash::ScopedTestCrosSettings scoped_test_cros_settings_{
      scoped_testing_local_state_.Get()};
  chromeos::FakeSessionManagerClient session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;
};

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, InitSuccessful) {
  chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
      std::vector<uint8_t>());
  chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);

  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      scoped_testing_local_state_.Get());

  EXPECT_TRUE(store.GetAccountId().empty());
  EXPECT_TRUE(store.GetRefreshToken().empty());

  DeviceOAuth2TokenStoreInitWaiter init_waiter;
  store.Init(init_waiter.GetCallback());

  EXPECT_FALSE(init_waiter.HasInitBeenCalled());

  // Make the system salt available.
  chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
      chromeos::FakeCryptohomeMiscClient::GetStubSystemSalt());
  chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  init_waiter.Wait();

  EXPECT_TRUE(init_waiter.HasInitBeenCalled());
  EXPECT_TRUE(init_waiter.GetInitResult());
  EXPECT_TRUE(init_waiter.GetValidationRequired());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, SaveToken) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      scoped_testing_local_state_.Get());

  InitStore(&store);

  store.SetAndSaveRefreshToken("test-token",
                               DeviceOAuth2TokenStore::StatusCallback());
  EXPECT_EQ("test-token", store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, SaveEncryptedTokenEarly) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      scoped_testing_local_state_.Get());

  // Set a new refresh token without the system salt available.
  InitWithPendingSalt(&store);

  store.SetAndSaveRefreshToken("test-token",
                               DeviceOAuth2TokenStore::StatusCallback());
  EXPECT_EQ("test-token", store.GetRefreshToken());

  // Make the system salt available.
  chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
      chromeos::FakeCryptohomeMiscClient::GetStubSystemSalt());
  chromeos::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();

  // The original token should still be present.
  EXPECT_EQ("test-token", store.GetRefreshToken());

  // Reloading shouldn't change the token either.
  chromeos::DeviceOAuth2TokenStoreChromeOS other_store(
      scoped_testing_local_state_.Get());
  InitStore(&other_store);

  EXPECT_EQ("test-token", other_store.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenStoreChromeOSTest, DoNotAnnounceTokenWithoutAccountID) {
  chromeos::DeviceOAuth2TokenStoreChromeOS store(
      scoped_testing_local_state_.Get());
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
