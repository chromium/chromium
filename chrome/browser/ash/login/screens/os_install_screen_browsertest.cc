// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

const test::UIPath kWelcomeScreen = {"connect", "welcomeScreen"};
const test::UIPath kOsInstallButton = {"connect", "welcomeScreen", "osInstall"};

const test::UIPath kOsInstallIntroNextButton = {"os-install",
                                                "osInstallIntroNextButton"};
const test::UIPath kOsInstallConfirmNextButton = {"os-install",
                                                  "osInstallConfirmNextButton"};
const test::UIPath kOsInstallErrorShutdownButton = {
    "os-install", "osInstallErrorShutdownButton"};
const test::UIPath kOsInstallSuccessShutdownButton = {
    "os-install", "osInstallSuccessShutdownButton"};

const test::UIPath kOsInstallDialogIntro = {"os-install",
                                            "osInstallDialogIntro"};
const test::UIPath kOsInstallDialogConfirm = {"os-install",
                                              "osInstallDialogConfirm"};
const test::UIPath kOsInstallDialogInProgress = {"os-install",
                                                 "osInstallDialogInProgress"};
const test::UIPath kOsInstallDialogError = {"os-install",
                                            "osInstallDialogError"};
const test::UIPath kOsInstallDialogSuccess = {"os-install",
                                              "osInstallDialogSuccess"};

}  // namespace

class OsInstallScreenTest : public OobeBaseTest, OsInstallClient::Observer {
 public:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    OsInstallClient::Get()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();

    OsInstallClient::Get()->RemoveObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowOsInstall);
  }

  void AdvanceToOsInstallScreen() {
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
    test::OobeJS().TapOnPath(kOsInstallButton);
    OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogIntro);
  }

  void AdvanceThroughIntroStep() {
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogIntro);

    test::OobeJS().TapOnPath(kOsInstallIntroNextButton);
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogConfirm);
  }

  void ConfirmInstallation() {
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogConfirm);

    test::OobeJS().TapOnPath(kOsInstallConfirmNextButton);
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogInProgress);
  }

  absl::optional<OsInstallClient::Status> GetStatus() const { return status_; }

 private:
  // OsInstallClient::Observer override:
  void StatusChanged(OsInstallClient::Status status,
                     const std::string& service_log) override {
    status_ = status;
  }

  absl::optional<OsInstallClient::Status> status_ = absl::nullopt;
};

// If the kAllowOsInstall switch is not set, the welcome screen should
// not show the OS install button.
IN_PROC_BROWSER_TEST_F(OobeBaseTest, InstallButtonHiddenByDefault) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().ExpectHiddenPath(kOsInstallButton);
}

// If the kAllowOsInstall is set, the welcome screen should show the
// OS install button.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, InstallButtonVisibleWithSwitch) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().ExpectVisiblePath(kOsInstallButton);
}

// Check that installation starts after clicking next on the confirm step.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, StartOsInstall) {
  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();

  EXPECT_EQ(GetStatus(), absl::nullopt);

  test::OobeJS().TapOnPath(kOsInstallConfirmNextButton);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogInProgress);

  EXPECT_EQ(GetStatus(), OsInstallClient::Status::InProgress);
}

// Check that if no destination device is found, the error step is shown.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallNoDestinationDevice) {
  auto* ti = OsInstallClient::Get()->GetTestInterface();

  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  ConfirmInstallation();

  ti->UpdateStatus(OsInstallClient::Status::NoDestinationDeviceFound);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogError);
}

// Check that a generic install error shows the error step and clicking
// the shutdown button powers off.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallGenericError) {
  auto* ti = OsInstallClient::Get()->GetTestInterface();

  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  ConfirmInstallation();

  ti->UpdateStatus(OsInstallClient::Status::Failed);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogError);

  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  test::OobeJS().TapOnPath(kOsInstallErrorShutdownButton);
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 1);
}

// Check that a successful install shows the success step and clicking
// the shutdown button powers off.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallSuccess) {
  auto* ti = OsInstallClient::Get()->GetTestInterface();

  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  ConfirmInstallation();

  ti->UpdateStatus(OsInstallClient::Status::Succeeded);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogSuccess);

  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  test::OobeJS().TapOnPath(kOsInstallSuccessShutdownButton);
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 1);
}

}  // namespace ash
