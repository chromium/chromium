// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/roaming_configuration_migration_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/networking/fake_network_roaming_state_migration_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kEmail1[] = "test-user-1@example.com";
constexpr char kEmail2[] = "test-user-2@example.com";

class RoamingConfigurationMigrationHandlerTest : public testing::Test {
 protected:
  RoamingConfigurationMigrationHandlerTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kCellularAllowPerNetworkRoaming);
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void SetUp() override {
    ASSERT_TRUE(mock_profile_manager_.SetUp());
    roaming_configuration_migration_handler_ =
        std::make_unique<RoamingConfigurationMigrationHandler>(
            &fake_network_roaming_state_migration_handler_);
    scoped_cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override {
    scoped_cros_settings_test_helper_.RestoreRealDeviceSettingsProvider();
  }

  void CreateTestingProfile(const std::string& email) {
    const AccountId test_account_id(AccountId::FromUserEmail(email));
    fake_user_manager_->AddUser(test_account_id);
    fake_user_manager_->LoginUser(test_account_id);

    TestingProfile* mock_profile = mock_profile_manager_.CreateTestingProfile(
        test_account_id.GetUserEmail(),
        {{ash::OwnerSettingsServiceAshFactory::GetInstance(),
          base::BindRepeating(&RoamingConfigurationMigrationHandlerTest::
                                  CreateOwnerSettingsServiceAsh,
                              base::Unretained(this))}});
    owner_settings_service_ash_ =
        ash::OwnerSettingsServiceAshFactory::GetInstance()
            ->GetForBrowserContext(mock_profile);
  }

  void FlushActiveProfileCallbacks(bool is_owner) {
    DCHECK(owner_settings_service_ash_);
    owner_settings_service_ash_->RunPendingIsOwnerCallbacksForTesting(is_owner);
  }

  void SetDataRoamingEnabled(bool data_roaming_enabled) {
    scoped_cros_settings_test_helper_.SetBoolean(
        chromeos::kSignedDataRoamingEnabled, data_roaming_enabled);
  }

  bool GetDataRoamingEnabled() {
    bool data_roaming_enabled;
    EXPECT_TRUE(chromeos::CrosSettings::Get()->GetBoolean(
        chromeos::kSignedDataRoamingEnabled, &data_roaming_enabled));
    return data_roaming_enabled;
  }

  FakeNetworkRoamingStateMigrationHandler* fake_migration_handler() {
    return &fake_network_roaming_state_migration_handler_;
  }

 private:
  std::unique_ptr<KeyedService> CreateOwnerSettingsServiceAsh(
      content::BrowserContext* context) {
    return scoped_cros_settings_test_helper_.CreateOwnerSettingsService(
        Profile::FromBrowserContext(context));
  }

  content::BrowserTaskEnvironment task_environment_;
  FakeNetworkRoamingStateMigrationHandler
      fake_network_roaming_state_migration_handler_;
  TestingProfileManager mock_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  ash::FakeChromeUserManager* fake_user_manager_;
  ash::OwnerSettingsServiceAsh* owner_settings_service_ash_;
  ash::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<RoamingConfigurationMigrationHandler>
      roaming_configuration_migration_handler_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(RoamingConfigurationMigrationHandlerTest,
       UpdatesSettingsWhenOwnerProfileIsAdded) {
  SetDataRoamingEnabled(false);

  // First add a non-owner and make sure it has no effect.
  CreateTestingProfile(kEmail1);
  FlushActiveProfileCallbacks(/*is_owner=*/false);
  EXPECT_FALSE(GetDataRoamingEnabled());

  CreateTestingProfile(kEmail2);
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  EXPECT_TRUE(GetDataRoamingEnabled());
}

TEST_F(RoamingConfigurationMigrationHandlerTest,
       UpdatesSettingsWhenActiveProfileIsOwner) {
  SetDataRoamingEnabled(true);
  CreateTestingProfile(kEmail1);

  // Flush the callbacks from adding a profile to specifically test the
  // callbacks for when a new network is found.
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  fake_migration_handler()->NotifyFoundCellularNetwork(false);
  EXPECT_TRUE(GetDataRoamingEnabled());
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  EXPECT_FALSE(GetDataRoamingEnabled());
}

TEST_F(RoamingConfigurationMigrationHandlerTest,
       UpdatesSettingsWithMixedRoaming) {
  SetDataRoamingEnabled(false);

  CreateTestingProfile(kEmail1);
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  EXPECT_TRUE(GetDataRoamingEnabled());

  fake_migration_handler()->NotifyFoundCellularNetwork(false);
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  EXPECT_FALSE(GetDataRoamingEnabled());

  fake_migration_handler()->NotifyFoundCellularNetwork(true);
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  EXPECT_FALSE(GetDataRoamingEnabled());
}

}  // namespace
}  // namespace policy
