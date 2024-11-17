// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {
namespace {

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "test-user1@gmail.com";

// HTML Elements
constexpr char kResetScreen[] = "reset";
constexpr char kConfirmationDialog[] = "confirmationDialog";
constexpr char kTpmUpdate[] = "tpmFirmwareUpdate";
constexpr char kTpmUpdateCheckbox[] = "tpmFirmwareUpdateCheckbox";

constexpr char kCancelDialogButton[] = "resetCancel";
constexpr char kTriggerPowerwashButton[] = "powerwash";
constexpr char kConfirmPowerwashButton[] = "confirmPowerwash";
constexpr char kCancelPowerwashButton[] = "cancelButton";
constexpr char kRestartButton[] = "restart";

void InvokeResetAccelerator() {
  ASSERT_TRUE(LoginScreenTestApi::SendAcceleratorNatively(ui::Accelerator(
      ui::VKEY_R, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
}

void ClickCancelButton() {
  test::OobeJS().TapOnPath({kResetScreen, kCancelDialogButton});
}

void CloseResetScreenAndWait() {
  test::OobeJS().TapOnPath({kResetScreen, kCancelDialogButton});
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(false /* visible */, {kResetScreen})
      ->Wait();
}

void ClickResetButton() {
  test::OobeJS().TapOnPath({kResetScreen, kConfirmPowerwashButton});
}

void ClickRestartButton() {
  // Clicking on the button to restart can be flaky if a synchronous call is
  // used because the WebUI can be destroyed before it returns.
  test::TapOnPathAndWaitForOobeToBeDestroyed({kResetScreen, kRestartButton});
}

void ClickToConfirmButton() {
  test::OobeJS().TapOnPath({kResetScreen, kTriggerPowerwashButton});
}

void ClickDismissConfirmationButton() {
  test::OobeJS().TapOnPath({kResetScreen, kCancelPowerwashButton});
}

void WaitForConfirmationDialogToOpen() {
  test::OobeJS()
      .CreateWaiter(
          test::GetOobeElementPath({kResetScreen, kConfirmationDialog}) +
          ".open")
      ->Wait();
}

void WaitForConfirmationDialogToClose() {
  test::OobeJS()
      .CreateWaiter(
          test::GetOobeElementPath({kResetScreen, kConfirmationDialog}) +
          ".open === false")
      ->Wait();
}

void ExpectConfirmationDialogClosed() {
  test::OobeJS().ExpectAttributeEQ("open", {kResetScreen, kConfirmationDialog},
                                   false);
}

}  // namespace

class ResetTest : public OobeBaseTest, public LocalStateMixin::Delegate {
 public:
  ResetTest() {
    fake_statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }

  ResetTest(const ResetTest&) = delete;
  ResetTest& operator=(const ResetTest&) = delete;

  ~ResetTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(b/353731379): Remove when removing legacy state determination code.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // Simulates reset screen request from views based login.
  void InvokeResetScreen() {
    LoginDisplayHost::default_host()->HandleAccelerator(
        LoginAcceleratorAction::kShowResetScreen);
    OobeScreenWaiter(ResetView::kScreenId).Wait();
    test::OobeJS()
        .CreateVisibilityWaiter(true /* visible */, {kResetScreen})
        ->Wait();
    EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
    ExpectConfirmationDialogClosed();
  }

  void SetUpLocalState() override {}

  LocalStateMixin local_state_mixin_{&mixin_host_, this};

 private:
  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

class ResetOobeTest : public OobeBaseTest {
 public:
  ResetOobeTest() {
    fake_statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }

  ResetOobeTest(const ResetOobeTest&) = delete;
  ResetOobeTest& operator=(const ResetOobeTest&) = delete;

  ~ResetOobeTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(b/353731379): Remove when removing legacy state determination code.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // Simulates reset screen request from OOBE UI.
  void InvokeResetScreen() {
    InvokeResetAccelerator();
    OobeScreenWaiter(ResetView::kScreenId).Wait();
    EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
    ExpectConfirmationDialogClosed();
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

class ResetFirstAfterBootTest : public ResetTest {
 public:
  ~ResetFirstAfterBootTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }

  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  }
};

class ResetFirstAfterBootTestWithRollback : public ResetFirstAfterBootTest {
 public:
  ~ResetFirstAfterBootTestWithRollback() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ResetFirstAfterBootTest::SetUpInProcessBrowserTestFixture();
    update_engine_client()->set_can_rollback_check_result(true);
  }
};

class ResetTestWithTpmFirmwareUpdate : public ResetTest {
 public:
  ResetTestWithTpmFirmwareUpdate() = default;
  ~ResetTestWithTpmFirmwareUpdate() override = default;

  // ResetTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);

    if (!content::IsPreTest())
      command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }

  void SetUpInProcessBrowserTestFixture() override {
    tpm_firmware_update_checker_callback_ = base::BindRepeating(
        &ResetTestWithTpmFirmwareUpdate::HandleTpmFirmwareUpdateCheck,
        base::Unretained(this));
    ResetScreen::SetTpmFirmwareUpdateCheckerForTesting(
        &tpm_firmware_update_checker_callback_);
    ResetTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ResetTest::TearDownInProcessBrowserTestFixture();
    ResetScreen::SetTpmFirmwareUpdateCheckerForTesting(nullptr);
  }

  bool HasPendingTpmFirmwareUpdateCheck() const {
    return !pending_tpm_firmware_update_check_.is_null();
  }

  void FinishPendingTpmFirmwareUpdateCheck(
      const std::set<tpm_firmware_update::Mode>& modes) {
    std::move(pending_tpm_firmware_update_check_).Run(modes);
  }

 private:
  void HandleTpmFirmwareUpdateCheck(
      ResetScreen::TpmFirmwareUpdateAvailabilityCallback callback,
      base::TimeDelta delay) {
    EXPECT_EQ(delay, base::Seconds(10));
    // Multiple checks are technically allowed, but not needed by these tests.
    ASSERT_FALSE(pending_tpm_firmware_update_check_);
    pending_tpm_firmware_update_check_ = std::move(callback);
  }

  ResetScreen::TpmFirmwareUpdateAvailabilityCallback
      pending_tpm_firmware_update_check_;
  ResetScreen::TpmFirmwareUpdateAvailabilityChecker
      tpm_firmware_update_checker_callback_;
};

class ResetTestWithTpmFirmwareUpdateRequested
    : public ResetTestWithTpmFirmwareUpdate {
 public:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  }
};

IN_PROC_BROWSER_TEST_F(ResetTest, ShowAndCancelMultipleTimes) {
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  // Invoke and close reset screen multiple times to make sure it is shown and
  // hidden each time.
  InvokeResetScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  InvokeResetScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  InvokeResetScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforePowerwash) {
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());

