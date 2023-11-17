// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/password_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kLocalPasswordButton = {"password-selection",
                                           "localPasswordButton"};
const test::UIPath kNextButton = {"password-selection", "nextButton"};

}  // namespace

class PasswordSelectionScreenTest : public OobeBaseTest {
 public:
  PasswordSelectionScreenTest() {
    feature_list_.InitAndEnableFeature(features::kLocalPasswordForConsumers);
  }
  ~PasswordSelectionScreenTest() override = default;

  void SetUpOnMainThread() override {
    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&PasswordSelectionScreenTest::HandleScreenExit,
                            base::Unretained(this)));
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    result_ = absl::nullopt;
  }

  PasswordSelectionScreen* GetScreen() {
    return static_cast<PasswordSelectionScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            PasswordSelectionScreenView::kScreenId));
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    original_callback_.Run(result_.value());
  }

  absl::optional<PasswordSelectionScreen::Result> result_;

 private:
  void HandleScreenExit(PasswordSelectionScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  PasswordSelectionScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, GaiaPasswordChoice) {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_CHOICE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, LocalPasswordChoice) {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::LOCAL_PASSWORD_CHOICE);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->knowledge_factor_setup.local_password_forced);
}

}  // namespace ash
