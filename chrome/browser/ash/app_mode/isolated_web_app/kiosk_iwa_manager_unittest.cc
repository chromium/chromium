// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kTestIwaKioskAccountIdSetting[] = "iwa_kiosk_account";
constexpr char kTestWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kTestUpdateUrl[] = "https://example.com/update.json";

AccountId CreateAccountIdFromPolicy(const std::string& account_id) {
  return AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
      account_id, policy::DeviceLocalAccountType::kKioskIsolatedWebApp));
}

const AccountId kExpectedIwaKioskAccountId =
    CreateAccountIdFromPolicy(kTestIwaKioskAccountIdSetting);

// Creates an IWA device local account.
// Create a valid account with default params.
base::Value::Dict BuildIwaKioskDeviceLocalAccount(
    const std::string& account_id = kTestIwaKioskAccountIdSetting,
    const std::string& web_bundle_id = kTestWebBundleId,
    const std::string& update_url = kTestUpdateUrl) {
  return base::Value::Dict()
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, account_id)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
           static_cast<int>(
               policy::DeviceLocalAccountType::kKioskIsolatedWebApp))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
           static_cast<int>(false))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
           web_bundle_id)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
           update_url);
}

// Creates a chrome app kiosk device local account that should be ignored.
base::Value::Dict BuildChromeAppKioskDeviceLocalAccount() {
  constexpr char kAccountId[] = "chromeapp_kiosk_account";
  constexpr char kKioskAppId[] = "kiosk_app_id";
  return base::Value::Dict()
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, kAccountId)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
           static_cast<int>(policy::DeviceLocalAccountType::kKioskApp))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
           static_cast<int>(false))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId, kKioskAppId);
}

// Creates a list of device local accounts with one IWA entry.
// Create a valid account with default params.
base::Value BuildListWithOneIwa(
    const std::string& account_id = kTestIwaKioskAccountIdSetting,
    const std::string& web_bundle_id = kTestWebBundleId,
    const std::string& update_url = kTestUpdateUrl) {
  return base::Value(base::Value::List().Append(
      BuildIwaKioskDeviceLocalAccount(account_id, web_bundle_id, update_url)));
}

// Creates a list of device local accounts with one IWA entry and one chrome app
// entry.
base::Value BuildListWithVarious() {
  return base::Value(base::Value::List()
                         .Append(BuildIwaKioskDeviceLocalAccount())
                         .Append(BuildChromeAppKioskDeviceLocalAccount()));
}

// Creates a list of device local accounts with duplicate IWA entries.
base::Value BuildListWithDuplicate() {
  return base::Value(base::Value::List()
                         .Append(BuildIwaKioskDeviceLocalAccount())
                         .Append(BuildIwaKioskDeviceLocalAccount()));
}

class MockKioskAppManagerObserver : public KioskAppManagerObserver {
 public:
  MockKioskAppManagerObserver() = default;
  MockKioskAppManagerObserver(const MockKioskAppManagerObserver&) = delete;
  MockKioskAppManagerObserver& operator=(const MockKioskAppManagerObserver&) =
      delete;
  ~MockKioskAppManagerObserver() override = default;

  MOCK_METHOD(void, OnKioskAppDataRemoved, (const std::string&), (override));
};

}  // namespace

class KioskIwaManagerBaseTest : public testing::Test {
 public:
  KioskIwaManagerBaseTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    UserDataAuthClient::InitializeFake();
    iwa_manager().AddObserver(&observer());
  }

  ~KioskIwaManagerBaseTest() override {
    task_environment_.RunUntilIdle();
    iwa_manager().RemoveObserver(&observer());
  }

 protected:
  void SetDefaultTestAccount(bool with_autolaunch = false) {
    SetDeviceLocalAccounts(BuildListWithOneIwa());
    if (with_autolaunch) {
      SetKioskAutoLaunch(kTestIwaKioskAccountIdSetting);
    }
  }

  void SetDeviceLocalAccounts(const base::Value& value) {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kAccountsPrefDeviceLocalAccounts, value);
  }

  void SetKioskAutoLaunch(const std::string& account_id) {
    scoped_testing_cros_settings_.device_settings()->SetString(
        ash::kAccountsPrefDeviceLocalAccountAutoLoginId, account_id);
    scoped_testing_cros_settings_.device_settings()->SetInteger(
        ash::kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
  }

  void DisableFeatureSwitch() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kIsolatedWebAppKiosk);
  }

  void EnableFeatureSwitch() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kIsolatedWebAppKiosk);
  }

  PrefRegistrySimple* registry() { return local_state_.Get()->registry(); }

  MockKioskAppManagerObserver& observer() { return observer_; }
  KioskIwaManager& iwa_manager() { return iwa_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  MockKioskAppManagerObserver observer_;
  KioskIwaManager iwa_manager_;
};

class KioskIwaManagerFeatureOffTest : public KioskIwaManagerBaseTest {
  void SetUp() override {
    DisableFeatureSwitch();
    SetDefaultTestAccount(/*with_autolaunch=*/true);
  }
};

TEST_F(KioskIwaManagerFeatureOffTest, GetInstance) {
  KioskIwaManager* iwa_manager_ptr = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager_ptr, nullptr);
  ASSERT_EQ(iwa_manager_ptr, &iwa_manager());
}