  ClickRestartButton();

  ASSERT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  ASSERT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, ResetOnWelcomeScreen) {
  test::WaitForWelcomeScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();

  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, RequestAndCancleResetOnWelcomeScreen) {
  test::WaitForWelcomeScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();

  ClickCancelButton();
  test::WaitForWelcomeScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();

  // Rollback unavailable. Show and cancel.
  update_engine_client()->set_can_rollback_check_result(false);
  InvokeResetScreen();
  CloseResetScreenAndWait();
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  // Go to confirmation phase, cancel from there in 2 steps.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();

  ClickToConfirmButton();
  WaitForConfirmationDialogToOpen();

  ClickDismissConfirmationButton();
  WaitForConfirmationDialogToClose();

  test::OobeJS().ExpectVisible(kResetScreen);
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  // Rollback available. Show and cancel from confirmation screen.
  update_engine_client()->set_can_rollback_check_result(true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeResetAccelerator();

  ClickToConfirmButton();
  WaitForConfirmationDialogToOpen();

  ClickDismissConfirmationButton();
  WaitForConfirmationDialogToClose();

  test::OobeJS().ExpectVisible(kResetScreen);
  CloseResetScreenAndWait();
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ShowAfterBootIfRequested) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  test::OobeJS().CreateVisibilityWaiter(true, {kResetScreen})->Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, RollbackUnavailable) {
  InvokeResetScreen();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  InvokeResetAccelerator();  // No changes
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  CloseResetScreenAndWait();

  // Next invocation leads to rollback view.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  CloseResetScreenAndWait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback, RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();

  // PRE test triggers start with Reset screen.
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  CloseResetScreenAndWait();

  // Next invocation leads to simple reset, not rollback view.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeResetAccelerator();  // Shows rollback.
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  ClickDismissConfirmationButton();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreenAndWait();
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  CloseResetScreenAndWait();

  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeResetAccelerator();  // Shows rollback.
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client()->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       ErrorOnRollbackRequested) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());
  test::OobeJS().ExpectHasNoClass("revert-promise-view", {kResetScreen});

  InvokeResetAccelerator();
  ClickToConfirmButton();
  WaitForConfirmationDialogToOpen();
  ClickResetButton();
  WaitForConfirmationDialogToClose();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client()->rollback_call_count());
  test::OobeJS().ExpectHasClass("revert-promise-view", {kResetScreen});

  update_engine::StatusResult error_update_status;
  error_update_status.set_current_operation(update_engine::Operation::ERROR);
  update_engine_client()->NotifyObserversThatStatusChanged(error_update_status);
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, "rollbackError"})
      ->Wait();

  // Clicking 'ok' on the error screen will either show the previous OOBE screen
  // or show the login screen. Here login screen should appear because there's
  // no previous screen.
  test::OobeJS().TapOnPath({kResetScreen, "okButton"});

  OobeWindowVisibilityWaiter(false).Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback, RevertAfterCancel) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client()->rollback_call_count());

  test::OobeJS().ExpectVisible(kResetScreen);
  test::OobeJS().ExpectHasNoClass("rollback-proposal-view", {kResetScreen});

  InvokeResetAccelerator();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {kResetScreen})
      ->Wait();

  CloseResetScreenAndWait();
  InvokeResetScreen();

  InvokeResetAccelerator();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {kResetScreen})
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetFromSigninWithFirmwareUpdate) {
  InvokeResetScreen();

  test::OobeJS().ExpectHiddenPath({kResetScreen, kTpmUpdate});
  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS().ExpectHiddenPath({kResetScreen, kTpmUpdate});
  ClickRestartButton();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       ResetFromSigninWithFirmwareUpdate) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  test::OobeJS().ClickOnPath({kResetScreen, kTpmUpdateCheckbox});

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("first_boot",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdateRequested,
                       TpmFirmwareUpdateAvailableButNotSelected) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
}

