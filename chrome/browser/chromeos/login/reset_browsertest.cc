// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

namespace {

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "test-user1@gmail.com";

}  // namespace

class ResetTest : public LoginManagerTest {
 public:
  ResetTest()
      : LoginManagerTest(false, true /* should_initialize_webui */),
        update_engine_client_(NULL),
        session_manager_client_(NULL),
        power_manager_client_(NULL) {}
  ~ResetTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    session_manager_client_ = new FakeSessionManagerClient;
    dbus_setter->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(session_manager_client_));
    power_manager_client_ = new FakePowerManagerClient;
    dbus_setter->SetPowerManagerClient(
        std::unique_ptr<PowerManagerClient>(power_manager_client_));
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(update_engine_client_));

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void RegisterSomeUser() {
    RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId));
    StartupUtils::MarkOobeCompleted();
  }

  bool JSExecuted(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  void InvokeResetScreen() {
    ASSERT_TRUE(JSExecuted("cr.ui.Oobe.handleAccelerator('reset');"));
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  }

  void InvokeRollbackOption() {
    ASSERT_TRUE(JSExecuted("cr.ui.Oobe.handleAccelerator('reset');"));
  }

  void HideRollbackOption() {
    ASSERT_TRUE(JSExecuted("cr.ui.Oobe.handleAccelerator('reset');"));
  }

  void CloseResetScreen() {
    ASSERT_TRUE(JSExecuted(
        "chrome.send('login.ResetScreen.userActed', ['cancel-reset']);"));
  }

  void ClickResetButton() {
    ASSERT_TRUE(JSExecuted(
        "chrome.send('login.ResetScreen.userActed', ['powerwash-pressed']);"));
  }

  void ClickRestartButton() {
    ASSERT_TRUE(JSExecuted(
        "chrome.send('login.ResetScreen.userActed', ['restart-pressed']);"));
  }
  void ClickToConfirmButton() {
    ASSERT_TRUE(JSExecuted(
        "chrome.send('login.ResetScreen.userActed', ['show-confirmation']);"));
  }
  void ClickDismissConfirmationButton() {
    ASSERT_TRUE(
        JSExecuted("chrome.send('login.ResetScreen.userActed', "
                   "['reset-confirm-dismissed']);"));
  }

  FakeUpdateEngineClient* update_engine_client_;
  FakeSessionManagerClient* session_manager_client_;
  FakePowerManagerClient* power_manager_client_;
};

class ResetFirstAfterBootTest : public ResetTest {
 public:
  ~ResetFirstAfterBootTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }
};

IN_PROC_BROWSER_TEST_F(ResetTest, PRE_ShowAndCancel) {
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetTest, ShowAndCancel) {
  JSExpect("!!document.querySelector('#reset.hidden')");
  InvokeResetScreen();
  JSExpect("!document.querySelector('#reset.hidden')");
  CloseResetScreen();
  JSExpect("!!document.querySelector('#reset.hidden')");
}

IN_PROC_BROWSER_TEST_F(ResetTest, PRE_RestartBeforePowerwash) {
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforePowerwash) {
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  ClickRestartButton();
  ASSERT_EQ(1, power_manager_client_->num_request_restart_calls());
  ASSERT_EQ(0, session_manager_client_->start_device_wipe_call_count());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
  update_engine_client_->set_can_rollback_check_result(false);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();

  // Rollback unavailable. Show and cancel.
  update_engine_client_->set_can_rollback_check_result(false);
  JSExpect("document.querySelector('#reset').hidden");
  JSExpect("document.querySelector('#overlay-reset').hidden");
  InvokeResetScreen();
  JSExpect("!document.querySelector('#reset').hidden");
  JSExpect("document.querySelector('#overlay-reset').hidden");
  CloseResetScreen();
  JSExpect("document.querySelector('#reset').hidden");

  // Go to confirmation phase, cancel from there in 2 steps.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  JSExpect("document.querySelector('#overlay-reset').hidden");
  ClickToConfirmButton();
  JSExpect("!document.querySelector('#overlay-reset').hidden");
  ClickDismissConfirmationButton();
  JSExpect("document.querySelector('#overlay-reset').hidden");
  JSExpect("!document.querySelector('#reset').hidden");
  CloseResetScreen();
  JSExpect("document.querySelector('#reset').hidden");

  // Rollback available. Show and cancel from confirmation screen.
  update_engine_client_->set_can_rollback_check_result(true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();
  JSExpect("document.querySelector('#overlay-reset').hidden");
  ClickToConfirmButton();
  JSExpect("!document.querySelector('#overlay-reset').hidden");
  ClickDismissConfirmationButton();
  JSExpect("document.querySelector('#overlay-reset').hidden");
  JSExpect("!document.querySelector('#reset').hidden");
  CloseResetScreen();
  JSExpect("document.querySelector('#reset').hidden");
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ShowAfterBootIfRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ShowAfterBootIfRequested) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  JSExpect("!document.querySelector('#reset').hidden");
  CloseResetScreen();
  JSExpect("document.querySelector('#reset').hidden");
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_RollbackUnavailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

// Disabled due to flakiness (crbug.com/870284)
IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, DISABLED_RollbackUnavailable) {
  update_engine_client_->set_can_rollback_check_result(false);

  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  InvokeRollbackOption();  // No changes
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(1, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();

  // Next invocation leads to rollback view.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(2, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, RollbackAvailable) {
  update_engine_client_->set_can_rollback_check_result(true);
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(1, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();

  // Next invocation leads to simple reset, not rollback view.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();  // Shows rollback.
  ClickDismissConfirmationButton();
  CloseResetScreen();
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(2, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();

  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();  // Shows rollback.
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(2, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ErrorOnRollbackRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ErrorOnRollbackRequested) {
  update_engine_client_->set_can_rollback_check_result(true);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  JSExpect("!$('reset').classList.contains('revert-promise-view')");
  InvokeRollbackOption();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
  JSExpect("$('reset').classList.contains('revert-promise-view')");
  UpdateEngineClient::Status error_update_status;
  error_update_status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  update_engine_client_->NotifyObserversThatStatusChanged(error_update_status);
  OobeScreenWaiter(OobeScreen::SCREEN_ERROR_MESSAGE).Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_RevertAfterCancel) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, RevertAfterCancel) {
  update_engine_client_->set_can_rollback_check_result(true);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  JSExpect("!$('reset').classList.contains('rollback-proposal-view')");
  InvokeRollbackOption();
  JSExpect("$('reset').classList.contains('rollback-proposal-view')");
  CloseResetScreen();
  InvokeResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  InvokeRollbackOption();
  JSExpect("$('reset').classList.contains('rollback-proposal-view')");
}

}  // namespace chromeos
