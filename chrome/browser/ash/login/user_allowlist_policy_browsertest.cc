// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kRegularUser[] = "regular@example.com";
const char kRegularGaiaID[] = "111111";
const char kSchoolAllowlist[] = "*@edu.com";

}  // namespace

// Integration test for DeviceUserAllowlist changes.
class UserAllowlistPolicyTest : public LoginManagerTest {
 protected:
  UserAllowlistPolicyTest() = default;
  UserAllowlistPolicyTest(const UserAllowlistPolicyTest&) = delete;
  UserAllowlistPolicyTest& operator=(const UserAllowlistPolicyTest&) = delete;
  ~UserAllowlistPolicyTest() override = default;

  void SetDeviceAllowNewUsersPolicy(bool enabled) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_allow_new_users()
        ->set_allow_new_users(enabled);
  }

  void AllowUniqueUserToSignIn(const std::string& user_id) {
    auto* user_allowlist =
        policy_helper_.device_policy()->payload().mutable_user_allowlist();
    user_allowlist->clear_user_allowlist();
    user_allowlist->add_user_allowlist(user_id);
    SetDeviceAllowNewUsersPolicy(false);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kAccountsPrefUsers});
  }

  void LoginRegularUser() {
    user_policy_mixin_.RequestPolicyUpdate();
    EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_PRIMARY);

    auto context =
        ash::LoginManagerMixin::CreateDefaultUserContext(regular_user_);
    login_manager_.LoginAndWaitForActiveSession(context);
    EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
  }

 private:
  const LoginManagerMixin::TestUserInfo regular_user_{
      AccountId::FromUserEmailGaiaId(kRegularUser, kRegularGaiaID)};

  policy::DevicePolicyCrosTestHelper policy_helper_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, regular_user_.account_id};
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  LoginManagerMixin login_manager_ = {&mixin_host_,
                                      {regular_user_},
                                      &fake_gaia_};
};

// Tests that the user is signed out when DeviceUserAllowlist policy value
// changes.
IN_PROC_BROWSER_TEST_F(UserAllowlistPolicyTest, ShutdownIfNotAllowed) {
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginRegularUser();

  base::RunLoop run_loop;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(run_loop.QuitClosure());

  // Only school users are allowed. Regular user session should be terminated.
  AllowUniqueUserToSignIn(kSchoolAllowlist);
  EXPECT_TRUE(chrome::IsSendingStopRequestToSessionManager());
  run_loop.Run();
}

}  // namespace ash
