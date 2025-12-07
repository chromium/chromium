// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace ash {
namespace {

// USM is a service flag for supervised accounts with optional supervision.
constexpr char kSupervisedUserServices[] = "[\"usm\"]";

// Verifies login configuration and waits for local server sign-in dialog.
void WaitForGaiaDialog(const AccountId& account_id) {
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

// Sets credentials with given `service_flags` for the test user with
// `account_id`.
void SetGaiaScreenCredentials(const AccountId& account_id,
                              const std::string& service_flags) {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                service_flags);
}

}  // namespace

// Tests updates to the user type after account supervision changes server-side.
class AccountSupervisionChangeTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<
          bool> /*Whether supervision is removed or added*/ {
 public:
  AccountSupervisionChangeTest() = default;
  AccountSupervisionChangeTest(const AccountSupervisionChangeTest&) = delete;
  AccountSupervisionChangeTest& operator=(const AccountSupervisionChangeTest&) =
      delete;

  ~AccountSupervisionChangeTest() override = default;

  // Returns true when the test removes supervision and false when the test adds
  // supervision.
  bool RemoveSupervision() const { return GetParam(); }
  LoggedInUserMixin& login_mixin() { return login_mixin_; }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  LoggedInUserMixin login_mixin_{
      &mixin_host_,
      /*test_base=*/this,
      embedded_test_server(),
      RemoveSupervision() ? LoggedInUserMixin::LogInType::kChild
                          : LoggedInUserMixin::LogInType::kConsumer,
      /*include_initial_user=*/true,
      /*account_id=*/std::nullopt,
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
};

// Tests the initial user type.
IN_PROC_BROWSER_TEST_P(AccountSupervisionChangeTest, PRE_UpdateUserType) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager);
  EXPECT_FALSE(user_manager->IsUserLoggedIn());

  login_mixin().LogInUser();

  EXPECT_TRUE(user_manager->IsUserLoggedIn());
  const user_manager::UserType initial_user_type =
      RemoveSupervision() ? user_manager::UserType::kChild
                          : user_manager::UserType::kRegular;
  EXPECT_EQ(initial_user_type,
            user_manager->GetUserType(login_mixin().GetAccountId()));
}

// Tests that the user type get updated after supervision state is changed
// sever-side.
IN_PROC_BROWSER_TEST_P(AccountSupervisionChangeTest, UpdateUserType) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager);
  EXPECT_FALSE(user_manager->IsUserLoggedIn());

  // When adding supervision, login mixin is configured for regular user and
  // additional configuration is needed for child user sign in after account
  // type change.
  if (!RemoveSupervision()) {
    login_mixin().GetFakeGaiaMixin()->SetupFakeGaiaForChildUser(
        login_mixin().GetAccountId().GetUserEmail(),
        login_mixin().GetAccountId().GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, /*issue_any_scope_token=*/true);
    login_mixin().GetUserPolicyMixin()->RequestPolicyUpdate();
  }

  const AccountId& test_account_id = login_mixin().GetAccountId();
  WaitForGaiaDialog(test_account_id);
  SetGaiaScreenCredentials(
      test_account_id, RemoveSupervision() ? FakeGaiaMixin::kEmptyUserServices
                                           : kSupervisedUserServices);

  login_mixin().GetLoginManagerMixin()->WaitForActiveSession();
  EXPECT_TRUE(user_manager->IsUserLoggedIn());

  const user_manager::UserType updated_user_type =
      RemoveSupervision() ? user_manager::UserType::kRegular
                          : user_manager::UserType::kChild;
  EXPECT_EQ(updated_user_type, user_manager->GetUserType(test_account_id));
}

INSTANTIATE_TEST_SUITE_P(All, AccountSupervisionChangeTest, testing::Bool());

}  // namespace ash
