// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient()
      : got_reply_(false),
        num_query_debugging_features_(0),
        num_enable_debugging_features_(0),
        num_remove_protection_(0) {}

  ~TestDebugDaemonClient() override {}

  // FakeDebugDaemonClient overrides:
  void SetDebuggingFeaturesStatus(int featues_mask) override {
    ResetWait();
    FakeDebugDaemonClient::SetDebuggingFeaturesStatus(featues_mask);
  }

  void EnableDebuggingFeatures(
      const std::string& password,
      const EnableDebuggingCallback& callback) override {
    FakeDebugDaemonClient::EnableDebuggingFeatures(
        password, base::Bind(&TestDebugDaemonClient::OnEnableDebuggingFeatures,
                             base::Unretained(this), callback));
  }

  void RemoveRootfsVerification(
      const DebugDaemonClient::EnableDebuggingCallback& callback) override {
    FakeDebugDaemonClient::RemoveRootfsVerification(
        base::Bind(&TestDebugDaemonClient::OnRemoveRootfsVerification,
                   base::Unretained(this), callback));
  }

  void QueryDebuggingFeatures(
      const DebugDaemonClient::QueryDevFeaturesCallback& callback) override {
    LOG(WARNING) << "QueryDebuggingFeatures";
    FakeDebugDaemonClient::QueryDebuggingFeatures(
        base::Bind(&TestDebugDaemonClient::OnQueryDebuggingFeatures,
                   base::Unretained(this), callback));
  }

  void OnRemoveRootfsVerification(
      const DebugDaemonClient::EnableDebuggingCallback& original_callback,
      bool succeeded) {
    LOG(WARNING) << "OnRemoveRootfsVerification: succeeded = " << succeeded;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(original_callback, succeeded));
    if (runner_.get())
      runner_->Quit();
    else
      got_reply_ = true;

    num_remove_protection_++;
  }

  void OnQueryDebuggingFeatures(
      const DebugDaemonClient::QueryDevFeaturesCallback& original_callback,
      bool succeeded,
      int feature_mask) {
    LOG(WARNING) << "OnQueryDebuggingFeatures: succeeded = " << succeeded
                 << ", feature_mask = " << feature_mask;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(original_callback, succeeded, feature_mask));
    if (runner_.get())
      runner_->Quit();
    else
      got_reply_ = true;

    num_query_debugging_features_++;
  }

  void OnEnableDebuggingFeatures(
      const DebugDaemonClient::EnableDebuggingCallback& original_callback,
      bool succeeded) {
    LOG(WARNING) << "OnEnableDebuggingFeatures: succeeded = " << succeeded
                 << ", feature_mask = ";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(original_callback, succeeded));
    if (runner_.get())
      runner_->Quit();
    else
      got_reply_ = true;

    num_enable_debugging_features_++;
  }

  void ResetWait() {
    got_reply_ = false;
    num_query_debugging_features_ = 0;
    num_enable_debugging_features_ = 0;
    num_remove_protection_ = 0;
  }

  int num_query_debugging_features() const {
    return num_query_debugging_features_;
  }

  int num_enable_debugging_features() const {
    return num_enable_debugging_features_;
  }

  int num_remove_protection() const { return num_remove_protection_; }

  void WaitUntilCalled() {
    if (got_reply_)
      return;

    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> runner_;
  bool got_reply_;
  int num_query_debugging_features_;
  int num_enable_debugging_features_;
  int num_remove_protection_;
};

