// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kUserEmail1[] = "user1@gmail.com";
const char kUserEmail2[] = "user2@gmail.com";
const char kUserEmail3[] = "user3@gmail.com";
const char kUserEmail4[] = "user4@gmail.com";

const GaiaId::Literal kGaiaId1("111111");
const GaiaId::Literal kGaiaId2("222222");
const GaiaId::Literal kGaiaId3("333333");
const GaiaId::Literal kGaiaId4("444444");

}  // namespace

// Integration browser test for DeviceMaxUserProfiles policy.
class DeviceMaxUserProfilesBrowserTest : public LoginManagerTest {
 protected:
  DeviceMaxUserProfilesBrowserTest() = default;
  DeviceMaxUserProfilesBrowserTest(const DeviceMaxUserProfilesBrowserTest&) =
      delete;
  DeviceMaxUserProfilesBrowserTest& operator=(
      const DeviceMaxUserProfilesBrowserTest&) = delete;
  ~DeviceMaxUserProfilesBrowserTest() override = default;

  void SetDeviceMaxUserProfilesPolicy(int max_users) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_devicemaxuserprofiles()
        ->set_value(max_users);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kAccountsPrefDeviceMaxUserProfiles});
  }

  void LoginUser(const LoginManagerMixin::TestUserInfo& user_info) {
    login_manager_.SkipPostLoginScreens();
    auto context = LoginManagerMixin::CreateDefaultUserContext(user_info);
    login_manager_.LoginAndWaitForActiveSession(context);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
  }

  const LoginManagerMixin::TestUserInfo user1_{
      AccountId::FromUserEmailGaiaId(kUserEmail1, kGaiaId1)};
  const LoginManagerMixin::TestUserInfo user2_{
      AccountId::FromUserEmailGaiaId(kUserEmail2, kGaiaId2)};
  const LoginManagerMixin::TestUserInfo user3_{
      AccountId::FromUserEmailGaiaId(kUserEmail3, kGaiaId3)};
  const LoginManagerMixin::TestUserInfo user4_{
      AccountId::FromUserEmailGaiaId(kUserEmail4, kGaiaId4)};

 private:
  policy::DevicePolicyCrosTestHelper policy_helper_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  LoginManagerMixin login_manager_ = {&mixin_host_,
                                      {user1_, user2_, user3_, user4_},
                                      &fake_gaia_};
};

// Tests that applying DeviceMaxUserProfiles on login screen trims excess LRU
// user profiles.
IN_PROC_BROWSER_TEST_F(DeviceMaxUserProfilesBrowserTest,
                       TrimExcessUsersOnLoginScreen) {
  auto* user_manager = user_manager::UserManager::Get();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(user_manager->GetPersistedUsers().size(), 4u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 4);

  // Set policy to cap stored profiles at 2.
  SetDeviceMaxUserProfilesPolicy(2);

  // Verifies that the user count is reduced to 2 both in UserManager and on
  // Login screen, and that evicted LRU profiles are removed from KnownUser.
  ASSERT_EQ(user_manager->GetPersistedUsers().size(), 2u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 2);

  const auto& persisted_users = user_manager->GetPersistedUsers();
  EXPECT_EQ(persisted_users[0]->GetAccountId(), user1_.account_id);
  EXPECT_EQ(persisted_users[1]->GetAccountId(), user2_.account_id);
  EXPECT_TRUE(user_manager->IsKnownUser(user1_.account_id));
  EXPECT_TRUE(user_manager->IsKnownUser(user2_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user3_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user4_.account_id));

  // Lowering the policy cap further to 1 trims user2_ as well.
  SetDeviceMaxUserProfilesPolicy(1);

  ASSERT_EQ(user_manager->GetPersistedUsers().size(), 1u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 1);
  EXPECT_EQ(user_manager->GetPersistedUsers()[0]->GetAccountId(),
            user1_.account_id);
  EXPECT_TRUE(user_manager->IsKnownUser(user1_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user2_.account_id));
}

// Tests that setting DeviceMaxUserProfiles to 0 keeps unlimited profiles.
IN_PROC_BROWSER_TEST_F(DeviceMaxUserProfilesBrowserTest,
                       ZeroValueAllowsUnlimitedUsers) {
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(user_manager::UserManager::Get()->GetPersistedUsers().size(), 4u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 4);

  // Setting to 0 should not remove any profiles.
  SetDeviceMaxUserProfilesPolicy(0);

  EXPECT_EQ(user_manager::UserManager::Get()->GetPersistedUsers().size(), 4u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 4);
}

// Tests that no users are trimmed when the policy is updated during an active
// session, and that the newly logged-in user becomes MRU and is preserved when
// trimming runs on the login screen.
IN_PROC_BROWSER_TEST_F(DeviceMaxUserProfilesBrowserTest,
                       PRE_ActiveUserSessionIsNotTrimmedWhenPolicyChanges) {
  auto* user_manager = user_manager::UserManager::Get();
  ASSERT_EQ(user_manager->GetPersistedUsers().size(), 4u);

  // Log in as user4_ (previously the oldest/LRU user).
  LoginUser(user4_);

  // Set policy to 1 while in-session.
  SetDeviceMaxUserProfilesPolicy(1);

  // No users are trimmed while a user session is active.
  const auto* active_user = user_manager->GetActiveUser();
  ASSERT_TRUE(active_user);
  EXPECT_EQ(active_user->GetAccountId(), user4_.account_id);
  EXPECT_EQ(user_manager->GetPersistedUsers().size(), 4u);
  EXPECT_TRUE(user_manager->IsKnownUser(user1_.account_id));
  EXPECT_TRUE(user_manager->IsKnownUser(user2_.account_id));
  EXPECT_TRUE(user_manager->IsKnownUser(user3_.account_id));
  EXPECT_TRUE(user_manager->IsKnownUser(user4_.account_id));
}

// Verifies that on the next login screen after the session in PRE_, all users
// survived the in-session policy update and trimming now preserves the new MRU
// user (user4_).
IN_PROC_BROWSER_TEST_F(DeviceMaxUserProfilesBrowserTest,
                       ActiveUserSessionIsNotTrimmedWhenPolicyChanges) {
  auto* user_manager = user_manager::UserManager::Get();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  // All 4 users survived the in-session policy update in PRE_, with user4_
  // now at the front (MRU) of the persisted user list.
  ASSERT_EQ(user_manager->GetPersistedUsers().size(), 4u);
  EXPECT_EQ(user_manager->GetPersistedUsers()[0]->GetAccountId(),
            user4_.account_id);

  // Applying the limit of 1 on the login screen now trims user1_, user2_, and
  // user3_, preserving user4_ as the MRU profile.
  SetDeviceMaxUserProfilesPolicy(1);

  ASSERT_EQ(user_manager->GetPersistedUsers().size(), 1u);
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 1);
  EXPECT_EQ(user_manager->GetPersistedUsers()[0]->GetAccountId(),
            user4_.account_id);
  EXPECT_TRUE(user_manager->IsKnownUser(user4_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user1_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user2_.account_id));
  EXPECT_FALSE(user_manager->IsKnownUser(user3_.account_id));
}

}  // namespace ash
