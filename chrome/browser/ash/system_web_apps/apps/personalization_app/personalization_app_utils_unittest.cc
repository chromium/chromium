// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {

namespace {

void AddAndLoginRegularUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());
  auto* user = user_manager->AddUser(account_id);
  user_manager->LoginUser(user->GetAccountId());
  user_manager->SwitchActiveUser(user->GetAccountId());
}

void AddAndLoginChildUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());
  auto* user = user_manager->AddChildUser(account_id);
  user_manager->LoginUser(user->GetAccountId());
  user_manager->SwitchActiveUser(user->GetAccountId());
}

void AddAndLoginGuestUser() {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());
  auto* user = user_manager->AddGuestUser();
  user_manager->LoginUser(user->GetAccountId());
  user_manager->SwitchActiveUser(user->GetAccountId());
}

class PersonalizationAppUtilsTest : public testing::Test {
 public:
  PersonalizationAppUtilsTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  PersonalizationAppUtilsTest(const PersonalizationAppUtilsTest&) = delete;
  PersonalizationAppUtilsTest& operator=(const PersonalizationAppUtilsTest&) =
      delete;
  ~PersonalizationAppUtilsTest() override = default;

  TestingProfileManager& profile_manager() { return profile_manager_; }

  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager().SetUp());
  }

  void TearDown() override {
    profile_manager().DeleteAllTestingProfiles();
    testing::Test::TearDown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
};

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_Guest) {
  auto* guest_profile = profile_manager().CreateGuestProfile();
  AddAndLoginGuestUser();
  ASSERT_FALSE(IsEligibleForSeaPen(guest_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_Child) {
  auto* child_profile =
      profile_manager().CreateTestingProfile("child@example.com");
  child_profile->SetIsSupervisedProfile();
  AddAndLoginChildUser(AccountId::FromUserEmail("child@example.com"));
  ASSERT_FALSE(IsEligibleForSeaPen(child_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_NoProfile) {
  ASSERT_FALSE(IsEligibleForSeaPen(nullptr));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_Googler) {
  auto* googler_profile =
      profile_manager().CreateTestingProfile("user@google.com");
  googler_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  AddAndLoginRegularUser(AccountId::FromUserEmail("user@google.com"));
  ASSERT_TRUE(IsEligibleForSeaPen(googler_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_Managed) {
  auto* managed_profile =
      profile_manager().CreateTestingProfile("managed@example.com");
  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  AddAndLoginRegularUser(AccountId::FromUserEmail("managed@example.com"));
  ASSERT_FALSE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_Regular) {
  auto* regular_profile =
      profile_manager().CreateTestingProfile("user@example.com");
  AddAndLoginRegularUser(AccountId::FromUserEmail("user@example.com"));
  ASSERT_TRUE(IsEligibleForSeaPen(regular_profile));
}

}  // namespace

}  // namespace ash::personalization_app
