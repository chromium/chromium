// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kErrorId[] = "install-attributes-error-message";

const test::UIPath kRestartButtonPath = {kErrorId, "restartButton"};
const test::UIPath kResetButtonPath = {kErrorId, "resetButton"};

}  // namespace

class InstallAttributesErrorScreenTest : public OobeBaseTest {
 public:
  InstallAttributesErrorScreenTest() {}

  void ShowInstallAttributesErrorScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        InstallAttributesErrorView::kScreenId);
  }
};

IN_PROC_BROWSER_TEST_F(InstallAttributesErrorScreenTest,
                       InstallAttributesScreenErrorOnRestart) {
  ShowInstallAttributesErrorScreen();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kFirstExecAfterBoot);

  test::OobeJS().ExpectVisiblePath(kRestartButtonPath);
  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(kRestartButtonPath);

  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

IN_PROC_BROWSER_TEST_F(InstallAttributesErrorScreenTest,
                       InstallAttributesScreenErrorOnReset) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFirstExecAfterBoot);
  ShowInstallAttributesErrorScreen();

  test::OobeJS().ExpectVisiblePath(kResetButtonPath);
  test::OobeJS().TapOnPath({"install-attributes-error-message", "resetButton"});
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
}

}  // namespace ash
