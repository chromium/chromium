// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/chromeos/child_accounts/family_features.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

const test::UIPath kParentalHandoffDialog = {"parental-handoff",
                                             "parentalHandoffDialog"};
const test::UIPath kNextButton = {"parental-handoff", "nextButton"};

SystemWebDialogDelegate* GetEduCoexistenceLoginDialog() {
  return chromeos::SystemWebDialogDelegate::FindInstance(
      SupervisedUserService::GetEduCoexistenceLoginUrl());
}

}  // namespace

class ParentalHandoffScreenBrowserTest : public OobeBaseTest {
 public:
  ParentalHandoffScreenBrowserTest();
  ParentalHandoffScreenBrowserTest(const ParentalHandoffScreenBrowserTest&) =
      delete;
  ParentalHandoffScreenBrowserTest& operator=(
      const ParentalHandoffScreenBrowserTest&) = delete;
  ~ParentalHandoffScreenBrowserTest() override = default;

  void SetUpOnMainThread() override;

 protected:
  void WaitForScreenExit();

  ParentalHandoffScreen* GetParentalHandoffScreen();

  void ExitSyncConsentScreen();

  void SkipToParentalHandoffScreen();

  const base::Optional<ParentalHandoffScreen::Result>& result() const {
    return result_;
  }

  LoginManagerMixin& login_manager_mixin() { return login_manager_mixin_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void HandleScreenExit(ParentalHandoffScreen::Result result);

  base::OnceCallback<void()> quit_closure_;

  base::Optional<ParentalHandoffScreen::Result> result_;

  ParentalHandoffScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  base::test::ScopedFeatureList feature_list_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<base::AutoReset<bool>> is_google_branded_build_;

  std::unique_ptr<base::AutoReset<bool>> assistant_is_enabled_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_, /* initial_users */ {},
                                         &fake_gaia_};
};

ParentalHandoffScreenBrowserTest::ParentalHandoffScreenBrowserTest() {
  feature_list_.InitWithFeatures(
      {supervised_users::kEduCoexistenceFlowV2, kFamilyLinkOobeHandoff},
      {} /*disable_features*/);
}

void ParentalHandoffScreenBrowserTest::SetUpOnMainThread() {
  is_google_branded_build_ =
      WizardController::ForceBrandedBuildForTesting(true);
  assistant_is_enabled_ =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(false);
  ParentalHandoffScreen* screen = GetParentalHandoffScreen();
  original_callback_ = screen->get_exit_callback_for_test();
  screen->set_exit_callback_for_test(
      base::BindRepeating(&ParentalHandoffScreenBrowserTest::HandleScreenExit,
                          base::Unretained(this)));
  OobeBaseTest::SetUpOnMainThread();
}

void ParentalHandoffScreenBrowserTest::WaitForScreenExit() {
  if (result_.has_value())
    return;
  base::RunLoop run_loop;
  quit_closure_ = base::BindOnce(run_loop.QuitClosure());
  run_loop.Run();
}

ParentalHandoffScreen*
ParentalHandoffScreenBrowserTest::GetParentalHandoffScreen() {
  return WizardController::default_controller()
      ->GetScreen<ParentalHandoffScreen>();
}

void ParentalHandoffScreenBrowserTest::ExitSyncConsentScreen() {
  test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent"})->Wait();

  const std::string button_name =
      chromeos::features::IsSplitSettingsSyncEnabled()
          ? "acceptButton"
          : "settingsSaveAndContinueButton";
  test::OobeJS().ExpectEnabledPath({"sync-consent", button_name});
  test::OobeJS().CreateFocusWaiter({"sync-consent", button_name})->Wait();
  test::OobeJS().TapOnPath({"sync-consent", button_name});

  OobeScreenExitWaiter waiter(SyncConsentScreenView::kScreenId);
  waiter.Wait();
}

void ParentalHandoffScreenBrowserTest::SkipToParentalHandoffScreen() {
  WizardController* wizard = WizardController::default_controller();

  // Wait for sync consent screen and exit from it.
  OobeScreenWaiter sync_consent_waiter(SyncConsentScreenView ::kScreenId);
  sync_consent_waiter.Wait();
  EXPECT_EQ(wizard->current_screen()->screen_id(),
            SyncConsentScreenView::kScreenId);
  ExitSyncConsentScreen();
}

void ParentalHandoffScreenBrowserTest::HandleScreenExit(
    ParentalHandoffScreen::Result result) {
  result_ = result;
  original_callback_.Run(result);
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

IN_PROC_BROWSER_TEST_F(ParentalHandoffScreenBrowserTest, RegularUserLogin) {
  login_manager_mixin().LoginAsNewRegularUser();
  SkipToParentalHandoffScreen();

  // Wait for exit from parental handoff screen.
  WaitForScreenExit();

  // Regular user login shouldn't show the EduCoexistenceLoginScreen.
  EXPECT_EQ(result().value(), ParentalHandoffScreen::Result::SKIPPED);

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Parental-handoff.Done", 0);
}

class ParentalHandoffScreenChildBrowserTest
    : public ParentalHandoffScreenBrowserTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    ParentalHandoffScreenBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void LoginAsNewChildUser() {
    WizardController::default_controller()
        ->get_wizard_context_for_testing()
        ->sign_in_as_child = true;
    login_manager_mixin().LoginAsNewChildUser();

    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    WizardControllerExitWaiter(LocaleSwitchView::kScreenId).Wait();
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(
        WizardController::default_controller()->current_screen()->screen_id(),
        EduCoexistenceLoginScreen::kScreenId);

    // Current screen is EduCoexistenceLoginScreen. Close it.
    GetEduCoexistenceLoginDialog()->Close();

    SkipToParentalHandoffScreen();

    OobeScreenWaiter parental_handoff_waiter(
        ParentalHandoffScreenView::kScreenId);
    parental_handoff_waiter.Wait();
  }

 protected:
  void ClickNextButtonOnParentalHandoffScreen() {
    test::OobeJS().ExpectVisiblePath(kParentalHandoffDialog);
    test::OobeJS().ExpectVisiblePath(kNextButton);
    test::OobeJS().TapOnPath(kNextButton);
  }

 private:
  LocalPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      &policy_server_mixin_};
};

IN_PROC_BROWSER_TEST_F(ParentalHandoffScreenChildBrowserTest, ChildUserLogin) {
  LoginAsNewChildUser();

  WizardController* wizard = WizardController::default_controller();

  EXPECT_EQ(wizard->current_screen()->screen_id(),
            ParentalHandoffScreenView::kScreenId);

  ClickNextButtonOnParentalHandoffScreen();

  WaitForScreenExit();

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Parental-handoff.Done", 1);
}

}  // namespace chromeos
