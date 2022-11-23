// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"

namespace ash {

IN_PROC_BROWSER_TEST_F(KioskBaseTest, KioskEnableCancel) {
  auto* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipPostLoginScreensForTesting();

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kEnableConsumerKiosk);

  // Wait for the kiosk_enable screen to show and cancel the screen.
  OobeScreenWaiter(KioskEnableScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"kiosk-enable", "close"});

  // Wait for the kiosk_enable screen to disappear.
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  // Check that the status still says configurable.
  EXPECT_EQ(KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskBaseTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  auto* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipPostLoginScreensForTesting();

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kEnableConsumerKiosk);

  // Wait for the kiosk_enable screen to show and enable kiosk.
  OobeScreenWaiter(KioskEnableScreenView::kScreenId).Wait();

  test::OobeJS().TapOnPath({"kiosk-enable", "enable"});
  // Wait for the signal that indicates Kiosk Mode is enabled.
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath({"kiosk-enable"}) +
                    ".state_ == 'success'")
      ->Wait();

  EXPECT_EQ(KioskAppManager::ConsumerKioskAutoLaunchStatus::kEnabled,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskBaseTest, KioskEnableAfter2ndSigninScreen) {
  auto* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipPostLoginScreensForTesting();

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kEnableConsumerKiosk);

  // Wait for the kiosk_enable screen to show and cancel the screen.
  OobeScreenWaiter(KioskEnableScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"kiosk-enable", "close"});

  // Navigate to gaia sign in screen.
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"user-creation", "nextButton"});

  // Wait for signin screen to appear again.
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Show kiosk enable screen again.
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kEnableConsumerKiosk);

  // And it should show up.
  OobeScreenWaiter(KioskEnableScreenView::kScreenId).Wait();
}

// TODO(https://crbug.com/934109): Fix kiosk launch when the device is
// untrusted.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DoNotLaunchWhenUntrusted DISABLED_DoNotLaunchWhenUntrusted
#else
#define MAYBE_DoNotLaunchWhenUntrusted DISABLED_DoNotLaunchWhenUntrusted
#endif
IN_PROC_BROWSER_TEST_F(KioskBaseTest, MAYBE_DoNotLaunchWhenUntrusted) {
  PrepareAppLaunch();
  SimulateNetworkOnline();

  // Make cros settings untrusted.
  settings_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);

  // Check that the attempt to start a kiosk app fails with an error.
  EXPECT_TRUE(LaunchApp(test_app_id()));
  bool ignored = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(),
      "if (cr.ui.Oobe.getInstance().errorMessageWasShownForTesting_) {"
      "  window.domAutomationController.send(true);"
      "} else {"
      "  cr.ui.Oobe.showSignInError = function(message, link, helpId) {"
      "    window.domAutomationController.send(true);"
      "  };"
      "}",
      &ignored));
}

// TODO(crbug.com/1149893): Migrate to KioskDeviceOwnedTest.
// This test is flaky (crbug.com/1294660).
IN_PROC_BROWSER_TEST_F(KioskBaseTest, DISABLED_SpokenFeedback) {
  test::SpeechMonitor sm;
  // Disable the TTS engine for testing so that we don't wait for the TTS engine
  // to load (the engine will never load on linux-chromeos builds).
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(false /* check launch data */,
                              false /* terminate app */,
                              true /* keep app open */);
  sm.ExpectSpeech("ChromeVox spoken feedback is ready");
  sm.Call([]() {
    // Navigate to the next object (should move to the heading and speak
    // it).
    // Trigger opening of the options page (should do nothing).
    // Then, force reading of 'done' given the right internal flag is
    // set.
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        AccessibilityManager::Get()->profile(),
        extension_misc::kChromeVoxExtensionId,
        R"(
          chrome.automation.getDesktop(d => {
            function waitForHeadingThenTest() {
              const heading =
                  d.find({role: chrome.automation.RoleType.HEADING});
              if (!heading) {
                setTimeout(waitForHeadingThenTest, 50);
                return;
              }
              ChromeVoxState.instance.navigateToRange(
                  cursors.Range.fromNode(heading));
              CommandHandler.onCommand('showOptionsPage');
              if (CommandHandler.isKioskSession_) {
                ChromeVox.tts.speak('done');
              }
            }
            waitForHeadingThenTest();
          });
        )");
  });
  sm.ExpectSpeech("Test Kiosk App 3 exclamations");
  sm.ExpectSpeech("Heading 1");
  sm.ExpectSpeech("done");
  sm.Replay();
}

class KioskAutoLaunchViewsTest : public OobeBaseTest,
                                 public LocalStateMixin::Delegate {
 public:
  KioskAutoLaunchViewsTest() { login_manager_mixin_.AppendRegularUsers(1); }

  KioskAutoLaunchViewsTest(const KioskAutoLaunchViewsTest& other) = delete;
  KioskAutoLaunchViewsTest& operator=(const KioskAutoLaunchViewsTest& other) =
      delete;

  ~KioskAutoLaunchViewsTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    owner_settings_service_ = std::make_unique<FakeOwnerSettingsService>(
        scoped_testing_cros_settings_.device_settings(), nullptr);

    // Add a new device local account and set its id for auto login.
    std::vector<policy::DeviceLocalAccount> accounts;
    accounts.emplace_back(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
                          kTestEnterpriseAccountId, kTestEnterpriseKioskApp,
                          "");
    policy::SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);
    scoped_testing_cros_settings_.device_settings()->SetString(
        kAccountsPrefDeviceLocalAccountAutoLoginId, kTestEnterpriseAccountId);
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    // Simulate auto login request from the previous session.
    PrefService* prefs = g_browser_process->local_state();
    ScopedDictPrefUpdate dict_update(prefs,
                                     KioskAppManager::kKioskDictionaryName);
    // The AutoLoginState is taken from KioskAppManager::AutoLoginState.
    dict_update->Set(
        KioskAppManager::kKeyAutoLoginState,
        static_cast<int>(KioskAppManager::AutoLoginState::kRequested));
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(KioskAutoLaunchViewsTest, ShowAutoLaunchScreen) {
  OobeScreenWaiter(KioskAutolaunchScreenView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace ash
