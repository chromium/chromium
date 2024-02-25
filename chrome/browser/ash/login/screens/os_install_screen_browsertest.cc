// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {
namespace {

const test::UIPath kWelcomeScreen = {"connect", "welcomeScreen"};
const test::UIPath kWelcomeGetStarted = {"connect", "welcomeScreen",
                                         "getStarted"};

const test::UIPath kOsTrialInstallRadioButton = {"os-trial", "installButton"};
const test::UIPath kOsTrialNextButton = {"os-trial", "nextButton"};

const test::UIPath kOsInstallExitButton = {"os-install", "osInstallExitButton"};
const test::UIPath kOsInstallIntroNextButton = {"os-install",
                                                "osInstallIntroNextButton"};
const test::UIPath kOsInstallConfirmNextButton = {"os-install",
                                                  "osInstallConfirmNextButton"};
const test::UIPath kOsInstallConfirmCloseButton = {"os-install",
                                                   "closeConfirmDialogButton"};
const test::UIPath kOsInstallErrorShutdownButton = {
    "os-install", "osInstallErrorShutdownButton"};

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

// Paths to test strings
const test::UIPath kOsInstallDialogSuccessSubtitile = {
    "os-install", "osInstallDialogSuccessSubtitile"};

std::string GetExpectedCountdownMessage(base::TimeDelta time_left) {
  return l10n_util::GetStringFUTF8(
      IDS_OS_INSTALL_SCREEN_SUCCESS_SUBTITLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, time_left),
      l10n_util::GetStringUTF16(IDS_INSTALLED_PRODUCT_OS_NAME));
}

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
    test::WaitForWelcomeScreen();
    test::OobeJS().TapOnPath(kWelcomeGetStarted);

    OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
    test::OobeJS().ExpectHasAttribute("checked", kOsTrialInstallRadioButton);
    test::OobeJS().ClickOnPath(kOsTrialNextButton);

    OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogIntro);
  }

  void AdvanceThroughIntroStep() {
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogIntro);

    test::OobeJS().TapOnPath(kOsInstallIntroNextButton);
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kOsInstallDialogConfirm) +
                      ".open")
        ->Wait();
    test::OobeJS().ExpectFocused(kOsInstallConfirmCloseButton);
  }

  void ConfirmInstallation() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kOsInstallDialogConfirm) +
                      ".open")
        ->Wait();
    test::OobeJS().TapOnPath(kOsInstallConfirmNextButton);

    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kOsInstallDialogConfirm) +
                      ".open === false")
        ->Wait();
    test::OobeJS().ExpectVisiblePath(kOsInstallDialogInProgress);
  }

  std::optional<OsInstallClient::Status> GetStatus() const { return status_; }

  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    WizardController::default_controller()
        ->GetScreen<OsInstallScreen>()
        ->set_tick_clock_for_testing(tick_clock);
  }

 private:
  // OsInstallClient::Observer override:
  void StatusChanged(OsInstallClient::Status status,
                     const std::string& service_log) override {
    status_ = status;
  }

  std::optional<OsInstallClient::Status> status_ = std::nullopt;
};

// If the kAllowOsInstall switch is not set, clicking `Get Started` button
// should show the network screen.
IN_PROC_BROWSER_TEST_F(OobeBaseTest, InstallButtonHiddenByDefault) {
  test::WaitForWelcomeScreen();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().TapOnPath(kWelcomeGetStarted);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// If the kAllowOsInstall is set, clicking `Get Started` button show show the
// `OS Trial` screen
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, InstallButtonVisibleWithSwitch) {
  test::WaitForWelcomeScreen();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().TapOnPath(kWelcomeGetStarted);
  OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
}

// Check that installation starts after clicking next on the confirm step.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, StartOsInstall) {
  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();

  EXPECT_EQ(GetStatus(), std::nullopt);

  ConfirmInstallation();

  EXPECT_EQ(GetStatus(), OsInstallClient::Status::InProgress);
}

// Check close button for ConfirmDialog and back button for IntroDialog.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallBackNavigation) {
  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  // Close confirm dialog
  test::OobeJS().TapOnPath(kOsInstallConfirmCloseButton);
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kOsInstallDialogConfirm) +
                    ".open === false")
      ->Wait();
  test::OobeJS().ExpectFocused(kOsInstallIntroNextButton);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogIntro);
  // Exit os install flow
  test::OobeJS().TapOnPath(kOsInstallExitButton);
  OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
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

// Check that a successful install shows the success step and countdown timer,
// which will shut down the computer automatically after 60 seconds.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallSuccessAutoShutdown) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockForTesting(mocked_task_runner->GetMockTickClock());
  auto* ti = OsInstallClient::Get()->GetTestInterface();

  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  ConfirmInstallation();

  ti->UpdateStatus(OsInstallClient::Status::Succeeded);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogSuccess);

  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  mocked_task_runner->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  test::OobeJS().ExpectElementText(
      GetExpectedCountdownMessage(base::Seconds(40)),
      kOsInstallDialogSuccessSubtitile);
  mocked_task_runner->FastForwardBy(base::Seconds(41));
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 1);
}

}  // namespace ash
