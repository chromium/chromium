// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/security_token_saml_test.h"

#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/test_client_cert_saml_idp_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/fake_gaia.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

SecurityTokenSamlTest::SecurityTokenSamlTest()
    : saml_idp_mixin_(&mixin_host_,
                      &gaia_mixin_,
                      /*client_cert_authorities=*/{GetClientCertCaName()}) {
  // Allow the forced installation of extensions in the background.
  needs_background_networking_ = true;

  SetClientCertAutoSelectPolicy();
  ConfigureFakeGaia();
}

SecurityTokenSamlTest::~SecurityTokenSamlTest() {}

void SecurityTokenSamlTest::SetUpCommandLine(base::CommandLine* command_line) {
  OobeBaseTest::SetUpCommandLine(command_line);
  // Skip OOBE post-login screens (like the sync consent screen in branded
  // builders) to make the test simpler by directly proceeding to user session
  // after the sign-in.
  command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  // Avoid aborting the user sign-in due to the user policy requests not being
  // faked in the test.
  command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
}

void SecurityTokenSamlTest::SetUpOnMainThread() {
  // Without this tests are flaky. TODO(b/333450354): Determine why and fix.
  SetDelayNetworkCallsForTesting(true);

  OobeBaseTest::SetUpOnMainThread();
  // Make sure the Gaia login frame is loaded before any other run loop is
  // executed, in order to avoid flakiness due to spurious error screens.
  WaitForSigninScreen();

  PrepareCertProviderExtension();
  gaia_mixin_.fake_gaia()->RegisterSamlUser(
      saml_test_users::kFirstUserCorpExampleComEmail,
      saml_idp_mixin_.GetSamlPageUrl());
  StartObservingLoginUiMessages();
}

void SecurityTokenSamlTest::SetUpInProcessBrowserTestFixture() {
  OobeBaseTest::SetUpInProcessBrowserTestFixture();
  // Init the user policy provider.
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
}

void SecurityTokenSamlTest::StartSignIn() {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                /*password=*/std::string(),
                                /*services=*/"[]");
}

// Waits until the security token PIN dialog appears on the login screen.
void SecurityTokenSamlTest::WaitForPinDialog() {
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
void SecurityTokenSamlTest::InputPinByClickingKeypad(const std::string& pin) {
  for (char pin_character : pin) {
    const std::string digit_button_id =
        std::string("digitButton") + pin_character;
    test::OobeJS().ClickOnPath(
        {"gaia-signin", "pinDialog", "pinKeyboard", digit_button_id});
  }
}

void SecurityTokenSamlTest::ClickPinDialogSubmit() {
  test::OobeJS().ClickOnPath({"gaia-signin", "pinDialog", "submit"});
}

std::string SecurityTokenSamlTest::GetCorrectPin() const {
  return kCorrectPin;
}

// Configures and installs the test certificate provider extension.
void SecurityTokenSamlTest::PrepareCertProviderExtension() {
  extension_force_install_mixin_.InitWithMockPolicyProvider(
      GetOriginalSigninProfile(), &policy_provider_);
  ASSERT_NO_FATAL_FAILURE(
      test_certificate_provider_extension_mixin_.ForceInstall(
          GetOriginalSigninProfile()));
  test_certificate_provider_extension_mixin_.extension()->set_require_pin(
      kCorrectPin);
}

// Sets up the client certificate to be automatically selected for the SAML
// page (by default a certificate selector needs to be shown).
void SecurityTokenSamlTest::SetClientCertAutoSelectPolicy() {
  const std::string policy_item_value = kClientCertAutoSelectPolicyValue;
  policy::PolicyMap policy_map =
      policy_provider_.policies()
          .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                       /*component_id=*/std::string()))
          .Clone();
  policy::PolicyMap::Entry* const existing_entry =
      policy_map.GetMutable(policy::key::kAutoSelectCertificateForUrls);
  if (existing_entry) {
    // Append to the existing policy.
    existing_entry->value(base::Value::Type::LIST)
        ->GetList()
        .Append(policy_item_value);
  } else {
    // Set the new policy value.
    base::Value::List policy_value;
    policy_value.Append(policy_item_value);
    policy_map.Set(policy::key::kAutoSelectCertificateForUrls,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(policy_value)),
                   /*external_data_fetcher=*/nullptr);
  }
  policy_provider_.UpdateChromePolicy(policy_map);
}

void SecurityTokenSamlTest::ConfigureFakeGaia() {
  // FakeGaia uses the fake merge session parameters for preparing the result
  // of the SAML sign-in.
  gaia_mixin_.set_initialize_configuration(false);
  gaia_mixin_.fake_gaia()->SetConfigurationHelper(
      saml_test_users::kFirstUserCorpExampleComEmail,
      /*auth_sid_cookie=*/std::string(),
      /*auth_lsid_cookie=*/std::string());
}

// Subscribes for the notifications from the Login Screen UI,
void SecurityTokenSamlTest::StartObservingLoginUiMessages() {
  GetLoginUI()->RegisterMessageCallback(
      "securityTokenPinDialogShownForTest",
      base::BindRepeating(&SecurityTokenSamlTest::OnPinDialogShownMessage,
                          weak_factory_.GetWeakPtr()));
}

// Called when the Login Screen UI notifies that the PIN dialog is shown.
void SecurityTokenSamlTest::OnPinDialogShownMessage(const base::Value::List&) {
  ++pin_dialog_shown_count_;
  if (pin_dialog_shown_run_loop_)
    pin_dialog_shown_run_loop_->Quit();
}

}  // namespace ash
