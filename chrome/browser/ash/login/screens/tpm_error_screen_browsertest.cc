// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kTpmErrorId[] = "tpm-error-message";

const test::UIPath kRestartButtonPath = {kTpmErrorId, "restartButton"};

}  // namespace

class TpmErrorScreenTest : public OobeBaseTest {
 public:
  TpmErrorScreenTest() {}

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
};

IN_PROC_BROWSER_TEST_F(TpmErrorScreenTest, EmptyOobeScreenPending) {
  ShowTpmErrorScreen();

  PrefService* prefs = g_browser_process->local_state();
  std::string pending_screen = prefs->GetString(prefs::kOobeScreenPending);
  EXPECT_EQ(pending_screen, "");
}

IN_PROC_BROWSER_TEST_F(TpmErrorScreenTest, OnRestartTpmOwnerError) {
  SetTpmOwnerError();
  ShowTpmErrorScreen();

  test::OobeJS().ExpectVisiblePath(kRestartButtonPath);
  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(kRestartButtonPath);

  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

IN_PROC_BROWSER_TEST_F(TpmErrorScreenTest, OnRestartTpmDbusError) {
  SetTpmDbusError();
  ShowTpmErrorScreen();

  test::OobeJS().ExpectVisiblePath(kRestartButtonPath);
  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(kRestartButtonPath);

  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

}  // namespace ash
