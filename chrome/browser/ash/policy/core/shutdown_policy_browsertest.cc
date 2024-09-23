// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/login_status.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/view.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

void WaitForShutdownButtonVisibility(bool visible) {
  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  while (LoginScreenTestApi::IsShutdownButtonShown() != visible) {
    LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
    ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  }
}

}  // namespace

class ShutdownPolicyBaseTest : public policy::DevicePolicyCrosBrowserTest,
                               public DeviceSettingsService::Observer {
 protected:
  ShutdownPolicyBaseTest() {}
  ~ShutdownPolicyBaseTest() override {}

  // DeviceSettingsService::Observer:
  void DeviceSettingsUpdated() override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  // policy::DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    tray_test_api_ = SystemTrayTestApi::Create();
  }

  // Updates the device shutdown policy and sets it to |reboot_on_shutdown|.
  void UpdateRebootOnShutdownPolicy(bool reboot_on_shutdown) {
    policy::DevicePolicyBuilder* builder = device_policy();
    ASSERT_TRUE(builder);
    em::ChromeDeviceSettingsProto& proto(builder->payload());
    proto.mutable_reboot_on_shutdown()->set_reboot_on_shutdown(
        reboot_on_shutdown);
  }

  // Refreshes device policy and waits for it to be applied.
  void SyncRefreshDevicePolicy() {
    run_loop_ = std::make_unique<base::RunLoop>();
    DeviceSettingsService::Get()->AddObserver(this);
    RefreshDevicePolicy();
    run_loop_->Run();
    DeviceSettingsService::Get()->RemoveObserver(this);
    run_loop_.reset();
  }

  // Blocks until the OobeUI indicates that the javascript side has been
  // initialized.
  void WaitUntilOobeUIIsReady(OobeUI* oobe_ui) {
    ASSERT_TRUE(oobe_ui);
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready) {
      run_loop.Run();
    }
  }

  bool result_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
};

class ShutdownPolicyInSessionTest : public ShutdownPolicyBaseTest {
 public:
  ShutdownPolicyInSessionTest(const ShutdownPolicyInSessionTest&) = delete;
  ShutdownPolicyInSessionTest& operator=(const ShutdownPolicyInSessionTest&) =
      delete;

 protected:
  ShutdownPolicyInSessionTest() {}
  ~ShutdownPolicyInSessionTest() override {}

  // Opens the system tray menu. This creates the tray views.
  void OpenSystemTrayMenu() { tray_test_api_->ShowBubble(); }

  // Closes the system tray menu. This deletes the tray views.
  void CloseSystemTrayMenu() { tray_test_api_->CloseBubble(); }

  // Returns true if the shutdown button's tooltip matches |tooltip|.
  bool HasShutdownButtonTooltip(const std::string& tooltip) {
    std::u16string actual_tooltip = tray_test_api_->GetShutdownButtonTooltip();
    return base::UTF8ToUTF16(tooltip) == actual_tooltip;
  }
};

// Tests that by default the shutdown button tooltip is "Power menu".
IN_PROC_BROWSER_TEST_F(ShutdownPolicyInSessionTest, TestBasic) {
  OpenSystemTrayMenu();
  EXPECT_TRUE(HasShutdownButtonTooltip("Power menu"));
  CloseSystemTrayMenu();
}

// Tests that enabling the reboot-on-shutdown policy changes the shutdown button
// tooltip to "restart". Note that the tooltip doesn't change dynamically if the
// menu is open during the policy change -- that's a rare condition and
// supporting it would add complexity.
//
// TODO(crbug.com/41393655): Disabled test due to flakiness.
IN_PROC_BROWSER_TEST_F(ShutdownPolicyInSessionTest, DISABLED_PolicyChange) {
  // Change the policy to reboot and let it propagate over mojo to ash.
  UpdateRebootOnShutdownPolicy(true);
  SyncRefreshDevicePolicy();
  content::RunAllPendingInMessageLoop();

  OpenSystemTrayMenu();
  EXPECT_TRUE(HasShutdownButtonTooltip("Restart"));
  CloseSystemTrayMenu();

  // Change the policy to shutdown and let it propagate over mojo to ash.
  UpdateRebootOnShutdownPolicy(false);
  SyncRefreshDevicePolicy();
  content::RunAllPendingInMessageLoop();

  OpenSystemTrayMenu();
  EXPECT_TRUE(HasShutdownButtonTooltip("Shut down"));
  CloseSystemTrayMenu();
}

class ShutdownPolicyLockerTest : public ShutdownPolicyBaseTest {
 public:
  ShutdownPolicyLockerTest(const ShutdownPolicyLockerTest&) = delete;
  ShutdownPolicyLockerTest& operator=(const ShutdownPolicyLockerTest&) = delete;

 protected:
  ShutdownPolicyLockerTest() = default;
  ~ShutdownPolicyLockerTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ShutdownPolicyBaseTest::SetUpInProcessBrowserTestFixture();
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SetUpOnMainThread() override {
    ShutdownPolicyBaseTest::SetUpOnMainThread();

    // Bring up the locker screen.
    ScreenLockerTester().Lock();
  }

  void TearDownOnMainThread() override {
    ScreenLocker::Hide();
    ShutdownPolicyBaseTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
};

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLockerTest, TestBasic) {
  ScreenLockerTester tester;
  EXPECT_FALSE(tester.IsLockRestartButtonShown());
  EXPECT_TRUE(tester.IsLockShutdownButtonShown());
}

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLockerTest, PolicyChange) {
  ScreenLockerTester tester;
  UpdateRebootOnShutdownPolicy(true);
  RefreshDevicePolicy();
  WaitForShutdownButtonVisibility(false);
  EXPECT_TRUE(tester.IsLockRestartButtonShown());

  UpdateRebootOnShutdownPolicy(false);
  RefreshDevicePolicy();
  WaitForShutdownButtonVisibility(true);
  EXPECT_FALSE(tester.IsLockRestartButtonShown());
}

class ShutdownPolicyLoginTest : public ShutdownPolicyBaseTest {
 public:
  ShutdownPolicyLoginTest(const ShutdownPolicyLoginTest&) = delete;
  ShutdownPolicyLoginTest& operator=(const ShutdownPolicyLoginTest&) = delete;

 protected:
  ShutdownPolicyLoginTest() = default;
  ~ShutdownPolicyLoginTest() override = default;

  // ShutdownPolicyBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void SetUpOnMainThread() override {
    ShutdownPolicyBaseTest::SetUpOnMainThread();

    LoginOrLockScreenVisibleWaiter().Wait();
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    ASSERT_TRUE(host->GetOobeWebContents());

    // Wait for the login UI to be ready.
    WaitUntilOobeUIIsReady(host->GetOobeUI());

    // Wait for GAIA screen, as it is the first screen and Shutdown button
    // visibility depends on if it is first sign-in step or not.
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
  }
};

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLoginTest, PolicyNotSet) {
  EXPECT_FALSE(LoginScreenTestApi::IsRestartButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsShutdownButtonShown());
}

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLoginTest, PolicyChange) {
  UpdateRebootOnShutdownPolicy(true);
  RefreshDevicePolicy();
  WaitForShutdownButtonVisibility(false);
  EXPECT_TRUE(LoginScreenTestApi::IsRestartButtonShown());

  UpdateRebootOnShutdownPolicy(false);
  RefreshDevicePolicy();
  WaitForShutdownButtonVisibility(true);
  EXPECT_FALSE(LoginScreenTestApi::IsRestartButtonShown());
}

}  // namespace ash
