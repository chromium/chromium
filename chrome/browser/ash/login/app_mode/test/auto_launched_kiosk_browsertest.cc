// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

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
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }

  AutoLaunchedKioskTest(const AutoLaunchedKioskTest&) = delete;
  AutoLaunchedKioskTest& operator=(const AutoLaunchedKioskTest&) = delete;

  ~AutoLaunchedKioskTest() override = default;

  virtual std::string GetTestAppId() const {
    return KioskAppsMixin::kTestChromeAppId;
  }

  virtual std::string GetTestAppAccountId() const {
    return KioskAppsMixin::kEnterpriseKioskAccountId;
  }

  virtual std::vector<std::string> GetTestSecondaryAppIds() const {
    return std::vector<std::string>();
  }

  void SetUp() override {
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

    KioskAppsMixin::AppendAutoLaunchKioskAccount(
        device_policy_update->policy_payload(), GetTestAppId(),
        GetTestAppAccountId());

    device_policy_update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_.RequestDeviceLocalAccountPolicyUpdate(
            GetTestAppAccountId());
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
        GetTestAppAccountId(), policy::DeviceLocalAccountType::kKioskApp);
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
    auto app = KioskChromeAppManager::Get()->GetApp(app_id);
    if (!app.has_value()) {
      ADD_FAILURE() << "App " << app_id << " not found.";
      return false;
    }
    return app->was_auto_launched_with_zero_delay;
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

  FakeCWS fake_cws_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;
  base::AutoReset<bool> skip_splash_wait_override_ =
      KioskTestHelper::SkipSplashScreenWait();

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

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kTestChromeAppId));

  ASSERT_TRUE(CloseAppWindow(KioskAppsMixin::kTestChromeAppId));
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

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kTestChromeAppId));

  ASSERT_TRUE(CloseAppWindow(KioskAppsMixin::kTestChromeAppId));
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

  EXPECT_TRUE(IsKioskAppAutoLaunched(KioskAppsMixin::kTestChromeAppId));
}

// Used to test app auto-launch flow when the launched app is not kiosk enabled.
class AutoLaunchedNonKioskEnabledAppTest : public AutoLaunchedKioskTest {
 public:
  AutoLaunchedNonKioskEnabledAppTest() = default;

  AutoLaunchedNonKioskEnabledAppTest(
      const AutoLaunchedNonKioskEnabledAppTest&) = delete;
  AutoLaunchedNonKioskEnabledAppTest& operator=(
      const AutoLaunchedNonKioskEnabledAppTest&) = delete;

  ~AutoLaunchedNonKioskEnabledAppTest() override = default;

  std::string GetTestAppId() const override {
    // Chrome app without the `kiosk_enabled` field in the manifest. The source
    // code is in:
    //   //chrome/test/data/chromeos/app_mode/apps_and_extensions/
    //     non_kiosk_enabled_app/src/
    return "gbcgichpbeeimejckkpgnaighpndpped";
  }
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedNonKioskEnabledAppTest, NotLaunched) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_TRUE(termination_subscription_);

  EXPECT_TRUE(IsKioskAppAutoLaunched(GetTestAppId()));

  ExtensionTestMessageListener listener("launchRequested");

  // App launch should be canceled, and kiosk session stopped.
  base::RunLoop run_loop;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_EQ(
      KioskAppLaunchError::Error::kNotKioskEnabled,
      KioskAppLaunchError::Get(CHECK_DEREF(g_browser_process->local_state())));
}

// Used to test management API availability in kiosk sessions.
class ManagementApiKioskTest : public AutoLaunchedKioskTest {
 public:
  ManagementApiKioskTest() = default;

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
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  // The tests expects to recieve two test result messages:
  //  * result for tests run by the secondary kiosk app.
  //  * result for tests run by the primary kiosk app.
  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace ash
