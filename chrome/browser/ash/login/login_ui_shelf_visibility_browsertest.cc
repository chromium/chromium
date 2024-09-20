// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_auth_page_waiter.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {

namespace {

constexpr char kExistingUserEmail[] = "existing@gmail.com";
constexpr char kExistingUserGaiaId[] = "9876543210";

constexpr char kNewUserEmail[] = "new@gmail.com";
constexpr char kNewUserGaiaId[] = "0123456789";

class LoginUIShelfVisibilityTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginUIShelfVisibilityTest() = default;

  LoginUIShelfVisibilityTest(const LoginUIShelfVisibilityTest&) = delete;
  LoginUIShelfVisibilityTest& operator=(const LoginUIShelfVisibilityTest&) =
      delete;

  ~LoginUIShelfVisibilityTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  void StartOnboardingFlow() {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(kNewUserEmail, kNewUserGaiaId,
                                  FakeGaiaMixin::kEmptyUserServices);

    // Wait for the exiting of the sign-in screen which will be followed
    // by the showing of the first onboarding screen.
    OobeScreenExitWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();
  }

 private:
  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kExistingUserEmail, kExistingUserGaiaId)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};
  EmbeddedTestServerSetupMixin test_server_mixin_{&mixin_host_,
                                                  embedded_test_server()};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
};

class OsInstallVisibilityTest : public LoginUIShelfVisibilityTest {
 public:
  OsInstallVisibilityTest() = default;
  ~OsInstallVisibilityTest() override = default;
  OsInstallVisibilityTest(const OsInstallVisibilityTest&) = delete;
  void operator=(const OsInstallVisibilityTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginUIShelfVisibilityTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowOsInstall);
  }
};

}  // namespace

// Verifies that shelf buttons are shown by default on login screen.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, DefaultVisibility) {
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsAddUserButtonShown());
}

// Verifies that guest button, add user button and enterprise enrollment button
// are hidden when Gaia dialog is shown.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, GaiaDialogOpen) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
}

// Verifies that guest button and add user button are hidden on post-login
// screens, after a user session is started.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, PostLoginScreen) {
  StartOnboardingFlow();

  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
}

// Verifies that OS install button is shown by default on login screen.
IN_PROC_BROWSER_TEST_F(OsInstallVisibilityTest, DefaultVisibility) {
  EXPECT_TRUE(LoginScreenTestApi::IsOsInstallButtonShown());
}

// Verifies that OS install button is hidden when Gaia dialog is shown.
IN_PROC_BROWSER_TEST_F(OsInstallVisibilityTest, GaiaDialogOpen) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsOsInstallButtonShown());
}

// Verifies that guest button, add user button, enterprise enrollment button and
// OS install button are hidden when os-install dialog is shown.
IN_PROC_BROWSER_TEST_F(OsInstallVisibilityTest, OsInstallDialogOpen) {
  EXPECT_TRUE(LoginScreenTestApi::ClickOsInstallButton());
  OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsOsInstallButtonShown());
}

// Verifies that OS install is hidden on post-login screens.
IN_PROC_BROWSER_TEST_F(OsInstallVisibilityTest, PostLoginScreen) {
  StartOnboardingFlow();
  EXPECT_FALSE(LoginScreenTestApi::IsOsInstallButtonShown());
}

class SamlInterstitialTest : public LoginManagerTest {
 public:
  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();

    device_policy_update->policy_payload()
        ->mutable_login_authentication_behavior()
        ->set_login_authentication_behavior(
            enterprise_management::
                LoginAuthenticationBehaviorProto_LoginBehavior_SAML_INTERSTITIAL);

    KioskAppsMixin::AppendKioskAccount(device_policy_update->policy_payload());

    device_policy_update.reset();

    device_state_.RequestDeviceLocalAccountPolicyUpdate(
        KioskAppsMixin::kEnterpriseKioskAccountId);
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  KioskAppsMixin kiosk_apps_{&mixin_host_, embedded_test_server()};
};

// Verifies that Apps and Guest buttons are visible when login flow starts from
// the SAML interstitial step.
IN_PROC_BROWSER_TEST_F(SamlInterstitialTest, AppsGuestButton) {
  KioskAppsMixin::WaitForAppsButton();
  EXPECT_TRUE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

class KioskSkuVisibilityTest : public LoginUIShelfVisibilityTest {
 public:
  KioskSkuVisibilityTest() {
    device_state_.set_skip_initial_policy_setup(true);
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableKioskLoginScreen);
  }
  ~KioskSkuVisibilityTest() override = default;
  KioskSkuVisibilityTest(const KioskSkuVisibilityTest&) = delete;
  void operator=(const KioskSkuVisibilityTest&) = delete;

 protected:
  policy::DevicePolicyCrosTestHelper* policy_helper() {
    return &policy_helper_;
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that shelf buttons of Guest mode and Add user are shown, and kiosk
// instruction bubble is hidden without kiosk SKU.
IN_PROC_BROWSER_TEST_F(KioskSkuVisibilityTest, WithoutKioskSku) {
  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
}

// Verifies that shelf buttons of Guest mode and Add user are hidden, and kiosk
// instruction bubble is hidden too without kiosk apps.
IN_PROC_BROWSER_TEST_F(KioskSkuVisibilityTest, WithoutApps) {
  policy_helper()->device_policy()->policy_data().set_license_sku(
      policy::kKioskSkuName);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
}

// Verifies that shelf buttons of Guest mode and Add user are hidden, and kiosk
// instruction bubble is shown with kiosk apps.
IN_PROC_BROWSER_TEST_F(KioskSkuVisibilityTest, WithApps) {
  policy_helper()->device_policy()->policy_data().set_license_sku(
      policy::kKioskSkuName);
  KioskAppsMixin::AppendKioskAccount(
      &policy_helper()->device_policy()->payload());
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
}

}  // namespace ash
