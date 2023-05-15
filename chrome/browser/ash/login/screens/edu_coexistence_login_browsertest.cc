// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog_onboarding.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

SystemWebDialogDelegate* GetInlineLoginDialog() {
  return SystemWebDialogDelegate::FindInstance(
      chrome::kChromeUIEDUCoexistenceLoginURLV2);
}

bool IsInlineLoginDialogShown() {
  return GetInlineLoginDialog() != nullptr;
}

}  // namespace

class EduCoexistenceLoginBrowserTest : public OobeBaseTest {
 public:
  EduCoexistenceLoginBrowserTest();
  ~EduCoexistenceLoginBrowserTest() override = default;

  EduCoexistenceLoginBrowserTest(const EduCoexistenceLoginBrowserTest&) =
      delete;
  EduCoexistenceLoginBrowserTest& operator=(
      const EduCoexistenceLoginBrowserTest&) = delete;

  void SetUpOnMainThread() override;

 protected:
  void WaitForScreenExit();

  EduCoexistenceLoginScreen* GetEduCoexistenceLoginScreen();

  const absl::optional<EduCoexistenceLoginScreen::Result>& result() {
    return result_;
  }

  LoginManagerMixin& login_manager_mixin() { return login_manager_mixin_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void HandleScreenExit(EduCoexistenceLoginScreen::Result result);

  base::OnceCallback<void()> quit_closure_;

  absl::optional<EduCoexistenceLoginScreen::Result> result_;

  EduCoexistenceLoginScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};

  base::HistogramTester histogram_tester_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
};

EduCoexistenceLoginBrowserTest::EduCoexistenceLoginBrowserTest() {}

void EduCoexistenceLoginBrowserTest::SetUpOnMainThread() {
  EduCoexistenceLoginScreen* screen = GetEduCoexistenceLoginScreen();
  original_callback_ = screen->get_exit_callback_for_test();
  screen->set_exit_callback_for_test(
      base::BindRepeating(&EduCoexistenceLoginBrowserTest::HandleScreenExit,
                          base::Unretained(this)));
  OobeBaseTest::SetUpOnMainThread();
}

void EduCoexistenceLoginBrowserTest::HandleScreenExit(
    EduCoexistenceLoginScreen::Result result) {
  result_ = result;
  original_callback_.Run(result);
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void EduCoexistenceLoginBrowserTest::WaitForScreenExit() {
  if (result_.has_value()) {
    return;
  }
  base::test::TestFuture<void> waiter;
  quit_closure_ = waiter.GetCallback();
  EXPECT_TRUE(waiter.Wait());
}

EduCoexistenceLoginScreen*
EduCoexistenceLoginBrowserTest::GetEduCoexistenceLoginScreen() {
  return EduCoexistenceLoginScreen::Get(
      WizardController::default_controller()->screen_manager());
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginBrowserTest, RegularUserLogin) {
  login_manager_mixin().LoginAsNewRegularUser();
  WaitForScreenExit();

  // Regular user login shouldn't show the EduCoexistenceLoginScreen.
  EXPECT_EQ(result().value(), EduCoexistenceLoginScreen::Result::SKIPPED);

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Edu-coexistence-login.Done", 0);
}

class EduCoexistenceLoginChildBrowserTest
    : public EduCoexistenceLoginBrowserTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    EduCoexistenceLoginBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void LoginAsNewChildUser() {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->sign_in_as_child = true;
    login_manager_mixin().LoginAsNewChildUser();

    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    WizardControllerExitWaiter(LocaleSwitchView::kScreenId).Wait();

    base::RunLoop().RunUntilIdle();
  }

 private:
  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      &policy_server_mixin_};
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginChildBrowserTest, ChildUserLogin) {
  LoginAsNewChildUser();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      EduCoexistenceLoginScreen::kScreenId);

  EduCoexistenceLoginScreen* screen = GetEduCoexistenceLoginScreen();

  // Expect that the inline login dialog is shown.
  EXPECT_TRUE(IsInlineLoginDialogShown());
  screen->Hide();
  base::RunLoop().RunUntilIdle();

  // Expect that the inline login dialog is hidden.
  EXPECT_FALSE(IsInlineLoginDialogShown());

  screen->Show(LoginDisplayHost::default_host()->GetWizardContextForTesting());

  // Expect that the inline login dialog is shown.
  EXPECT_TRUE(IsInlineLoginDialogShown());

  // Dialog got closed.
  GetInlineLoginDialog()->Close();
  WaitForScreenExit();

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Edu-coexistence-login.Done", 1);
}

}  // namespace ash