class EnableDebuggingTest : public LoginManagerTest {
 public:
  EnableDebuggingTest()
      : LoginManagerTest(false, true /* should_initialize_webui */),
        debug_daemon_client_(NULL),
        power_manager_client_(NULL) {}
  ~EnableDebuggingTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
    // Disable HID detection because it takes precedence and could block
    // enable-debugging UI.
    command_line->AppendSwitch(chromeos::switches::kDisableHIDDetectionOnOOBE);
  }

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new FakePowerManagerClient;
    dbus_setter->SetPowerManagerClient(
        std::unique_ptr<PowerManagerClient>(power_manager_client_));
    debug_daemon_client_ = new TestDebugDaemonClient;
    dbus_setter->SetDebugDaemonClient(
        std::unique_ptr<DebugDaemonClient>(debug_daemon_client_));

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  bool JSExecuted(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  void WaitUntilJSIsReady() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host)
      return;
    chromeos::OobeUI* oobe_ui = host->GetOobeUI();
    if (!oobe_ui)
      return;
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready)
      run_loop.Run();
  }

  void InvokeEnableDebuggingScreen() {
    ASSERT_TRUE(JSExecuted("cr.ui.Oobe.handleAccelerator('debugging');"));
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING).Wait();
  }

  void CloseEnableDebuggingScreen() {
    ASSERT_TRUE(JSExecuted("$('debugging-cancel-button').click();"));
  }

  void ClickRemoveProtectionButton() {
    ASSERT_TRUE(JSExecuted("$('debugging-remove-protection-button').click();"));
  }

  void ClickEnableButton() {
    ASSERT_TRUE(JSExecuted("$('debugging-enable-button').click();"));
  }

  void ClickOKButton() {
    ASSERT_TRUE(JSExecuted("$('debugging-ok-button').click();"));
  }

  void ShowRemoveProtectionScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        DebugDaemonClient::DEV_FEATURE_NONE);
    WaitUntilJSIsReady();
    JSExpect("!!document.querySelector('#debugging.hidden')");
    InvokeEnableDebuggingScreen();
    JSExpect("!document.querySelector('#debugging.hidden')");
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();
    VerifyRemoveProtectionScreen();
  }

  void VerifyRemoveProtectionScreen() {
    JSExpect("!!document.querySelector('#debugging.remove-protection-view')");
    JSExpect("!document.querySelector('#debugging.setup-view')");
    JSExpect("!document.querySelector('#debugging.done-view')");
    JSExpect("!document.querySelector('#debugging.wait-view')");
  }

  void ShowSetupScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED);
    WaitUntilJSIsReady();
    JSExpect("!!document.querySelector('#debugging.hidden')");
    InvokeEnableDebuggingScreen();
    JSExpect("!document.querySelector('#debugging.hidden')");
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();
    JSExpect("!document.querySelector('#debugging.remove-protection-view')");
    JSExpect("!!document.querySelector('#debugging.setup-view')");
    JSExpect("!document.querySelector('#debugging.done-view')");
    JSExpect("!document.querySelector('#debugging.wait-view')");
  }

  TestDebugDaemonClient* debug_daemon_client_;
  FakePowerManagerClient* power_manager_client_;
};

// Show remove protection screen, click on [Cancel] button.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowAndCancelRemoveProtection) {
  ShowRemoveProtectionScreen();
  CloseEnableDebuggingScreen();
  JSExpect("!!document.querySelector('#debugging.hidden')");

  EXPECT_EQ(debug_daemon_client_->num_query_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show remove protection, click on [Remove protection] button and wait for
// reboot.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowAndRemoveProtection) {
  ShowRemoveProtectionScreen();
  debug_daemon_client_->ResetWait();
  ClickRemoveProtectionButton();
  debug_daemon_client_->WaitUntilCalled();
  JSExpect("!!document.querySelector('#debugging.wait-view')");
  // Check if we have rebooted after enabling.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(power_manager_client_->num_request_restart_calls(), 1);
}