class ResetTestWithTpmFirmwareUpdateCleanup
    : public ResetTestWithTpmFirmwareUpdate {
 public:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                      static_cast<int>(tpm_firmware_update::Mode::kCleanup));
  }
};

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdateCleanup,
                       ResetWithTpmCleanUp) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kCleanup});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("cleanup",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

class ResetTestWithTpmFirmwareUpdatePreserve
    : public ResetTestWithTpmFirmwareUpdate {
 public:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->SetInteger(
        prefs::kFactoryResetTPMFirmwareUpdateMode,
        static_cast<int>(tpm_firmware_update::Mode::kPreserveDeviceState));
  }
};

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdatePreserve,
                       ResetWithTpmUpdatePreservingDeviceState) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck(
      {tpm_firmware_update::Mode::kPreserveDeviceState,
       tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("preserve_stateful",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

class ResetTestWithTpmFirmwareUpdatePowerwash
    : public ResetTestWithTpmFirmwareUpdate {
 public:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                      static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
  }
};

// Tests that clicking TPM firmware update checkbox is no-op if the update was
// requested before the Reset screen was shown (e.g. on previous boot in
// settings, or by policy).
IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdatePowerwash,
                       TpmFirmwareUpdateRequestedBeforeShowNotEditable) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  test::OobeJS().ClickOnPath({kResetScreen, kTpmUpdateCheckbox});

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck(
      {tpm_firmware_update::Mode::kPreserveDeviceState,
       tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("first_boot",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdatePowerwash,
                       AvailableTpmUpdateModesChangeDuringRequest) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kResetScreen, kTpmUpdate})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
}

}  // namespace ash
