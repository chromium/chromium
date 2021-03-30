// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <iterator>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/ash/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/ash/login/saml/test_client_cert_saml_idp_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/https_forwarder.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/simple_feature.h"
#include "google_apis/gaia/fake_gaia.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Pattern for the DeviceLoginScreenAutoSelectCertificateForUrls admin policy
// that automatically selects the certificate exposed by the test certificate
// provider extension.
constexpr char kClientCertAutoSelectPolicyValue[] =
    R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})";
// The DER-encoded "CN=B CA" DistinguishedName value, which represents the
// issuer of the certificate exposed by the test certificate provider extension.
constexpr uint8_t kClientCertCaName[] = {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b,
                                         0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                         0x04, 0x42, 0x20, 0x43, 0x41};

// The PIN code that the test certificate provider extension is configured to
// expect.
constexpr char kCorrectPin[] = "17093";

std::string GetClientCertCaName() {
  return std::string(std::begin(kClientCertCaName),
                     std::end(kClientCertCaName));
}

std::string GetActiveUserEmail() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user)
    return std::string();
  return user->GetAccountId().GetUserEmail();
}

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

// Tests the challenge-response type of SAML login (e.g., the smart card based
// user login).
//
// The rough sequence of steps in the correct scenario:
// 1. the user e-mail is entered into the Gaia form;
// 2. the browser is redirected to the (first) SAML page;
// 3. the browser is redirected to the second SAML page, which requests a client
//    certificate during the TLS handshake;
// 4. the test certificate provider extension returns the client certificate;
// 5. the TLS handshake with the SAML server continues, and the server makes a
//    challenge request;
// 6. the test certificate provider extension receives the challenge and
//    requests the PIN, for which the PIN dialog is shown on top of the SAML
//    page frame;
// 7. the PIN is entered;
// 8. the test certificate provider extension receives the PIN and generates the
//    signature of the challenge, which is forwarded to the SAML server;
// 9. the TLS handshake with the SAML server completes, and the SAML page
//    redirects back to Gaia to signal about the successful authentication;
// 10. the user session starts.
class SecurityTokenSamlTest : public OobeBaseTest {
 protected:
  SecurityTokenSamlTest() {
    // Allow the forced installation of extensions in the background.
    needs_background_networking_ = true;

    SetClientCertAutoSelectPolicy();
    ConfigureFakeGaia();
  }

  SecurityTokenSamlTest(const SecurityTokenSamlTest&) = delete;
  SecurityTokenSamlTest& operator=(const SecurityTokenSamlTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Skip OOBE post-login screens (like the sync consent screen in branded
    // builders) to make the test simpler by directly proceeding to user session
    // after the sign-in.
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    // Avoid aborting the user sign-in due to the user policy requests not being
    // faked in the test.
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    PrepareCertProviderExtension();
    gaia_mixin_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFirstUserCorpExampleComEmail,
        saml_idp_mixin_.GetSamlPageUrl());
    StartObservingLoginUiMessages();
  }

  void TearDownOnMainThread() override {
    certificate_provider_extension_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  int pin_dialog_shown_count() const { return pin_dialog_shown_count_; }

  void StartSignIn() {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(
            saml_test_users::kFirstUserCorpExampleComEmail,
            /*password=*/std::string(),
            /*services=*/"[]");
  }

  // Waits until the security token PIN dialog appears on the login screen.
  void WaitForPinDialog() {
    if (pin_dialog_shown_count_)
      return;
    base::RunLoop run_loop;
    pin_dialog_shown_run_loop_ = &run_loop;
    // Quit() will be called from OnPinDialogShownMessage().
    run_loop.Run();
    pin_dialog_shown_run_loop_ = nullptr;
  }

  // Enters the security token PIN by simulating click events on the on-screen
  // keypad.
  void InputPinByClickingKeypad(const std::string& pin) {
    for (char pin_character : pin) {
      const std::string digit_button_id =
          std::string("digitButton") + pin_character;
      test::OobeJS().ClickOnPath(
          {"gaia-signin", "pinDialog", "pinKeyboard", digit_button_id});
    }
  }

 private:
  // Configures and installs the test certificate provider extension.
  void PrepareCertProviderExtension() {
    certificate_provider_extension_ =
        std::make_unique<TestCertificateProviderExtension>(
            GetOriginalSigninProfile());
    certificate_provider_extension_->set_require_pin(kCorrectPin);
    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        TestCertificateProviderExtension::GetExtensionSourcePath(),
        TestCertificateProviderExtension::GetExtensionPemPath(),
        ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad));
  }

  // Sets up the client certificate to be automatically selected for the SAML
  // page (by default a certificate selector needs to be shown).
  void SetClientCertAutoSelectPolicy() {
    device_state_mixin_.RequestDevicePolicyUpdate()
        ->policy_payload()
        ->mutable_device_login_screen_auto_select_certificate_for_urls()
        ->add_login_screen_auto_select_certificate_rules(
            kClientCertAutoSelectPolicyValue);
  }

  void ConfigureFakeGaia() {
    // FakeGaia uses the fake merge session parameters for preparing the result
    // of the SAML sign-in.
    gaia_mixin_.set_initialize_fake_merge_session(false);
    gaia_mixin_.fake_gaia()->SetFakeMergeSessionParams(
        saml_test_users::kFirstUserCorpExampleComEmail,
        /*auth_sid_cookie=*/std::string(),
        /*auth_lsid_cookie=*/std::string());
  }

  // Subscribes for the notifications from the Login Screen UI,
  void StartObservingLoginUiMessages() {
    GetLoginUI()->RegisterMessageCallback(
        "securityTokenPinDialogShownForTest",
        base::BindRepeating(&SecurityTokenSamlTest::OnPinDialogShownMessage,
                            weak_factory_.GetWeakPtr()));
  }

  // Called when the Login Screen UI notifies that the PIN dialog is shown.
  void OnPinDialogShownMessage(const base::ListValue*) {
    ++pin_dialog_shown_count_;
    if (pin_dialog_shown_run_loop_)
      pin_dialog_shown_run_loop_->Quit();
  }

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{TestCertificateProviderExtension::extension_id()};

  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  FakeGaiaMixin gaia_mixin_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  TestClientCertSamlIdpMixin saml_idp_mixin_{
      &mixin_host_, &gaia_mixin_,
      /*client_cert_authorities=*/{GetClientCertCaName()}};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  std::unique_ptr<TestCertificateProviderExtension>
      certificate_provider_extension_;
  int pin_dialog_shown_count_ = 0;
  base::RunLoop* pin_dialog_shown_run_loop_ = nullptr;
  base::WeakPtrFactory<SecurityTokenSamlTest> weak_factory_{this};
};

#define NEVER_ENABLED_Basic DISABLED_Basic

// Tests the successful login scenario with the correct PIN.
// TODO(crbug.com/1033936): Fix the flakiness and enable it.
IN_PROC_BROWSER_TEST_F(SecurityTokenSamlTest, NEVER_ENABLED_Basic) {
  WaitForSigninScreen();
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "pinDialog"});

  StartSignIn();
  WaitForPinDialog();
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "pinDialog"});

  InputPinByClickingKeypad(kCorrectPin);
  test::OobeJS().ClickOnPath({"gaia-signin", "pinDialog", "submit"});
  test::WaitForPrimaryUserSessionStart();
  EXPECT_EQ(saml_test_users::kFirstUserCorpExampleComEmail,
            GetActiveUserEmail());
  EXPECT_EQ(1, pin_dialog_shown_count());
}

}  // namespace chromeos
