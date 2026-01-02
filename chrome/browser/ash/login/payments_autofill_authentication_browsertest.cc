// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/active_session_auth_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class PaymentsAutofillAuthBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  PaymentsAutofillAuthBrowserTest() = default;
  ~PaymentsAutofillAuthBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  void RunAuthenticationTest(
      bool expected_success,
      base::OnceCallback<void(ActiveSessionAuthControllerImpl*)>
          on_dialog_shown);

  const LoginManagerMixin::TestUserInfo user_{
      AccountId::FromUserEmailGaiaId("test-user@example.com",
                                     GaiaId("1234567890")),
      test::UserAuthConfig::Create({AshAuthFactor::kLocalPassword})
          .WithLocalPassword(test::kLocalPassword)};
  CryptohomeMixin cryptohome_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {user_},
                                   nullptr,
                                   &cryptohome_};
};

class PaymentsAutofillNoAuthFactorsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  PaymentsAutofillNoAuthFactorsBrowserTest()
      : user_(AccountId::FromUserEmailGaiaId("test-user@example.com",
                                             GaiaId("1234567890")),
              test::UserAuthConfig::Create({})),
        login_manager_(&mixin_host_, {user_}, nullptr, &cryptohome_) {}

  ~PaymentsAutofillNoAuthFactorsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  const LoginManagerMixin::TestUserInfo user_;
  CryptohomeMixin cryptohome_{&mixin_host_};
  LoginManagerMixin login_manager_;
};

void PaymentsAutofillAuthBrowserTest::RunAuthenticationTest(
    bool expected_success,
    base::OnceCallback<void(ActiveSessionAuthControllerImpl*)>
        on_dialog_shown) {
  base::RunLoop run_loop;
  InSessionAuthDialogController::Get()->ShowAuthDialog(
      InSessionAuthDialogController::Reason::kAccessAutofillPayments,
      std::make_optional<std::string>("Test prompt"),
      base::BindLambdaForTesting(
          [&](bool success, const std::string& token, base::TimeDelta timeout) {
            EXPECT_EQ(expected_success, success);
            run_loop.Quit();
          }));

  // Wait for the auth dialog to open.
  ASSERT_TRUE(base::test::RunUntil([]() {
    return Shell::Get()->active_session_auth_controller()->IsShown();
  }));

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  std::move(on_dialog_shown).Run(controller);

  run_loop.Run();
}

// Verify that calling ShowAuthDialog with kAccessAutofillPayments reason shows
// the auth dialog, and that we can successfully authenticate.
IN_PROC_BROWSER_TEST_F(PaymentsAutofillAuthBrowserTest, VerifyAuth) {
  // Log in the user.
  login_manager_.LoginWithDefaultContext(user_);
  test::WaitForPrimaryUserSessionStart();

  RunAuthenticationTest(
      /*expected_success=*/true,
      base::BindLambdaForTesting(
          [&](ActiveSessionAuthControllerImpl* controller) {
            ActiveSessionAuthControllerImpl::TestApi(controller)
                .SubmitPassword(user_.auth_config.local_password);
          }));
}

// Verify that closing the dialog results in a failure callback.
IN_PROC_BROWSER_TEST_F(PaymentsAutofillAuthBrowserTest, CloseDialog) {
  // Log in the user.
  login_manager_.LoginWithDefaultContext(user_);
  test::WaitForPrimaryUserSessionStart();

  RunAuthenticationTest(
      /*expected_success=*/false,
      base::BindLambdaForTesting(
          [](ActiveSessionAuthControllerImpl* controller) {
            ActiveSessionAuthControllerImpl::TestApi(controller).Close();
          }));
}

IN_PROC_BROWSER_TEST_F(PaymentsAutofillNoAuthFactorsBrowserTest, VerifyNoAuth) {
  // Log in the user.
  login_manager_.LoginWithDefaultContext(user_);
  test::WaitForPrimaryUserSessionStart();

  base::test::TestFuture<bool, const std::string&, base::TimeDelta> future;
  InSessionAuthDialogController::Get()->ShowAuthDialog(
      InSessionAuthDialogController::Reason::kAccessAutofillPayments,
      std::make_optional<std::string>("Test prompt"), future.GetCallback());
  EXPECT_TRUE(future.Get<0>());
}

}  // namespace ash
