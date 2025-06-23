// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager_wrapper.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class KioskAppLevelLogsManagerWrapperTest
    : public testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  KioskAppLevelLogsManagerWrapperTest() = default;

  KioskAppLevelLogsManagerWrapperTest(
      const KioskAppLevelLogsManagerWrapperTest&) = delete;
  KioskAppLevelLogsManagerWrapperTest& operator=(
      const KioskAppLevelLogsManagerWrapperTest&) = delete;

  void SetUp() override { SetKioskApplicationLogCollectionPolicy(GetParam()); }

  void CreateWrapper() {
    wrapper_ = std::make_unique<KioskAppLevelLogsManagerWrapper>(&profile_);
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
