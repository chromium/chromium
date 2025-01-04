// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const test::UIPath kSplashScreenLaunchText = {"app-launch-splash",
                                              "launchText"};

void WaitForNetworkTimeoutMessage() {
  test::TestPredicateWaiter(base::BindRepeating([]() {
    return test::OobeJS().GetString(
               ash::test::GetOobeElementPath(kSplashScreenLaunchText) +
               ".textContent") ==
           l10n_util::GetStringUTF8(IDS_APP_START_NETWORK_WAIT_TIMEOUT_MESSAGE);
  })).Wait();
}

// Helper class to count how many times an extension has been loaded.
class ExtensionReadyObserver : public extensions::ExtensionRegistryObserver {
 public:
  ExtensionReadyObserver(extensions::ExtensionRegistry* registry,
                         const extensions::ExtensionId& extension_id)
      : extension_id_(extension_id) {
    extension_registry_observation_.Observe(registry);
  }

  int fired_times() const { return count_; }

 private:
  // extensions::ExtensionReadyObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override {
    if (extension->id() == extension_id_) {
      count_++;
    }
  }

  int count_ = 0;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      extension_registry_observation_{this};
  const extensions::ExtensionId extension_id_;
};

}  // namespace

// Kiosk tests with a fake device owner setup.
class KioskDeviceOwnedTest : public KioskBaseTest {
 public:
  KioskDeviceOwnedTest() {
    settings_helper_.Set(kDeviceOwner,
                         base::Value(test_owner_account_id_.GetUserEmail()));
    login_manager_.AppendRegularUsers(1);
  }

  void SetUpOnMainThread() override {
    KioskBaseTest::SetUpOnMainThread();

    GetFakeUserManager().SetOwnerId(test_owner_account_id_);
  }

 protected:
  LoginManagerMixin login_manager_{
      &mixin_host_,
      {{LoginManagerMixin::TestUserInfo{test_owner_account_id_}}}};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

 private:
  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }
};

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       LaunchAppNetworkDownConfigureNotAllowed) {
  auto auto_reset =
      NetworkUiController::SetCanConfigureNetworkForTesting(false);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(NetworkStatus::kOffline);
  OobeScreenWaiter splash_waiter(AppLaunchSplashScreenView::kScreenId);
  splash_waiter.Wait();

  WaitForNetworkTimeoutMessage();

  // Configure network link should not be visible.
  test::OobeJS().ExpectHiddenPath(kConfigNetwork);

  // Network becomes online and app launch is resumed.
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

// Verifies that an enterprise device does not auto-launch kiosk mode when cros
// settings are untrusted.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       NoEnterpriseAutoLaunchWhenUntrusted) {
  PrepareAppLaunch();
  SimulateNetworkOnline();

  // Make cros settings untrusted.
  settings_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);

  // Trigger the code that handles auto-launch on enterprise devices. This would
  // normally be called from ShowLoginWizard(), which runs so early that it is
  // not possible to inject an auto-launch policy before it runs.
  LoginDisplayHost* login_display_host = LoginDisplayHost::default_host();
  ASSERT_TRUE(login_display_host);
  login_display_host->StartKiosk(test_kiosk_app().id(), true);

  // Check that no launch has started.
  EXPECT_FALSE(KioskController::Get().IsSessionStarting());
}

// This test verifies that accessibility extensions do not preserve any local
// data in-between session, as opposed to what they usually do in user sessions.
// See crbug.com/1049566
IN_PROC_BROWSER_TEST_F(
    KioskDeviceOwnedTest,
    PRE_AccessibilityExtensionsResetTheirStateUponSessionRestart) {
  test::SpeechMonitor speech_monitor;
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();

  ExtensionReadyObserver ready_observer(
      extensions::ExtensionRegistry::Get(app_profile),
      extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor.Replay();

  // Extension should be loaded only once.
  EXPECT_EQ(ready_observer.fired_times(), 1);

  static constexpr char kSetInStorageAPI[] =
      R"(chrome.storage.local.set(
             {test: 'testValue'},
             () => { chrome.test.sendScriptResult('') });)";
  // Store some data using Storage API for the extension.
  extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      app_profile, extension_misc::kChromeVoxExtensionId, kSetInStorageAPI);

  // Expect the data to be saved.
  static constexpr char kGetFromStorageAPI[] =
      R"(chrome.storage.local.get(
             'test',
             (value) => chrome.test.sendScriptResult(value.test));)";
  EXPECT_EQ("testValue",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));

  // The data should persist when extension is restarted.
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  test::SpeechMonitor speech_monitor2;
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor2.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor2.Replay();

  // Expect the data to be still there.
  EXPECT_EQ("testValue",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));
}

// TODO(crbug.com/1395435): Re-enable this test
IN_PROC_BROWSER_TEST_F(
    KioskDeviceOwnedTest,
    DISABLED_AccessibilityExtensionsResetTheirStateUponSessionRestart) {
  test::SpeechMonitor speech_monitor;
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();

  ExtensionReadyObserver ready_observer(
      extensions::ExtensionRegistry::Get(app_profile),
      extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor.Replay();

  // Extension should be loaded only once.
  EXPECT_EQ(ready_observer.fired_times(), 1);

  // Expect the data to be cleared.
  static constexpr char kGetFromStorageAPI[] =
      R"(
      chrome.storage.local.get(
          "test",
          function(value) {
              chrome.test.sendScriptResult(value.test == undefined ?
                  "<none>" : value.test);
          }
      );
      )";
  EXPECT_EQ("<none>",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));

  static constexpr char kGetFromLocalStorage[] =
      R"(
      chrome.test.sendScriptResult(
          localStorage.getItem('test2') == undefined ?
              "<none>" : localStorage.getItem('test2'));
      )";
  EXPECT_EQ("<none>",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromLocalStorage));
}

}  // namespace ash
