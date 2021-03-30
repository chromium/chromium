// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {
const char kDomain[] = "domain.com";
}  // namespace

class LoginScreenPolicyTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginScreenPolicyTest() = default;

  void RefreshDevicePolicyAndWaitForSettingChange(
      const char* cros_setting_name);

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPolicyTest);
};

void LoginScreenPolicyTest::RefreshDevicePolicyAndWaitForSettingChange(
    const char* cros_setting_name) {
  policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      {cros_setting_name});
}

IN_PROC_BROWSER_TEST_F(LoginScreenPolicyTest, PolicyInputMethodsListEmpty) {
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

class LoginScreenGuestButtonPolicyTest : public LoginScreenPolicyTest {
 protected:
  void SetGuestModePolicy(bool enabled) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(enabled);
    RefreshDevicePolicyAndWaitForSettingChange(
        chromeos::kAccountsPrefAllowGuest);
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicyTest, NoUsers) {
  OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Default.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // When there are no users - should be the same as OOBE.
  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  SetGuestModePolicy(false);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  // Should not affect.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  SetGuestModePolicy(true);
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicyTest, HasUsers) {
  OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Default.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  ash::LoginScreen::Get()->GetModel()->SetUserList({{}});
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  // Should not affect.
  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  ash::LoginScreen::Get()->GetModel()->SetUserList({});
  EXPECT_TRUE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
}

class LoginScreenLocalePolicyTestBase : public LoginScreenPolicyTest {
 public:
  explicit LoginScreenLocalePolicyTestBase(const std::string& locale)
      : locale_(locale) {}
  LoginScreenLocalePolicyTestBase(const LoginScreenLocalePolicyTestBase&) =
      delete;
  LoginScreenLocalePolicyTestBase& operator=(
      const LoginScreenLocalePolicyTestBase&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    LoginScreenPolicyTest::SetUpInProcessBrowserTestFixture();
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_login_screen_locales()->add_login_screen_locales(locale_);
    RefreshDevicePolicy();
  }

 private:
  std::string locale_;
};

class LoginScreenLocalePolicyTest : public LoginScreenLocalePolicyTestBase {
 public:
  LoginScreenLocalePolicyTest() : LoginScreenLocalePolicyTestBase("fr-FR") {}
  LoginScreenLocalePolicyTest(const LoginScreenLocalePolicyTest&) = delete;
  LoginScreenLocalePolicyTest& operator=(const LoginScreenLocalePolicyTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(LoginScreenLocalePolicyTest,
                       LoginLocaleEnforcedByPolicy) {
  // Verifies that the default locale can be overridden with policy.
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());

  // TODO(https://crbug.com/1071010) Implement dynamic locale reload on policy
  // change.
}

class LoginScreenButtonsLocalePolicy : public LoginScreenLocalePolicyTestBase {
 public:
  LoginScreenButtonsLocalePolicy() : LoginScreenLocalePolicyTestBase("ar-EG") {
    device_state_.set_domain(kDomain);
  }
  LoginScreenButtonsLocalePolicy(const LoginScreenButtonsLocalePolicy&) =
      delete;
  LoginScreenButtonsLocalePolicy& operator=(
      const LoginScreenButtonsLocalePolicy&) = delete;
};

IN_PROC_BROWSER_TEST_F(LoginScreenButtonsLocalePolicy,
                       LoginShelfButtonsTextAndAlignment) {
  // Actual text on the button.
  std::u16string actual_text =
      ash::LoginScreenTestApi::GetShutDownButtonLabel();

  // Shut down text in the current locale.
  std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_SHUTDOWN_BUTTON);

  EXPECT_EQ(expected_text, actual_text);

  // Check if the shelf buttons are correctly aligned for RTL locale.
  // Target bounds are not updated in case of wrong alignment.
  gfx::Rect actual_bounds =
      ash::LoginScreenTestApi::GetShutDownButtonTargetBounds();
  gfx::Rect expected_bounds =
      ash::LoginScreenTestApi::GetShutDownButtonMirroredBounds();

  // RTL locales use the mirrored bounds, this is why we check the X coordinate.
  EXPECT_EQ(expected_bounds.x(), actual_bounds.x());
}

IN_PROC_BROWSER_TEST_F(LoginScreenButtonsLocalePolicy,
                       PRE_UnifiedTrayLabelsText) {
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginScreenButtonsLocalePolicy, UnifiedTrayLabelsText) {
  auto unified_tray_test_api = ash::SystemTrayTestApi::Create();

  // Check that tray is open.
  // The tray must be open before trying to retrieve its elements.
  EXPECT_TRUE(unified_tray_test_api->IsBubbleViewVisible(
      ash::VIEW_ID_TRAY_ENTERPRISE, true /* open_tray */));

  // Text on EnterpriseManagedView tooltip in current locale.
  std::u16string expected_text =
      ash::features::IsManagedDeviceUIRedesignEnabled()
          ? l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY,
                                       base::UTF8ToUTF16(kDomain))
          : l10n_util::GetStringFUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
                                       ui::GetChromeOSDeviceName(),
                                       base::UTF8ToUTF16(kDomain));
  EXPECT_EQ(expected_text, unified_tray_test_api->GetBubbleViewTooltip(
                               ash::VIEW_ID_TRAY_ENTERPRISE));
}

}  // namespace chromeos
