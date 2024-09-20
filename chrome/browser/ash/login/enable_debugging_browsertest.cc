// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

constexpr char kDebuggingScreenId[] = "debugging";

const test::UIPath kRemoveProtectionDialog = {kDebuggingScreenId,
                                              "removeProtectionDialog"};
const test::UIPath kSetupDialog = {kDebuggingScreenId, "setupDialog"};
const test::UIPath kWaitDialog = {kDebuggingScreenId, "waitDialog"};
const test::UIPath kDoneDialog = {kDebuggingScreenId, "doneDialog"};
const test::UIPath kErrorDialog = {kDebuggingScreenId, "errorDialog"};

const test::UIPath kHelpLink = {kDebuggingScreenId, "help-link"};
const test::UIPath kPasswordInput = {kDebuggingScreenId, "password"};
const test::UIPath kPassword2Input = {kDebuggingScreenId, "passwordRepeat"};
const test::UIPath kPasswordNote = {kDebuggingScreenId, "password-note"};

const test::UIPath kCancelButton = {kDebuggingScreenId,
                                    "removeProtectionCancelButton"};
const test::UIPath kEnableButton = {kDebuggingScreenId, "enableButton"};
const test::UIPath kRemoveProtectionButton = {kDebuggingScreenId,
                                              "removeProtectionProceedButton"};

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;

  ~TestDebugDaemonClient() override = default;

  // FakeDebugDaemonClient overrides:
  void SetDebuggingFeaturesStatus(int featues_mask) override {
    ResetWait();
    FakeDebugDaemonClient::SetDebuggingFeaturesStatus(featues_mask);
  }

  void EnableDebuggingFeatures(const std::string& password,
                               EnableDebuggingCallback callback) override {
    FakeDebugDaemonClient::EnableDebuggingFeatures(
        password,
        base::BindOnce(&TestDebugDaemonClient::OnEnableDebuggingFeatures,
                       base::Unretained(this), std::move(callback)));
  }

  void RemoveRootfsVerification(EnableDebuggingCallback callback) override {
    FakeDebugDaemonClient::RemoveRootfsVerification(
        base::BindOnce(&TestDebugDaemonClient::OnRemoveRootfsVerification,
                       base::Unretained(this), std::move(callback)));
  }

  void QueryDebuggingFeatures(QueryDevFeaturesCallback callback) override {
    LOG(WARNING) << "QueryDebuggingFeatures";
    FakeDebugDaemonClient::QueryDebuggingFeatures(
        base::BindOnce(&TestDebugDaemonClient::OnQueryDebuggingFeatures,
                       base::Unretained(this), std::move(callback)));
  }

  void OnRemoveRootfsVerification(EnableDebuggingCallback original_callback,
                                  bool succeeded) {
    LOG(WARNING) << "OnRemoveRootfsVerification: succeeded = " << succeeded;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(original_callback), succeeded));
    if (runner_.get())
      runner_->Quit();
    else
      got_reply_ = true;

    num_remove_protection_++;
  }

  void OnQueryDebuggingFeatures(QueryDevFeaturesCallback original_callback,
                                bool succeeded,
                                int feature_mask) {
    LOG(WARNING) << "OnQueryDebuggingFeatures: succeeded = " << succeeded
                 << ", feature_mask = " << feature_mask;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(original_callback), succeeded, feature_mask));
    if (runner_.get())
      runner_->Quit();
    else
      got_reply_ = true;

    num_query_debugging_features_++;
  }

  void OnEnableDebuggingFeatures(EnableDebuggingCallback original_callback,
                                 bool succeeded) {
    LOG(WARNING) << "OnEnableDebuggingFeatures: succeeded = " << succeeded
                 << ", feature_mask = ";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(original_callback), succeeded));
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
  bool got_reply_ = false;
  int num_query_debugging_features_ = 0;
  int num_enable_debugging_features_ = 0;
  int num_remove_protection_ = 0;
};

}  // namespace

class EnableDebuggingTestBase : public OobeBaseTest {
 public:
  EnableDebuggingTestBase() = default;

