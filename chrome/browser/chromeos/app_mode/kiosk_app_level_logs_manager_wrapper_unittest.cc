// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager_wrapper.h"

#include <memory>

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kInstallUrl[] = "https://install.url";

}  // namespace

class KioskAppLevelLogsManagerWrapperTest
    : public testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  KioskAppLevelLogsManagerWrapperTest() = default;

  KioskAppLevelLogsManagerWrapperTest(
      const KioskAppLevelLogsManagerWrapperTest&) = delete;
  KioskAppLevelLogsManagerWrapperTest& operator=(
      const KioskAppLevelLogsManagerWrapperTest&) = delete;

  void SetUp() override {
    SetKioskApplicationLogCollectionPolicy(GetParam());
    SetUpKioskAppId();
  }

  void SetUpKioskAppId() {
    std::string email = policy::GenerateDeviceLocalAccountUserId(
        kInstallUrl, policy::DeviceLocalAccountType::kWebKioskApp);
    AccountId account_id(AccountId::FromUserEmail(email));
    kiosk_app_id_ = ash::KioskAppId::ForWebApp(account_id);
  }

  void CreateWrapper() {
    wrapper_ = std::make_unique<KioskAppLevelLogsManagerWrapper>(&profile_,
                                                                 kiosk_app_id_);
  }

  void SetKioskApplicationLogCollectionPolicy(bool value) {
    profile().GetPrefs()->SetBoolean(
        prefs::kKioskApplicationLogCollectionEnabled, value);
  }

  bool IsPolicyEnabled() { return GetParam(); }

  KioskAppLevelLogsManagerWrapper* wrapper() { return wrapper_.get(); }

  TestingProfile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;

  ash::KioskAppId kiosk_app_id_;

  std::unique_ptr<KioskAppLevelLogsManagerWrapper> wrapper_;
};

TEST_P(KioskAppLevelLogsManagerWrapperTest,
       ShouldOnlyEnableLoggingIfEnabledByPolicy) {
  CreateWrapper();

  EXPECT_EQ(wrapper()->IsLogCollectionEnabled(), IsPolicyEnabled());
}

TEST_P(KioskAppLevelLogsManagerWrapperTest,
       ShouldUpdateLoggingStateWhenPolicyIsUpdated) {
  CreateWrapper();

  EXPECT_EQ(wrapper()->IsLogCollectionEnabled(), IsPolicyEnabled());

  bool update_policy_value = !IsPolicyEnabled();
  SetKioskApplicationLogCollectionPolicy(update_policy_value);

  EXPECT_EQ(wrapper()->IsLogCollectionEnabled(), update_policy_value);
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskAppLevelLogsManagerWrapperTest,
                         testing::Bool());

}  // namespace chromeos
