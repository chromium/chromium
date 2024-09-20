// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ProfileUserManagerControllerTest : public testing::Test {
 public:
  void SetUp() override {
    // Instantiate ProfileHelper.
    ProfileHelper::Get();

    ASSERT_TRUE(testing_profile_manager_.SetUp());
    controller_ = std::make_unique<ProfileUserManagerController>(
        testing_profile_manager_.profile_manager(), user_manager_.Get());
  }

  user_manager::FakeUserManager& user_manager() { return *user_manager_; }

  TestingProfileManager& testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      user_manager_{
          std::make_unique<user_manager::FakeUserManager>(local_state_.Get())};
  // To follow the destruction order in the production, declare controller's
  // pointer first.
  std::unique_ptr<ProfileUserManagerController> controller_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal(), &local_state_};
};

TEST_F(ProfileUserManagerControllerTest, GetProfilePrefs) {
  const AccountId kOwnerAccountId =
      AccountId::FromUserEmailGaiaId("owner@example.com", "1234567890");

  // Log in the user and create the profile.
  user_manager().AddUser(kOwnerAccountId);
  user_manager().UserLoggedIn(
      kOwnerAccountId,
      user_manager::FakeUserManager::GetFakeUsernameHash(kOwnerAccountId),
      /*browser_restart=*/false, /*is_child=*/false);
  user_manager::User* user = user_manager().GetActiveUser();
  ASSERT_FALSE(user->GetProfilePrefs());

  // Triggers ProfileUserManagerController::OnProfileAdded().
  auto* profile = testing_profile_manager().CreateTestingProfile(
      kOwnerAccountId.GetUserEmail());

  EXPECT_TRUE(user->GetProfilePrefs());
  EXPECT_EQ(profile->GetPrefs(), user->GetProfilePrefs());

  // Triggers ProfileUserManagerController::OnProfileWillBeDestroyed().
  testing_profile_manager().DeleteAllTestingProfiles();

  EXPECT_FALSE(user->GetProfilePrefs());
}

TEST_F(ProfileUserManagerControllerTest, AnnotateAccountId) {
  const AccountId kAccountId =
      AccountId::FromUserEmailGaiaId("account@example.com", "1234567890");

  // Log in the user and create the profile.
  user_manager().AddUser(kAccountId);
  user_manager().UserLoggedIn(
      kAccountId,
      user_manager::FakeUserManager::GetFakeUsernameHash(kAccountId),
      /*browser_restart=*/false, /*is_child=*/false);
  user_manager::User* user = user_manager().GetActiveUser();
  ASSERT_FALSE(user->GetProfilePrefs());

  // Trigger OnProfileCreationStarted() which annotates AccountId.
  auto* profile =
      testing_profile_manager().CreateTestingProfile(kAccountId.GetUserEmail());

  auto* account_id = ash::AnnotatedAccountId::Get(profile);
  ASSERT_TRUE(account_id);
  EXPECT_EQ(*account_id, kAccountId);
}

}  // namespace ash