  EnableDebuggingTestBase(const EnableDebuggingTestBase&) = delete;
  EnableDebuggingTestBase& operator=(const EnableDebuggingTestBase&) = delete;

  ~EnableDebuggingTestBase() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Disable HID detection because it takes precedence and could block
    // enable-debugging UI.
    command_line->AppendSwitch(switches::kDisableHIDDetectionOnOOBEForTesting);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    debug_daemon_client_ = std::make_unique<TestDebugDaemonClient>();
    DebugDaemonClient::SetInstanceForTest(debug_daemon_client_.get());
  }

  void TearDownInProcessBrowserTestFixture() override {
    DebugDaemonClient::SetInstanceForTest(nullptr);
    debug_daemon_client_.reset();
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void InvokeEnableDebuggingScreen() {
    LoginDisplayHost::default_host()->HandleAccelerator(
        LoginAcceleratorAction::kEnableDebugging);

    OobeScreenWaiter(EnableDebuggingScreenView::kScreenId).Wait();
  }

  void CloseEnableDebuggingScreen() { test::OobeJS().TapOnPath(kCancelButton); }

  void ClickEnableButton() { test::OobeJS().TapOnPath(kEnableButton); }

  void ShowRemoveProtectionScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        DebugDaemonClient::DEV_FEATURE_NONE);
    WaitForOobeUI();
    test::OobeJS().ExpectHidden(kDebuggingScreenId);
    InvokeEnableDebuggingScreen();
    test::OobeJS()
        .CreateVisibilityWaiter(true, kRemoveProtectionDialog)
        ->Wait();
    test::OobeJS().ExpectVisiblePath(kRemoveProtectionButton);
    test::OobeJS().ExpectVisiblePath(kHelpLink);
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();
  }

  void ShowSetupScreen() {
    debug_daemon_client_->SetDebuggingFeaturesStatus(
        debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED);
    WaitForOobeUI();
    test::OobeJS().ExpectHidden(kDebuggingScreenId);
    InvokeEnableDebuggingScreen();
    test::OobeJS().CreateVisibilityWaiter(true, kSetupDialog)->Wait();
    debug_daemon_client_->WaitUntilCalled();
    base::RunLoop().RunUntilIdle();

    test::OobeJS().ExpectVisiblePath(kPasswordInput);
    test::OobeJS().ExpectVisiblePath(kPassword2Input);
    test::OobeJS().ExpectVisiblePath(kPasswordNote);
  }

  std::unique_ptr<TestDebugDaemonClient> debug_daemon_client_;
};

class EnableDebuggingDevTest : public EnableDebuggingTestBase {
 public:
  EnableDebuggingDevTest() = default;
  ~EnableDebuggingDevTest() override = default;

  // EnableDebuggingTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableDebuggingTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }
};

// Show remove protection screen, click on [Cancel] button.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, ShowAndCancelRemoveProtection) {
  ShowRemoveProtectionScreen();
  CloseEnableDebuggingScreen();
  test::OobeJS().ExpectHidden(kDebuggingScreenId);

  EXPECT_EQ(debug_daemon_client_->num_query_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show remove protection, click on [Remove protection] button and wait for
// reboot.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, ShowAndRemoveProtection) {
  // Disarm faked reboot, otherwise Chrome just stops and there's nothing to
  // verify.
  chromeos::FakePowerManagerClient* fake_power_manager_client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_NE(fake_power_manager_client, nullptr);
  fake_power_manager_client->set_restart_callback(base::DoNothing());

  ShowRemoveProtectionScreen();
  debug_daemon_client_->ResetWait();
  test::OobeJS().TapOnPath(kRemoveProtectionButton);
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().ExpectVisiblePath(kWaitDialog);

  // Check if we have rebooted after enabling.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(fake_power_manager_client->num_request_restart_calls(), 1);
}

