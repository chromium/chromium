// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace em = enterprise_management;

namespace chromeos {

class LoginScreenPolicyTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginScreenPolicyTest() = default;

  void RefreshDevicePolicyAndWaitForSettingChange(
      const char* cros_setting_name);

 protected:
  void WaitForLoginScreen() {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources())
        .Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void TearDownOnMainThread() override {
    // This shuts down the login UI.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
    policy::DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPolicyTest);
};

void LoginScreenPolicyTest::RefreshDevicePolicyAndWaitForSettingChange(
    const char* cros_setting_name) {
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  std::unique_ptr<CrosSettings::ObserverSubscription> subscription(
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          cros_setting_name, runner->QuitClosure()));

  RefreshDevicePolicy();
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(LoginScreenPolicyTest, RestrictInputMethods) {
  WaitForLoginScreen();

  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethods().size());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_input_methods()->add_login_screen_input_methods(
      "xkb:de::ger");
  RefreshDevicePolicyAndWaitForSettingChange(
      chromeos::kDeviceLoginScreenInputMethods);

  ASSERT_EQ(1U, imm->GetActiveIMEState()->GetAllowedInputMethods().size());

  // Remove the policy again
  proto.mutable_login_screen_input_methods()
      ->clear_login_screen_input_methods();
  RefreshDevicePolicyAndWaitForSettingChange(
      chromeos::kDeviceLoginScreenInputMethods);

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethods().size());
}

IN_PROC_BROWSER_TEST_F(LoginScreenPolicyTest, PolicyInputMethodsListEmpty) {
  WaitForLoginScreen();

  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethods().size());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_input_methods()->Clear();
  EXPECT_TRUE(proto.has_login_screen_input_methods());
  EXPECT_EQ(
      0, proto.login_screen_input_methods().login_screen_input_methods_size());
  RefreshDevicePolicyAndWaitForSettingChange(
      chromeos::kDeviceLoginScreenInputMethods);

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethods().size());
}

class LoginScreenGuestButtonPolicy : public LoginScreenPolicyTest {
 public:
  LoginScreenGuestButtonPolicy() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenGuestButtonPolicy);
};

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicy, NoUsers) {
  WaitForLoginScreen();

  // Default.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // When there are no users - should be the same as OOBE.
  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [true]);");
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
    RefreshDevicePolicyAndWaitForSettingChange(
        chromeos::kAccountsPrefAllowGuest);

    EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

    test::ExecuteOobeJS("chrome.send('showGuestInOobe', [true]);");
    // Should not affect.
    EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  }

  {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(true);
    RefreshDevicePolicyAndWaitForSettingChange(
        chromeos::kAccountsPrefAllowGuest);

    EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
  }
}

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicy, HasUsers) {
  WaitForLoginScreen();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  ash::LoginScreen::Get()->GetModel()->SetUserList({{}});
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Should not affect.
  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [true]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  ash::LoginScreen::Get()->GetModel()->SetUserList({});
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

class LoginScreenLocalePolicyTest : public LoginScreenPolicyTest {
 protected:
  LoginScreenLocalePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    LoginScreenPolicyTest::SetUpInProcessBrowserTestFixture();

    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_login_screen_locales()->add_login_screen_locales("fr-FR");
    RefreshDevicePolicy();
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenLocalePolicyTest,
                       DISABLED_PRE_LoginLocaleEnforcedByPolicy) {
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginScreenLocalePolicyTest,
                       DISABLED_LoginLocaleEnforcedByPolicy) {
  // Verifies that the default locale can be overridden with policy.
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  base::string16 french_title =
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_SIGNING_IN);

  // Make sure this is really French and differs from the English title.
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  base::string16 english_title =
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_SIGNING_IN);
  EXPECT_NE(french_title, english_title);
}

}  // namespace chromeos
