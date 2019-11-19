// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
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
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
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
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
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
    debug_daemon_client_ = new TestDebugDaemonClient;
    dbus_setter->SetDebugDaemonClient(
        std::unique_ptr<DebugDaemonClient>(debug_daemon_client_));

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
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
    test::ExecuteOobeJS("cr.ui.Oobe.handleAccelerator('debugging');");
    OobeScreenWaiter(EnableDebuggingScreenView::kScreenId).Wait();
  }

  void CloseEnableDebuggingScreen() {
    // TODO(crbug.com/944573): inline this method once UI is polymer-based.
    test::JSChecker js = test::OobeJS();
    js.set_polymer_ui(false);
    js.TapOn("debugging-cancel-button");
  }

  void ClickRemoveProtectionButton() {
    // TODO(crbug.com/944573): inline this method once UI is polymer-based.
    test::JSChecker js = test::OobeJS();
    js.set_polymer_ui(false);
    js.TapOn("debugging-remove-protection-button");
  }

  void ClickEnableButton() {
    // TODO(crbug.com/944573): inline this method once UI is polymer-based.
    test::JSChecker js = test::OobeJS();
    js.set_polymer_ui(false);
    js.TapOn("debugging-enable-button");
  }

  void ClickOKButton() {
    // TODO(crbug.com/944573): inline this method once UI is polymer-based.
    test::JSChecker js = test::OobeJS();
    js.set_polymer_ui(false);
    js.TapOn("debugging-ok-button");
  }

  void ShowRemoveProtectionScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        DebugDaemonClient::DEV_FEATURE_NONE);
    WaitUntilJSIsReady();
    test::OobeJS().ExpectHidden("debugging");
    InvokeEnableDebuggingScreen();
    test::OobeJS().ExpectVisible("debugging");
    test::OobeJS().ExpectVisible({"debugging-remove-protection-button"});
    test::OobeJS().ExpectVisible({"enable-debugging-help-link"});
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();
    VerifyRemoveProtectionScreen();
  }

  void VerifyRemoveProtectionScreen() {
    test::OobeJS().ExpectHasClass("remove-protection-view", {"debugging"});
    test::OobeJS().ExpectHasNoClass("setup-view", {"debugging"});
    test::OobeJS().ExpectHasNoClass("done-view", {"debugging"});
    test::OobeJS().ExpectHasNoClass("wait-view", {"debugging"});
  }

  void ShowSetupScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED);
    WaitUntilJSIsReady();
    test::OobeJS().ExpectHidden("debugging");
    InvokeEnableDebuggingScreen();
    test::OobeJS().ExpectVisible("debugging");
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();
    test::OobeJS().ExpectHasNoClass("remove-protection-view", {"debugging"});
    test::OobeJS().ExpectHasClass("setup-view", {"debugging"});
    test::OobeJS().ExpectHasNoClass("done-view", {"debugging"});
    test::OobeJS().ExpectHasNoClass("wait-view", {"debugging"});

    test::OobeJS().ExpectVisible("enable-debugging-passwords");
    test::OobeJS().ExpectVisible("enable-debugging-password");
    test::OobeJS().ExpectVisible("enable-debugging-password2");
    test::OobeJS().ExpectVisible("enable-debugging-setup-details");
    test::OobeJS().ExpectVisible("enable-debugging-password-note");
  }

  TestDebugDaemonClient* debug_daemon_client_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingTest);
};

// Show remove protection screen, click on [Cancel] button.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowAndCancelRemoveProtection) {
  ShowRemoveProtectionScreen();
  CloseEnableDebuggingScreen();
  test::OobeJS().ExpectHidden("debugging");

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
  test::OobeJS().ExpectHasClass("wait-view", {"debugging"});

  // Check if we have rebooted after enabling.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// Show setup screen. Click on [Enable] button. Wait until done screen is shown.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, ShowSetup) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateHasClassWaiter(true, "done-view", {"debugging"})->Wait();
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show setup screen. Type in matching passwords.
// Click on [Enable] button. Wait until done screen is shown.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, SetupMatchingPasswords) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  test::OobeJS().TypeIntoPath("test0000", {"enable-debugging-password"});
  test::OobeJS().TypeIntoPath("test0000", {"enable-debugging-password2"});
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateHasClassWaiter(true, "done-view", {"debugging"})->Wait();

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show setup screen. Type in different passwords.
// Click on [Enable] button. Assert done screen is not shown.
// Then confirm that typing in matching passwords enables debugging features.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, SetupNotMatchingPasswords) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  test::OobeJS().TypeIntoPath("test0000", {"enable-debugging-password"});
  test::OobeJS().TypeIntoPath("test9999", {"enable-debugging-password2"});
  ClickEnableButton();
  test::OobeJS()
      .CreateHasClassWaiter(false, "done-view", {"debugging"})
      ->Wait();

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);

  test::OobeJS().TypeIntoPath("test0000", {"enable-debugging-password2"});
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateHasClassWaiter(true, "done-view", {"debugging"})->Wait();

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
  test::OobeJS().ExpectHidden("debugging");
  InvokeEnableDebuggingScreen();
  test::OobeJS().ExpectVisible("debugging");
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
  test::OobeJS().ExpectHidden("debugging");
  InvokeEnableDebuggingScreen();
  test::OobeJS().ExpectVisible("debugging");
  test::OobeJS().ExpectHasClass("wait-view", {"debugging"});

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
  test::OobeJS().ExpectHidden("debugging");
  InvokeEnableDebuggingScreen();
  test::OobeJS().ExpectVisible("debugging");
  test::OobeJS()
      .CreateHasClassWaiter(true, "error-view", {"debugging"})
      ->Wait();
  test::OobeJS().ExpectHasNoClass("remove-protection-view", {"debugging"});
  test::OobeJS().ExpectHasNoClass("setup-view", {"debugging"});
  test::OobeJS().ExpectHasNoClass("done-view", {"debugging"});
  test::OobeJS().ExpectHasNoClass("wait-view", {"debugging"});
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
  OobeScreenWaiter(EnableDebuggingScreenView::kScreenId).Wait();
}

// Canceling auto shown setup screen should close it.
IN_PROC_BROWSER_TEST_F(EnableDebuggingRequestedTest, CancelAutoShowSetup) {
  OobeScreenWaiter(EnableDebuggingScreenView::kScreenId).Wait();
  CloseEnableDebuggingScreen();
  test::OobeJS().ExpectHidden("debugging");
}

}  // namespace chromeos
