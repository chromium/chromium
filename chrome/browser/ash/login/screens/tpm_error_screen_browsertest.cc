// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kTpmErrorId[] = "tpm-error-message";

const test::UIPath kSkipButtonPath = {kTpmErrorId, "skipButton"};
const test::UIPath kRestartButtonPath = {kTpmErrorId, "restartButton"};

}  // namespace

class TpmErrorScreenTest : public OobeBaseTest {
 public:
  TpmErrorScreenTest() {}

  void SetUpOnMainThread() override {
    TpmErrorScreen* tpm_error_screen =
        WizardController::default_controller()->GetScreen<TpmErrorScreen>();

    original_callback_ = tpm_error_screen->get_exit_callback_for_testing();
    tpm_error_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowTpmErrorScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        TpmErrorView::kScreenId);
  }

  void SetTpmOwnerError() {
    LoginDisplayHost::default_host()->GetWizardContext()->tpm_owned_error =
        true;
  }

  void SetTpmDbusError() {
    LoginDisplayHost::default_host()->GetWizardContext()->tpm_dbus_error = true;
  }

  TpmErrorScreen::Result WaitForScreenExitResult() {
    TpmErrorScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 private:
  base::test::TestFuture<TpmErrorScreen::Result> screen_result_waiter_;
  TpmErrorScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(TpmErrorScreenTest, NoSkipOptionOnTpmDbusError) {
  SetTpmDbusError();
  ShowTpmErrorScreen();

  test::OobeJS().ExpectVisiblePath(kRestartButtonPath);
  test::OobeJS().ExpectHiddenPath(kSkipButtonPath);

  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(kRestartButtonPath);

  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

IN_PROC_BROWSER_TEST_F(TpmErrorScreenTest, SkipButtonOnTpmOwnedError) {
  SetTpmOwnerError();
  ShowTpmErrorScreen();

  test::OobeJS().ExpectVisiblePath(kRestartButtonPath);
  test::OobeJS().ClickOnPath(kSkipButtonPath);

  TpmErrorScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, TpmErrorScreen::Result::kSkip);

  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}

}  // namespace ash
