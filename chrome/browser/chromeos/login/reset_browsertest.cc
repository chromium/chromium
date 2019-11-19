// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "test-user1@gmail.com";

void InvokeRollbackOption() {
  test::ExecuteOobeJS("cr.ui.Oobe.handleAccelerator('reset');");
}

void CloseResetScreen() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['cancel-reset']);");
}

void ClickResetButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['powerwash-pressed']);");
}

void ClickRestartButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['restart-pressed']);");
}

void ClickToConfirmButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['show-confirmation']);");
}

void ClickDismissConfirmationButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', "
      "['reset-confirm-dismissed']);");
}

// Helper class that tracks whether 'login-prompt-visible' signal was requested
// from the session manager service.
class LoginPromptVisibleObserver : public SessionManagerClient::Observer {
 public:
  explicit LoginPromptVisibleObserver(
      SessionManagerClient* session_manager_client) {
    observer_.Add(session_manager_client);
  }
  ~LoginPromptVisibleObserver() override = default;

  bool signal_emitted() const { return signal_emitted_; }

  // SessionManagerClient::Observer:
  void EmitLoginPromptVisibleCalled() override {
    ASSERT_FALSE(signal_emitted_);
    signal_emitted_ = true;
  }

 private:
  bool signal_emitted_ = false;

  ScopedObserver<SessionManagerClient, SessionManagerClient::Observer>
      observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginPromptVisibleObserver);
};

}  // namespace

class ResetTest : public MixinBasedInProcessBrowserTest {
 public:
  ResetTest() = default;
  ~ResetTest() override = default;

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(update_engine_client_));

    SessionManagerClient::InitializeFakeInMemory();
    login_prompt_visible_observer_ =
        std::make_unique<LoginPromptVisibleObserver>(
            SessionManagerClient::Get());

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    login_prompt_visible_observer_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  // Simulates reset screen request from views based login.
  void InvokeResetScreen() {
    chromeos::LoginDisplayHost::default_host()->ShowResetScreen();
    OobeScreenWaiter(ResetView::kScreenId).Wait();
  }

  FakeUpdateEngineClient* update_engine_client_ = nullptr;
  std::unique_ptr<LoginPromptVisibleObserver> login_prompt_visible_observer_;

 private:
  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};

  DISALLOW_COPY_AND_ASSIGN(ResetTest);
};

class ResetOobeTest : public OobeBaseTest {
 public:
  ResetOobeTest() = default;
  ~ResetOobeTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(update_engine_client_));

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  // Simulates reset screen request from OOBE UI.
  void InvokeResetScreen() {
    test::ExecuteOobeJS("cr.ui.Oobe.handleAccelerator('reset');");
  }

  FakeUpdateEngineClient* update_engine_client_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResetOobeTest);
};

class ResetFirstAfterBootTest : public ResetTest {
 public:
  ~ResetFirstAfterBootTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }

  void SetUpOnMainThread() override {
    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();
  }
};

class ResetFirstAfterBootTestWithRollback : public ResetTest {
 public:
  ~ResetFirstAfterBootTestWithRollback() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }
  void SetUpInProcessBrowserTestFixture() override {
    ResetTest::SetUpInProcessBrowserTestFixture();
    update_engine_client_->set_can_rollback_check_result(true);
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
    EXPECT_EQ(delay, base::TimeDelta::FromSeconds(10));
    // Multiple checks are technically allowed, but not needed by these tests.
    ASSERT_FALSE(pending_tpm_firmware_update_check_);
    pending_tpm_firmware_update_check_ = std::move(callback);
  }

  ResetScreen::TpmFirmwareUpdateAvailabilityCallback
      pending_tpm_firmware_update_check_;
  ResetScreen::TpmFirmwareUpdateAvailabilityChecker
      tpm_firmware_update_checker_callback_;
};

IN_PROC_BROWSER_TEST_F(ResetTest, ShowAndCancel) {
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::OobeJS().ExpectVisible("reset");

  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforePowerwash) {
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  ClickRestartButton();
  ASSERT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
  ASSERT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, ResetOnWelcomeScreen) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();

  OobeScreenWaiter(ResetView::kScreenId).Wait();
  test::OobeJS().ExpectVisible("reset");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, RequestAndCancleResetOnWelcomeScreen) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  InvokeResetScreen();

  OobeScreenWaiter(ResetView::kScreenId).Wait();
  test::OobeJS().ExpectVisible("reset");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  CloseResetScreen();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectHidden("reset");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
}

// See http://crbug.com/990362 for details.
IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, DISABLED_PRE_ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  update_engine_client_->set_can_rollback_check_result(false);
}