TEST_F(KioskIwaManagerFeatureOffTest, ReturnsEmptyResults) {
  EXPECT_TRUE(iwa_manager().GetApps().empty());
  EXPECT_EQ(iwa_manager().GetApp(kExpectedIwaKioskAccountId), nullptr);
  EXPECT_EQ(iwa_manager().GetAutoLaunchAccountId(), std::nullopt);
}

class KioskIwaManagerTest : public KioskIwaManagerBaseTest {
  void SetUp() override {
    EnableFeatureSwitch();
    KioskIwaManager::RegisterPrefs(registry());
  }
};

TEST_F(KioskIwaManagerTest, GetInstance) {
  KioskIwaManager* iwa_manager_ptr = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager_ptr, nullptr);
  ASSERT_EQ(iwa_manager_ptr, &iwa_manager());
}

TEST_F(KioskIwaManagerTest, SkipsAccountWithInvalidWebBundleId) {
  constexpr char kBadWebBundleId[] = "abcdefg";
  SetDeviceLocalAccounts(BuildListWithOneIwa(kTestIwaKioskAccountIdSetting,
                                             kBadWebBundleId, kTestUpdateUrl));

  EXPECT_TRUE(iwa_manager().GetApps().empty());
  EXPECT_EQ(iwa_manager().GetApp(kExpectedIwaKioskAccountId), nullptr);
}

TEST_F(KioskIwaManagerTest, SkipsAccountWithInvalidUrl) {
  constexpr char kBadUpdateUrl[] = "http//update.com";
  SetDeviceLocalAccounts(BuildListWithOneIwa(kTestIwaKioskAccountIdSetting,
                                             kTestWebBundleId, kBadUpdateUrl));

  EXPECT_TRUE(iwa_manager().GetApps().empty());
  EXPECT_EQ(iwa_manager().GetApp(kExpectedIwaKioskAccountId), nullptr);
}

TEST_F(KioskIwaManagerTest, SkipsEmptyAccountId) {
  SetDeviceLocalAccounts(
      BuildListWithOneIwa("", kTestWebBundleId, kTestUpdateUrl));

  EXPECT_TRUE(iwa_manager().GetApps().empty());
}

TEST_F(KioskIwaManagerTest, SkipsDuplicateAccountId) {
  SetDeviceLocalAccounts(BuildListWithDuplicate());

  EXPECT_EQ(iwa_manager().GetApps().size(), 1u);
}

TEST_F(KioskIwaManagerTest, StoresOnlyIwaData) {
  SetDeviceLocalAccounts(BuildListWithVarious());

  const auto app_list = iwa_manager().GetApps();

  // KioskIwaManager stores IWA data, but skips the other one.
  ASSERT_EQ(app_list.size(), 1u);
  EXPECT_EQ(app_list.front().account_id, kExpectedIwaKioskAccountId);
}

TEST_F(KioskIwaManagerTest, GetAppFromAccountId) {
  SetDefaultTestAccount();

  const KioskIwaData* iwa_data =
      iwa_manager().GetApp(kExpectedIwaKioskAccountId);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
  EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
}

TEST_F(KioskIwaManagerTest, NoIwaAutolaunch) {
  SetDefaultTestAccount();

  // no autolaunch set at all
  EXPECT_EQ(iwa_manager().GetAutoLaunchAccountId(), std::nullopt);

  // other autolaunch account
  SetKioskAutoLaunch("other_account_id");
  EXPECT_EQ(iwa_manager().GetAutoLaunchAccountId(), std::nullopt);
}

TEST_F(KioskIwaManagerTest, WithIwaAutolaunch) {
  SetDefaultTestAccount(/*with_autolaunch=*/true);

  EXPECT_EQ(iwa_manager().GetAutoLaunchAccountId(), kExpectedIwaKioskAccountId);
}

TEST_F(KioskIwaManagerTest, RemoveApp) {
  SetDefaultTestAccount();
  EXPECT_EQ(iwa_manager().GetApps().size(), 1u);
  const KioskIwaData* iwa_data =
      iwa_manager().GetApp(kExpectedIwaKioskAccountId);
  ASSERT_NE(iwa_data, nullptr);

  const std::string kExpectedRemovedAppId = iwa_data->app_id();
  EXPECT_CALL(observer(), OnKioskAppDataRemoved(kExpectedRemovedAppId));

  // Clears device local accounts and should trigger app/account removal.
  SetDeviceLocalAccounts(base::Value());
}

TEST_F(KioskIwaManagerTest, ChangeManifestUrl) {
  SetDefaultTestAccount();

  {
    const KioskIwaData* iwa_data =
        iwa_manager().GetApp(kExpectedIwaKioskAccountId);
    ASSERT_NE(iwa_data, nullptr);
    EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
    EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
  }

  // Updating the manifest URL of existing IWA shouldn't delete the account.
  EXPECT_CALL(observer(), OnKioskAppDataRemoved(::testing::_)).Times(0);

  constexpr char kNewUpdateUrl[] = "https://example.com/replacement.json";
  SetDeviceLocalAccounts(BuildListWithOneIwa(kTestIwaKioskAccountIdSetting,
                                             kTestWebBundleId, kNewUpdateUrl));

  {
    const KioskIwaData* updated_iwa_data =
        iwa_manager().GetApp(kExpectedIwaKioskAccountId);
    ASSERT_NE(updated_iwa_data, nullptr);
    EXPECT_EQ(updated_iwa_data->web_bundle_id().id(), kTestWebBundleId);
    EXPECT_EQ(updated_iwa_data->update_manifest_url().spec(), kNewUpdateUrl);
  }
}

}  // namespace ash