// Show setup screen. Click on [Enable] button. Wait until done screen is shown.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowSetup) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  base::RunLoop().RunUntilIdle();
  JSExpect("!!document.querySelector('#debugging.done-view')");
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Test images come with some features enabled but still has rootfs protection.
// Invoking debug screen should show remove protection screen.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowOnTestImages) {
  debug_daemon_client_->SetDebuggingFeaturesStatus(
      debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED |
      debugd::DevFeatureFlag::DEV_FEATURE_SYSTEM_ROOT_PASSWORD_SET);
  WaitUntilJSIsReady();
  JSExpect("!!document.querySelector('#debugging.hidden')");
  InvokeEnableDebuggingScreen();
  JSExpect("!document.querySelector('#debugging.hidden')");
  debug_daemon_client_->WaitUntilCalled();
  base::RunLoop().RunUntilIdle();
  VerifyRemoveProtectionScreen();

  EXPECT_EQ(debug_daemon_client_->num_query_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, WaitForDebugDaemon) {
  // Stat with service not ready.
  debug_daemon_client_->SetServiceIsAvailable(false);
  debug_daemon_client_->SetDebuggingFeaturesStatus(
      DebugDaemonClient::DEV_FEATURE_NONE);
  WaitUntilJSIsReady();

  // Invoking UI and it should land on wait-view.
  JSExpect("!!document.querySelector('#debugging.hidden')");
  InvokeEnableDebuggingScreen();
  JSExpect("!document.querySelector('#debugging.hidden')");
  JSExpect("!!document.querySelector('#debugging.wait-view')");

  // Mark service ready and it should proceed to remove protection view.
  debug_daemon_client_->SetServiceIsAvailable(true);
  debug_daemon_client_->WaitUntilCalled();
  base::RunLoop().RunUntilIdle();
  VerifyRemoveProtectionScreen();
}

class EnableDebuggingNonDevTest : public EnableDebuggingTest {
 public:
  EnableDebuggingNonDevTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip EnableDebuggingTest::SetUpCommandLine().
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetDebugDaemonClient(
        std::unique_ptr<DebugDaemonClient>(new FakeDebugDaemonClient));
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }
};

// Try to show enable debugging dialog, we should see error screen here.
IN_PROC_BROWSER_TEST_F(EnableDebuggingNonDevTest, NoShowInNonDevMode) {
  JSExpect("!!document.querySelector('#debugging.hidden')");
  InvokeEnableDebuggingScreen();
  JSExpect("!document.querySelector('#debugging.hidden')");
  base::RunLoop().RunUntilIdle();
  JSExpect("!!document.querySelector('#debugging.error-view')");
  JSExpect("!document.querySelector('#debugging.remove-protection-view')");
  JSExpect("!document.querySelector('#debugging.setup-view')");
  JSExpect("!document.querySelector('#debugging.done-view')");
  JSExpect("!document.querySelector('#debugging.wait-view')");
}

class EnableDebuggingRequestedTest : public EnableDebuggingTest {
 public:
  EnableDebuggingRequestedTest() {}

  // EnableDebuggingTest overrides:
  bool SetUpUserDataDirectory() override {
    base::DictionaryValue local_state_dict;
    local_state_dict.SetBoolean(prefs::kDebuggingFeaturesRequested, true);

    base::FilePath user_data_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    CHECK(
        JSONFileValueSerializer(local_state_path).Serialize(local_state_dict));

    return EnableDebuggingTest::SetUpUserDataDirectory();
  }
  void SetUpInProcessBrowserTestFixture() override {
    EnableDebuggingTest::SetUpInProcessBrowserTestFixture();

    debug_daemon_client_->SetDebuggingFeaturesStatus(
        debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED);
  }
};

// Setup screen is automatically shown when the feature is requested.
IN_PROC_BROWSER_TEST_F(EnableDebuggingRequestedTest, AutoShowSetup) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING).Wait();
}

// Canceling auto shown setup screen should close it.
IN_PROC_BROWSER_TEST_F(EnableDebuggingRequestedTest, CancelAutoShowSetup) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING).Wait();
  CloseEnableDebuggingScreen();
  JSExpect("!!document.querySelector('#debugging.hidden')");
}

}  // namespace chromeos
