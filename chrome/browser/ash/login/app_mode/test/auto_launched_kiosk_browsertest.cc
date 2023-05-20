// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {
namespace {

namespace em = ::enterprise_management;

// Primary kiosk app that runs tests for chrome.management API.
// The tests are run on the kiosk app launch event.
// It has a secondary test kiosk app, which is loaded alongside the app. The
// secondary app will send a message to run chrome.management API tests in
// in its context as well.
// The app's CRX is located under:
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       adinpkdaebaiabdlinlbjmenialdhibc.crx
// Source from which the CRX is generated is under path:
//   chrome/test/data/chromeos/app_mode/management_api/primary_app/
constexpr char kTestManagementApiKioskApp[] =
    "adinpkdaebaiabdlinlbjmenialdhibc";

// Secondary kiosk app that runs tests for chrome.management API.
// The app is loaded alongside `kTestManagementApiKioskApp`. The tests are run
// in the response to a message sent from `kTestManagementApiKioskApp`.
// The app's CRX is located under:
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       kajpgkhinciaiihghpdamekpjpldgpfi.crx
// Source from which the CRX is generated is under path:
//   chrome/test/data/chromeos/app_mode/management_api/secondary_app/
constexpr char kTestManagementApiSecondaryApp[] =
    "kajpgkhinciaiihghpdamekpjpldgpfi";

}  // namespace

class AutoLaunchedKioskTest : public OobeBaseTest {
 public:
  AutoLaunchedKioskTest()
      : verifier_format_override_(crx_file::VerifierFormat::CRX3) {
    device_state_.set_domain("domain.com");
  }

  AutoLaunchedKioskTest(const AutoLaunchedKioskTest&) = delete;
  AutoLaunchedKioskTest& operator=(const AutoLaunchedKioskTest&) = delete;

  ~AutoLaunchedKioskTest() override = default;

  virtual std::string GetTestAppId() const {
    return KioskAppsMixin::kKioskAppId;
  }
  virtual std::vector<std::string> GetTestSecondaryAppIds() const {
    return std::vector<std::string>();
  }

  void SetUp() override {
    skip_splash_wait_override_ =
        KioskLaunchController::SkipSplashScreenWaitForTesting();
    login_manager_.set_session_restore_enabled();
    login_manager_.SetDefaultLoginSwitches(
        {std::make_pair("test_switch_1", ""),
         std::make_pair("test_switch_2", "test_switch_2_value")});
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    fake_cws_.Init(embedded_test_server());
    fake_cws_.SetUpdateCrx(GetTestAppId(), GetTestAppId() + ".crx", "1.0.0");

    std::vector<std::string> secondary_apps = GetTestSecondaryAppIds();
    for (const auto& secondary_app : secondary_apps) {
      fake_cws_.SetUpdateCrx(secondary_app, secondary_app + ".crx", "1.0.0");
    }

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    SessionManagerClient::InitializeFakeInMemory();

    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);

    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_update->policy_payload()->mutable_device_local_accounts();
    device_local_accounts->set_auto_login_id(
        KioskAppsMixin::kEnterpriseKioskAccountId);

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(KioskAppsMixin::kEnterpriseKioskAccountId);
    account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(GetTestAppId());

    device_policy_update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_.RequestDeviceLocalAccountPolicyUpdate(
            KioskAppsMixin::kEnterpriseKioskAccountId);
    device_local_account_policy_update.reset();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void PreRunTestOnMainThread() override {
    // Initialize extension test message listener early on, as the test kiosk
    // app gets launched early in Chrome session setup for CrashRestore test.
    // Listeners created in IN_PROC_BROWSER_TEST might miss the messages sent
    // from the kiosk app.
    app_window_loaded_listener_ =
        std::make_unique<ExtensionTestMessageListener>("appWindowLoaded");
    termination_subscription_ =
        browser_shutdown::AddAppTerminatingCallback(base::DoNothing());

    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    app_window_loaded_listener_.reset();
    termination_subscription_ = {};

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  const std::string GetTestAppUserId() const {
    return policy::GenerateDeviceLocalAccountUserId(
        KioskAppsMixin::kEnterpriseKioskAccountId,
        policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }

  bool CloseAppWindow(const std::string& app_id) {
    Profile* const app_profile = ProfileManager::GetPrimaryUserProfile();
    if (!app_profile) {
      ADD_FAILURE() << "No primary (app) profile.";
      return false;
    }

    extensions::AppWindowRegistry* const app_window_registry =
        extensions::AppWindowRegistry::Get(app_profile);
    extensions::AppWindow* const window =
        apps::AppWindowWaiter(app_window_registry, app_id).Wait();
    if (!window) {
      ADD_FAILURE() << "No app window found for " << app_id << ".";
      return false;
    }

    window->GetBaseWindow()->Close();

    // Wait until the app terminates if it is still running.
    if (!app_window_registry->GetAppWindowsForApp(app_id).empty()) {
      RunUntilBrowserProcessQuits();
    }
    return true;
  }

  bool IsKioskAppAutoLaunched(const std::string& app_id) {
    KioskAppManager::App app;
    if (!KioskAppManager::Get()->GetApp(app_id, &app)) {
      ADD_FAILURE() << "App " << app_id << " not found.";
      return false;
    }
    return app.was_auto_launched_with_zero_delay;
  }

  void ExpectCommandLineHasDefaultPolicySwitches(
      const base::CommandLine& cmd_line) {
    EXPECT_TRUE(cmd_line.HasSwitch("test_switch_1"));
    EXPECT_EQ("", cmd_line.GetSwitchValueASCII("test_switch_1"));
    EXPECT_TRUE(cmd_line.HasSwitch("test_switch_2"));
    EXPECT_EQ("test_switch_2_value",
              cmd_line.GetSwitchValueASCII("test_switch_2"));
  }

 protected:
  std::unique_ptr<ExtensionTestMessageListener> app_window_loaded_listener_;
  base::CallbackListSubscription termination_subscription_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  FakeCWS fake_cws_;
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;
  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;

  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, PRE_CrashRestore) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_TRUE(termination_subscription_);

  // Check that policy flags have not been lost.
  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(app_window_loaded_listener_->WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kKioskAppId));

