// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
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
const test::UIPath kOsInstallSuccessRestartButton = {
    "os-install", "osInstallSuccessRestartButton"};

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
const test::UIPath kOsInstallDialogIntroTitle = {
    "os-install", "osInstallDialogIntroTitleId"};
const test::UIPath kOsInstallDialogConfirmTitle = {
    "os-install", "osInstallDialogConfirmTitleId"};
const test::UIPath kOsInstallDialogProgressTitle = {
    "os-install", "osInstallDialogInProgressTitleId"};
const test::UIPath kOsInstallDialogErrorNoDestSubtitle = {
    "os-install", "osInstallDialogErrorNoDestSubtitleId"};
const test::UIPath kOsInstallDialogSuccessSubtitile = {
    "os-install", "osInstallDialogSuccessSubtitile"};

std::u16string GetDeviceOSName(bool is_branded) {
  return l10n_util::GetStringUTF16(is_branded ? IDS_CLOUD_READY_OS_NAME
                                              : IDS_CHROMIUM_OS_NAME);
}

std::string GetExpectedCountdownMessage(int time_left, bool is_branded) {
  return l10n_util::GetStringFUTF8(
      IDS_OS_INSTALL_SCREEN_SUCCESS_SUBTITLE, GetDeviceOSName(is_branded),
      l10n_util::GetPluralStringFUTF16(IDS_TIME_LONG_SECS, time_left));
}

std::string GetExpectedMessageWithBrand(int message_id, bool is_branded) {
  return l10n_util::GetStringFUTF8(message_id, GetDeviceOSName(is_branded));
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
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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

  absl::optional<OsInstallClient::Status> GetStatus() const { return status_; }

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

  absl::optional<OsInstallClient::Status> status_ = absl::nullopt;
};

// If the kAllowOsInstall switch is not set, clicking `Get Started` button
// should show the network screen.
IN_PROC_BROWSER_TEST_F(OobeBaseTest, InstallButtonHiddenByDefault) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().TapOnPath(kWelcomeGetStarted);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// If the kAllowOsInstall is set, clicking `Get Started` button show show the
// `OS Trial` screen
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, InstallButtonVisibleWithSwitch) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().TapOnPath(kWelcomeGetStarted);
  OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
}

// Check that installation starts after clicking next on the confirm step.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, StartOsInstall) {
  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();

  EXPECT_EQ(GetStatus(), absl::nullopt);

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

// Check that a successful install shows the success step and clicking
// the restart button restarts the computer.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, OsInstallSuccessRestartClicked) {
  auto* ti = OsInstallClient::Get()->GetTestInterface();

  AdvanceToOsInstallScreen();
  AdvanceThroughIntroStep();
  ConfirmInstallation();

  ti->UpdateStatus(OsInstallClient::Status::Succeeded);
  test::OobeJS().ExpectVisiblePath(kOsInstallDialogSuccess);

  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_manager_client->num_request_restart_calls(), 0);
  test::OobeJS().TapOnPath(kOsInstallSuccessRestartButton);
  EXPECT_EQ(power_manager_client->num_request_restart_calls(), 1);
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
  bool is_branded =
      LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build;
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  mocked_task_runner->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 0);
  test::OobeJS().ExpectElementText(GetExpectedCountdownMessage(40, is_branded),
                                   kOsInstallDialogSuccessSubtitile);
  mocked_task_runner->FastForwardBy(base::Seconds(41));
  EXPECT_EQ(power_manager_client->num_request_shutdown_calls(), 1);
}

// Param determines whether the build is branded or not.
class OsInstallScreenStringsTest : public OsInstallScreenTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    OsInstallScreenTest::SetUpOnMainThread();
    is_branded_ = GetParam();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        is_branded_;
  }
  bool is_branded_ = false;
};

IN_PROC_BROWSER_TEST_P(OsInstallScreenStringsTest, OsInstallStrings) {
  AdvanceToOsInstallScreen();
  test::OobeJS().ExpectElementText(
      GetExpectedMessageWithBrand(IDS_OS_INSTALL_SCREEN_INTRO_TITLE,
                                  is_branded_),
      kOsInstallDialogIntroTitle);
  test::OobeJS().ExpectElementText(
      GetExpectedMessageWithBrand(IDS_OS_INSTALL_SCREEN_CONFIRM_TITLE,
                                  is_branded_),
      kOsInstallDialogConfirmTitle);
  test::OobeJS().ExpectElementText(
      GetExpectedMessageWithBrand(IDS_OS_INSTALL_SCREEN_IN_PROGRESS_TITLE,
                                  is_branded_),
      kOsInstallDialogProgressTitle);
  test::OobeJS().ExpectElementText(
      GetExpectedMessageWithBrand(IDS_OS_INSTALL_SCREEN_ERROR_NO_DEST_SUBTITLE,
                                  is_branded_),
      kOsInstallDialogErrorNoDestSubtitle);
}

INSTANTIATE_TEST_SUITE_P(All, OsInstallScreenStringsTest, ::testing::Bool());

}  // namespace ash