// Show setup screen. Click on [Enable] button. Wait until done screen is shown.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, ShowSetup) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateVisibilityWaiter(true, kDoneDialog)->Wait();

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show setup screen. Type in matching passwords.
// Click on [Enable] button. Wait until done screen is shown.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, SetupMatchingPasswords) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  test::OobeJS().TypeIntoPath("test0000", kPasswordInput);
  test::OobeJS().TypeIntoPath("test0000", kPassword2Input);
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateVisibilityWaiter(true, kDoneDialog)->Wait();

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Show setup screen. Type in different passwords.
// Click on [Enable] button. Assert done screen is not shown.
// Then confirm that typing in matching passwords enables debugging features.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, SetupNotMatchingPasswords) {
  ShowSetupScreen();
  debug_daemon_client_->ResetWait();
  test::OobeJS().TypeIntoPath("test0000", kPasswordInput);
  test::OobeJS().TypeIntoPath("test9999", kPassword2Input);
  test::OobeJS().ExpectDisabledPath(kEnableButton);

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);

  test::OobeJS().TypeIntoPath("test0000", kPassword2Input);
  ClickEnableButton();
  debug_daemon_client_->WaitUntilCalled();
  test::OobeJS().CreateVisibilityWaiter(true, kDoneDialog)->Wait();

  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

// Test images come with some features enabled but still has rootfs protection.
// Invoking debug screen should show remove protection screen.
IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, ShowOnTestImages) {
  debug_daemon_client_->SetDebuggingFeaturesStatus(
      debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED |
      debugd::DevFeatureFlag::DEV_FEATURE_SYSTEM_ROOT_PASSWORD_SET);
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebuggingScreenId);
  InvokeEnableDebuggingScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kRemoveProtectionDialog)->Wait();
  debug_daemon_client_->WaitUntilCalled();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(debug_daemon_client_->num_query_debugging_features(), 1);
  EXPECT_EQ(debug_daemon_client_->num_enable_debugging_features(), 0);
  EXPECT_EQ(debug_daemon_client_->num_remove_protection(), 0);
}

IN_PROC_BROWSER_TEST_F(EnableDebuggingDevTest, WaitForDebugDaemon) {
  // Stat with service not ready.
  debug_daemon_client_->SetServiceIsAvailable(false);
  debug_daemon_client_->SetDebuggingFeaturesStatus(
      DebugDaemonClient::DEV_FEATURE_NONE);
  WaitForOobeUI();

  // Invoking UI and it should land on wait-view.
  test::OobeJS().ExpectHidden(kDebuggingScreenId);
  InvokeEnableDebuggingScreen();
  test::OobeJS().ExpectVisiblePath(kWaitDialog);

  // Mark service ready and it should proceed to remove protection view.
  debug_daemon_client_->SetServiceIsAvailable(true);
  debug_daemon_client_->WaitUntilCalled();
  base::RunLoop().RunUntilIdle();
  test::OobeJS().ExpectVisiblePath(kRemoveProtectionDialog);
}

// Uses the base class setup, with a TestDebugDaemonClient.
using EnableDebuggingTest = EnableDebuggingTestBase;

// Try to show enable debugging dialog, we should see error screen here.
IN_PROC_BROWSER_TEST_F(EnableDebuggingTest, NoShowInNonDevMode) {
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebuggingScreenId);
  InvokeEnableDebuggingScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
}

class EnableDebuggingRequestedTest : public EnableDebuggingDevTest {
 public:
  EnableDebuggingRequestedTest() {}

  // EnableDebuggingDevTest overrides:
  bool SetUpUserDataDirectory() override {
    base::Value::Dict local_state_dict;
    local_state_dict.Set(prefs::kDebuggingFeaturesRequested, true);

    base::FilePath user_data_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    CHECK(
        JSONFileValueSerializer(local_state_path).Serialize(local_state_dict));

    return EnableDebuggingDevTest::SetUpUserDataDirectory();
  }
  void SetUpInProcessBrowserTestFixture() override {
    EnableDebuggingDevTest::SetUpInProcessBrowserTestFixture();

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
  test::OobeJS().ExpectHidden(kDebuggingScreenId);
}

}  // namespace ash
