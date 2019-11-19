// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggaeimfdpnmlhdhpcikgoblffmkckdmn
constexpr char kTestKioskApp[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";

// This is a simple test that only sends an extension message when app launch is
// requested. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/gbcgichpbeeimejckkpgnaighpndpped
constexpr char kTestNonKioskEnabledApp[] = "gbcgichpbeeimejckkpgnaighpndpped";

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
// The app is loaded alongside |kTestManagementApiKioskApp|. The tests are run
// in the response to a message sent from |kTestManagementApiKioskApp|.
// The app's CRX is located under:
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       kajpgkhinciaiihghpdamekpjpldgpfi.crx
// Source from which the CRX is generated is under path:
//   chrome/test/data/chromeos/app_mode/management_api/secondary_app/
constexpr char kTestManagementApiSecondaryApp[] =
    "kajpgkhinciaiihghpdamekpjpldgpfi";

constexpr char kTestAccountId[] = "enterprise-kiosk-app@localhost";

// Used to listen for app termination notification.
class TerminationObserver : public content::NotificationObserver {
 public:
  TerminationObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }
  ~TerminationObserver() override = default;

  // Whether app has been terminated - i.e. whether app termination notification
  // has been observed.
  bool terminated() const { return notification_seen_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ASSERT_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
    notification_seen_ = true;
  }

  bool notification_seen_ = false;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TerminationObserver);
};

}  // namespace

class AutoLaunchedKioskTest : public MixinBasedInProcessBrowserTest {
 public:
  AutoLaunchedKioskTest()
      : verifier_format_override_(crx_file::VerifierFormat::CRX3) {
    device_state_.set_domain("domain.com");
  }

  ~AutoLaunchedKioskTest() override = default;

  virtual std::string GetTestAppId() const { return kTestKioskApp; }
  virtual std::vector<std::string> GetTestSecondaryAppIds() const {
    return std::vector<std::string>();
  }

  void SetUp() override {
    AppLaunchController::SkipSplashWaitForTesting();
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
    for (const auto& secondary_app : secondary_apps)
      fake_cws_.SetUpdateCrx(secondary_app, secondary_app + ".crx", "1.0.0");

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
    device_local_accounts->set_auto_login_id(kTestAccountId);

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kTestAccountId);
    account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(GetTestAppId());

    device_policy_update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_.RequestDeviceLocalAccountPolicyUpdate(kTestAccountId);
    device_local_account_policy_update.reset();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void PreRunTestOnMainThread() override {
    // Initialize extension test message listener early on, as the test kiosk
    // app gets launched early in Chrome session setup for CrashRestore test.
    // Listeners created in IN_PROC_BROWSER_TEST might miss the messages sent
    // from the kiosk app.
    app_window_loaded_listener_ =
        std::make_unique<ExtensionTestMessageListener>("appWindowLoaded",
                                                       false);
    termination_observer_ = std::make_unique<TerminationObserver>();
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    app_window_loaded_listener_.reset();
    termination_observer_.reset();

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  const std::string GetTestAppUserId() const {
    return policy::GenerateDeviceLocalAccountUserId(
        kTestAccountId, policy::DeviceLocalAccount::TYPE_KIOSK_APP);
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
    if (!app_window_registry->GetAppWindowsForApp(app_id).empty())
      RunUntilBrowserProcessQuits();
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
  std::unique_ptr<TerminationObserver> termination_observer_;

 private:
  FakeCWS fake_cws_;
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;

  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
  LoginManagerMixin login_manager_{&mixin_host_, {}};

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DISALLOW_COPY_AND_ASSIGN(AutoLaunchedKioskTest);
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, PRE_CrashRestore) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // Check that policy flags have not been lost.
  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(app_window_loaded_listener_->WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestKioskApp));

  ASSERT_TRUE(CloseAppWindow(kTestKioskApp));
}

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, CrashRestore) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(app_window_loaded_listener_->WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestKioskApp));

  ASSERT_TRUE(CloseAppWindow(kTestKioskApp));
}

// Used to test app auto-launch flow when the launched app is not kiosk enabled.
class AutoLaunchedNonKioskEnabledAppTest : public AutoLaunchedKioskTest {
 public:
  AutoLaunchedNonKioskEnabledAppTest() {}
  ~AutoLaunchedNonKioskEnabledAppTest() override = default;

  std::string GetTestAppId() const override { return kTestNonKioskEnabledApp; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoLaunchedNonKioskEnabledAppTest);
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedNonKioskEnabledAppTest, NotLaunched) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestNonKioskEnabledApp));

  ExtensionTestMessageListener listener("launchRequested", false);

  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // App launch should be canceled, and user session stopped.
  termination_waiter.Wait();

  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_EQ(KioskAppLaunchError::NOT_KIOSK_ENABLED, KioskAppLaunchError::Get());
}

// Used to test management API availability in kiosk sessions.
class ManagementApiKioskTest : public AutoLaunchedKioskTest {
 public:
  ManagementApiKioskTest() {}
  ~ManagementApiKioskTest() override = default;

  // AutoLaunchedKioskTest:
  std::string GetTestAppId() const override {
    return kTestManagementApiKioskApp;
  }
  std::vector<std::string> GetTestSecondaryAppIds() const override {
    return {kTestManagementApiSecondaryApp};
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagementApiKioskTest);
};

IN_PROC_BROWSER_TEST_F(ManagementApiKioskTest, ManagementApi) {
  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // The tests expects to recieve two test result messages:
  //  * result for tests run by the secondary kiosk app.
  //  * result for tests run by the primary kiosk app.
  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace chromeos
