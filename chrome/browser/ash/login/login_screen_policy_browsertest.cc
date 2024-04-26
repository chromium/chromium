// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

namespace em = ::enterprise_management;

const char kDomain[] = "domain.com";
const char16_t kDomain16[] = u"domain.com";

}  // namespace

class LoginScreenPolicyTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginScreenPolicyTest() = default;

  LoginScreenPolicyTest(const LoginScreenPolicyTest&) = delete;
  LoginScreenPolicyTest& operator=(const LoginScreenPolicyTest&) = delete;

  void RefreshDevicePolicyAndWaitForSettingChange(
      const char* cros_setting_name);

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
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

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethodIds().size());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_input_methods()->Clear();
  EXPECT_TRUE(proto.has_login_screen_input_methods());
  EXPECT_EQ(
      0, proto.login_screen_input_methods().login_screen_input_methods_size());
  RefreshDevicePolicyAndWaitForSettingChange(kDeviceLoginScreenInputMethods);

  ASSERT_EQ(0U, imm->GetActiveIMEState()->GetAllowedInputMethodIds().size());
}

class LoginScreenGuestButtonPolicyTest : public LoginScreenPolicyTest {
 protected:
  void SetGuestModePolicy(bool enabled) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(enabled);
    RefreshDevicePolicyAndWaitForSettingChange(kAccountsPrefAllowGuest);
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicyTest, NoUsers) {
  OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Default.
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  // When there are no users - should be the same as OOBE.
  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [false]);");
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  SetGuestModePolicy(false);
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  // Should not affect.
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  SetGuestModePolicy(true);
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

IN_PROC_BROWSER_TEST_F(LoginScreenGuestButtonPolicyTest, HasUsers) {
  OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Default.
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  LoginScreen::Get()->GetModel()->SetUserList({{}});
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  // Should not affect.
  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  LoginScreen::Get()->GetModel()->SetUserList({});
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [false]);");
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
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
}

class LoginScreenLocalePolicyWithVPDTest
    : public LoginScreenLocalePolicyTestBase {
 public:
  LoginScreenLocalePolicyWithVPDTest()
      : LoginScreenLocalePolicyTestBase("fr-FR") {
    // TODO(crbug.com/334954143) Fix the tests when turning on the reduce
    // accept-language feature.
    scoped_feature_list_.InitWithFeatures(
        {}, {network::features::kReduceAcceptLanguage});
    // Set a different locale in VPD.
    fake_statistics_provider_.SetMachineStatistic("initial_locale", "en-US");
  }
  LoginScreenLocalePolicyWithVPDTest(
      const LoginScreenLocalePolicyWithVPDTest&) = delete;
  LoginScreenLocalePolicyWithVPDTest& operator=(
      const LoginScreenLocalePolicyWithVPDTest&) = delete;

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that for the network service the device policy locale takes
// preference over the VPD locale.
IN_PROC_BROWSER_TEST_F(LoginScreenLocalePolicyWithVPDTest,
                       SigninAcceptLanguage) {
  auto* profile_network_context =
      ProfileNetworkContextServiceFactory::GetForContext(
          ash::BrowserContextHelper::Get()->GetSigninBrowserContext());
  ASSERT_NE(profile_network_context, nullptr);

  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  base::FilePath empty_relative_partition_path;
  profile_network_context->ConfigureNetworkContextParams(
      /*in_memory=*/true, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  ASSERT_EQ(network_context_params.accept_language, "fr-FR,fr;q=0.9");
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
  std::u16string actual_text = LoginScreenTestApi::GetShutDownButtonLabel();

  // Shut down text in the current locale.
  std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_SHUTDOWN_BUTTON);

  // Login shelf button's should be updated to the current locale.
  EXPECT_EQ(expected_text, actual_text);

  // Check if the shelf buttons are correctly aligned for RTL locale.
  // Target bounds are not updated in case of wrong alignment.
  gfx::Rect actual_bounds = LoginScreenTestApi::GetShutDownButtonTargetBounds();
  gfx::Rect expected_bounds =
      LoginScreenTestApi::GetShutDownButtonMirroredBounds();

  // RTL locales use the mirrored bounds, this is why we check the X coordinate.
  EXPECT_EQ(expected_bounds.x(), actual_bounds.x());
}

IN_PROC_BROWSER_TEST_F(LoginScreenButtonsLocalePolicy,
                       PRE_UnifiedTrayLabelsText) {
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginScreenButtonsLocalePolicy, UnifiedTrayLabelsText) {
  auto unified_tray_test_api = SystemTrayTestApi::Create();

  // Check that tray is open.
  // The tray must be open before trying to retrieve its elements.
  EXPECT_TRUE(unified_tray_test_api->IsBubbleViewVisible(
      VIEW_ID_QS_MANAGED_BUTTON, true /* open_tray */));

  // Text on EnterpriseManagedView tooltip in current locale.
  std::u16string expected_text =
      l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY, kDomain16);
  EXPECT_EQ(expected_text, unified_tray_test_api->GetBubbleViewTooltip(
                               VIEW_ID_QS_MANAGED_BUTTON));
}

}  // namespace ash
