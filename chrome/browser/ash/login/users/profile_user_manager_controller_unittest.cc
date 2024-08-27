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
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ProfileUserManagerController, GetProfilePrefs) {
  // Instantiate ProfileHelper.
  ProfileHelper::Get();

  content::BrowserTaskEnvironment task_environment;
  ScopedCrosSettingsTestHelper settings_helper;
  const AccountId kOwnerAccountId =
      AccountId::FromUserEmailGaiaId("owner@example.com", "1234567890");

  // Log in the user and create the profile.
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      user_manager{
          std::make_unique<user_manager::FakeUserManager>(local_state.Get())};
  // To follow the destruction order in the production, declare controller's
  // pointer first.
  std::unique_ptr<ProfileUserManagerController> controller;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal(),
                                        &local_state);
  ASSERT_TRUE(profile_manager.SetUp());
  controller = std::make_unique<ProfileUserManagerController>(
      profile_manager.profile_manager(), user_manager.Get());

  user_manager->AddUser(kOwnerAccountId);
  user_manager->UserLoggedIn(
      kOwnerAccountId,
      user_manager::FakeUserManager::GetFakeUsernameHash(kOwnerAccountId),
      /*browser_restart=*/false, /*is_child=*/false);
  user_manager::User* user = user_manager->GetActiveUser();
  ASSERT_FALSE(user->GetProfilePrefs());

  // Triggers ProfileUserManagerController::OnProfileAdded().
  auto* profile =
      profile_manager.CreateTestingProfile(kOwnerAccountId.GetUserEmail());

  EXPECT_TRUE(user->GetProfilePrefs());
  EXPECT_EQ(profile->GetPrefs(), user->GetProfilePrefs());

  // Rgiggers ProfileUserManagerController::OnProfileWillBeDestroyed().
  profile_manager.DeleteAllTestingProfiles();

  EXPECT_FALSE(user->GetProfilePrefs());
}

}  // namespace ash