  ASSERT_TRUE(CloseAppWindow(KioskAppsMixin::kKioskAppId));
}

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, CrashRestore) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-6ac07cf6-6fe6-49d7-9398-769574c032ba");

  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_TRUE(termination_subscription_);

  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(app_window_loaded_listener_->WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kKioskAppId));

  ASSERT_TRUE(CloseAppWindow(KioskAppsMixin::kKioskAppId));
}

class AutoLaunchedKioskPowerWashRequestedTest
    : public OobeBaseTest,
      public LocalStateMixin::Delegate {
 public:
  AutoLaunchedKioskPowerWashRequestedTest() = default;
  ~AutoLaunchedKioskPowerWashRequestedTest() override = default;

  void SetUpLocalState() override {
    g_browser_process->local_state()->SetBoolean(prefs::kFactoryResetRequested,
                                                 true);
  }

  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskPowerWashRequestedTest, DoesNotLaunch) {
  OobeScreenWaiter(ResetView::kScreenId).Wait();
}

class AutoLaunchedKioskEphemeralUsersTest : public AutoLaunchedKioskTest {
 public:
  AutoLaunchedKioskEphemeralUsersTest() = default;
  ~AutoLaunchedKioskEphemeralUsersTest() override = default;

  // AutoLaunchedKioskTest:
  void SetUpInProcessBrowserTestFixture() override {
    AutoLaunchedKioskTest::SetUpInProcessBrowserTestFixture();
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);
  }
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskEphemeralUsersTest, Launches) {
  // Check that policy flags have not been lost.
  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(app_window_loaded_listener_->WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kKioskAppId));
}

// Used to test app auto-launch flow when the launched app is not kiosk enabled.
class AutoLaunchedNonKioskEnabledAppTest : public AutoLaunchedKioskTest {
 public:
  AutoLaunchedNonKioskEnabledAppTest() {}

  AutoLaunchedNonKioskEnabledAppTest(
      const AutoLaunchedNonKioskEnabledAppTest&) = delete;
  AutoLaunchedNonKioskEnabledAppTest& operator=(
      const AutoLaunchedNonKioskEnabledAppTest&) = delete;

  ~AutoLaunchedNonKioskEnabledAppTest() override = default;

  std::string GetTestAppId() const override { return kTestNonKioskEnabledApp; }
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedNonKioskEnabledAppTest, NotLaunched) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_TRUE(termination_subscription_);

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestNonKioskEnabledApp));

  ExtensionTestMessageListener listener("launchRequested");

  // App launch should be canceled, and kiosk session stopped.
  base::RunLoop run_loop;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_EQ(KioskAppLaunchError::Error::kNotKioskEnabled,
            KioskAppLaunchError::Get());
}

// Used to test management API availability in kiosk sessions.
class ManagementApiKioskTest : public AutoLaunchedKioskTest {
 public:
  ManagementApiKioskTest() {}

  ManagementApiKioskTest(const ManagementApiKioskTest&) = delete;
  ManagementApiKioskTest& operator=(const ManagementApiKioskTest&) = delete;

  ~ManagementApiKioskTest() override = default;

  // AutoLaunchedKioskTest:
  std::string GetTestAppId() const override {
    return kTestManagementApiKioskApp;
  }
  std::vector<std::string> GetTestSecondaryAppIds() const override {
    return {kTestManagementApiSecondaryApp};
  }
};

IN_PROC_BROWSER_TEST_F(ManagementApiKioskTest, ManagementApi) {
  // The tests expects to recieve two test result messages:
  //  * result for tests run by the secondary kiosk app.
  //  * result for tests run by the primary kiosk app.
  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace ash