// See http://crbug.com/990362 for details.
IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, DISABLED_ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();

  // Rollback unavailable. Show and cancel.
  update_engine_client_->set_can_rollback_check_result(false);
  InvokeResetScreen();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::OobeJS().CreateVisibilityWaiter(true, {"reset"})->Wait();
  test::OobeJS().ExpectHidden("overlay-reset");
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Go to confirmation phase, cancel from there in 2 steps.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"})->Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  ClickToConfirmButton();
  test::OobeJS().CreateVisibilityWaiter(true, {"overlay-reset"})->Wait();
  ClickDismissConfirmationButton();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"})->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, {"reset"})->Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Rollback available. Show and cancel from confirmation screen.
  update_engine_client_->set_can_rollback_check_result(true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();
  test::OobeJS().ExpectHidden("overlay-reset");
  ClickToConfirmButton();
  test::OobeJS().CreateVisibilityWaiter(true, {"overlay-reset"})->Wait();
  ClickDismissConfirmationButton();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"})->Wait();
  test::OobeJS().ExpectVisible("reset");
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ShowAfterBootIfRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ShowAfterBootIfRequested) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::OobeJS().CreateVisibilityWaiter(true, {"reset"})->Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_RollbackUnavailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, RollbackUnavailable) {
  InvokeResetScreen();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  InvokeRollbackOption();  // No changes
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Next invocation leads to rollback view.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

// See http://crbug.com/990362 for details.
IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       DISABLED_RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();

  // PRE test triggers start with Reset screen.
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Next invocation leads to simple reset, not rollback view.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  InvokeRollbackOption();  // Shows rollback.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  ClickDismissConfirmationButton();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  CloseResetScreen();
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();  // Shows rollback.
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_ErrorOnRollbackRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       ErrorOnRollbackRequested) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  test::OobeJS().ExpectHasNoClass("revert-promise-view", {"reset"});

  InvokeRollbackOption();
  ClickToConfirmButton();
  ClickResetButton();

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
  test::OobeJS().ExpectHasClass("revert-promise-view", {"reset"});

  update_engine::StatusResult error_update_status;
  error_update_status.set_current_operation(update_engine::Operation::ERROR);
  update_engine_client_->NotifyObserversThatStatusChanged(error_update_status);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  // Clicking 'ok' on the error screen will either show the previous OOBE screen
  // or show the login screen. Here login screen should appear because there's
  // no previous screen.
  test::OobeJS().TapOnPath({"error-message-md-ok-button"});

  OobeWindowVisibilityWaiter(false).Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_RevertAfterCancel) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

// This test frequently times out on sanitizer and debug build bots. See
// https://crbug.com/1025926.
IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       DISABLED_RevertAfterCancel) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());

  test::OobeJS().ExpectVisible("reset");
  test::OobeJS().ExpectHasNoClass("rollback-proposal-view", {"reset"});

  InvokeRollbackOption();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {"reset"})
      ->Wait();

  CloseResetScreen();
  OobeScreenExitWaiter(ResetView::kScreenId).Wait();

  InvokeResetScreen();
  OobeScreenWaiter(ResetView::kScreenId).Wait();

  InvokeRollbackOption();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {"reset"})
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetFromSigninWithFirmwareUpdate) {
  InvokeResetScreen();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  test::OobeJS().ExpectHiddenPath({"oobe-reset-md", "tpmFirmwareUpdate"});
  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS().ExpectHiddenPath({"oobe-reset-md", "tpmFirmwareUpdate"});
  ClickRestartButton();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       ResetFromSigninWithFirmwareUpdate) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();
  test::OobeJS().Evaluate(
      test::GetOobeElementPath({"oobe-reset-md", "tpmFirmwareUpdateCheckbox"}) +
      ".fire('click')");

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

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_TpmFirmwareUpdateAvailableButNotSelected) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       TpmFirmwareUpdateAvailableButNotSelected) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetWithTpmCleanUp) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kCleanup));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate, ResetWithTpmCleanUp) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
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

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetWithTpmUpdatePreservingDeviceState) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(
      prefs::kFactoryResetTPMFirmwareUpdateMode,
      static_cast<int>(tpm_firmware_update::Mode::kPreserveDeviceState));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       ResetWithTpmUpdatePreservingDeviceState) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
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

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_TpmFirmwareUpdateRequestedBeforeShowNotEditable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
}

// Tests that clicking TPM firmware update checkbox is no-op if the update was
// requested before the Reset screen was shown (e.g. on previous boot in
// settings, or by policy).
IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       TpmFirmwareUpdateRequestedBeforeShowNotEditable) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  test::OobeJS().Evaluate(
      test::GetOobeElementPath({"oobe-reset-md", "tpmFirmwareUpdateCheckbox"}) +
      ".fire('click')");

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

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_AvailableTpmUpdateModesChangeDuringRequest) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       AvailableTpmUpdateModesChangeDuringRequest) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(login_prompt_visible_observer_->signal_emitted());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
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

}  // namespace chromeos
