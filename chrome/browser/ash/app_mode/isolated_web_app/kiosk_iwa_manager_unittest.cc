// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kIwaKioskAccountIdSetting[] = "iwa_kiosk_account";
constexpr char kTestWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kTestUpdateUrl[] = "https://example.com/update.json";

AccountId CreateAccountIdFromPolicy(const std::string& account_id) {
  return AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
      account_id, policy::DeviceLocalAccountType::kKioskIsolatedWebApp));
}

const AccountId kExpectedIwaKioskAccountId =
    CreateAccountIdFromPolicy(kIwaKioskAccountIdSetting);

// Creates an IWA device local account that should be stored.
base::Value::Dict BuildIwaKioskDeviceLocalAccount() {
  return base::Value::Dict()
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId,
           kIwaKioskAccountIdSetting)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
           static_cast<int>(
               policy::DeviceLocalAccountType::kKioskIsolatedWebApp))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
           static_cast<int>(false))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
           kTestWebBundleId)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
           kTestUpdateUrl);
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

base::Value BuildTestDeviceLocalAccounts() {
  return base::Value(base::Value::List()
                         .Append(BuildIwaKioskDeviceLocalAccount())
                         .Append(BuildChromeAppKioskDeviceLocalAccount()));
}
}  // namespace

class KioskIwaManagerTest : public testing::Test {
 public:
  KioskIwaManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    SetDeviceLocalAccounts(BuildTestDeviceLocalAccounts());
  }

 protected:
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

 private:
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ScopedTestingLocalState local_state_;

  KioskIwaManager iwa_manager_;
};

class KioskIwaManagerFeatureOnTest : public KioskIwaManagerTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kIsolatedWebAppKiosk};
};

TEST_F(KioskIwaManagerFeatureOnTest, StoresOnlyIwaData) {
  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);

  const auto app_list = iwa_manager->GetApps();

  // KioskIwaManager stores IWA data, but skips the other one.
  ASSERT_EQ(app_list.size(), 1u);
  EXPECT_EQ(app_list.front().account_id, kExpectedIwaKioskAccountId);
}

TEST_F(KioskIwaManagerFeatureOnTest, GetAppFromAccountId) {
  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);

  const KioskIwaData* iwa_data =
      iwa_manager->GetApp(kExpectedIwaKioskAccountId);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
  EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
}

TEST_F(KioskIwaManagerFeatureOnTest, NoAutolaunch) {
  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);
  EXPECT_EQ(iwa_manager->GetAutoLaunchAccountId(), std::nullopt);
}

TEST_F(KioskIwaManagerFeatureOnTest, NonIwaAutolaunch) {
  SetKioskAutoLaunch("other_account_id");
  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);
  EXPECT_EQ(iwa_manager->GetAutoLaunchAccountId(), std::nullopt);
}

TEST_F(KioskIwaManagerFeatureOnTest, WithIwaAutolaunch) {
  SetKioskAutoLaunch(kIwaKioskAccountIdSetting);

  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);
  EXPECT_EQ(iwa_manager->GetAutoLaunchAccountId(), kExpectedIwaKioskAccountId);
}

class KioskIwaManagerFeatureOffTest : public KioskIwaManagerTest {
 public:
  KioskIwaManagerFeatureOffTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kIsolatedWebAppKiosk);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(KioskIwaManagerFeatureOffTest, ReturnsEmptyResults) {
  SetKioskAutoLaunch(kIwaKioskAccountIdSetting);

  KioskIwaManager* iwa_manager = KioskIwaManager::Get();
  ASSERT_NE(iwa_manager, nullptr);

  EXPECT_TRUE(iwa_manager->GetApps().empty());
  EXPECT_EQ(iwa_manager->GetApp(kExpectedIwaKioskAccountId), nullptr);
  EXPECT_EQ(iwa_manager->GetAutoLaunchAccountId(), std::nullopt);
}

}  // namespace ash
