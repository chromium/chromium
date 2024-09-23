// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_SECURITY_TOKEN_SAML_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_SECURITY_TOKEN_SAML_TEST_H_

#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/test_client_cert_saml_idp_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension_mixin.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// Allows to test the challenge-response type of SAML login (e.g., the smart
// card based user login).
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
//
// Example usage:
//
// IN_PROC_BROWSER_TEST_F(SecurityTokenSamlTest, TestName) {
//   StartSignIn();
//   WaitForPinDialog();
//
//   InputPinByClickingKeypad(GetCorrectPin());
//   ClickPinDialogSubmit();
//   test::WaitForPrimaryUserSessionStart();
// }

// Test parameter controls if new AuthSession-based cryptohome API
// should be used.
class SecurityTokenSamlTest : public OobeBaseTest {
 protected:
  SecurityTokenSamlTest();
  SecurityTokenSamlTest(const SecurityTokenSamlTest&) = delete;
  SecurityTokenSamlTest& operator=(const SecurityTokenSamlTest&) = delete;
  ~SecurityTokenSamlTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  void StartSignIn();

  // Waits until the security token PIN dialog appears on the login screen.
  void WaitForPinDialog();

  // Enters the security token PIN by simulating click events on the on-screen
  // keypad.
  void InputPinByClickingKeypad(const std::string& pin);

  // Simulates a click on the PIN dialog submit button.
  void ClickPinDialogSubmit();

  // Returns the Pin for `certificate_provider_extension_`.
  std::string GetCorrectPin() const;

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

  int pin_dialog_shown_count() const { return pin_dialog_shown_count_; }

 private:
  // Configures and installs the test certificate provider extension.
  void PrepareCertProviderExtension();

  // Sets up the client certificate to be automatically selected for the SAML
  // page (by default a certificate selector needs to be shown).
  void SetClientCertAutoSelectPolicy();

  void ConfigureFakeGaia();

  // Subscribes for the notifications from the Login Screen UI,
  void StartObservingLoginUiMessages();

  // Called when the Login Screen UI notifies that the PIN dialog is shown.
  void OnPinDialogShownMessage(const base::Value::List&);

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{TestCertificateProviderExtension::extension_id()};

  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  TestClientCertSamlIdpMixin saml_idp_mixin_;
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{
          &mixin_host_, &extension_force_install_mixin_};
  int pin_dialog_shown_count_ = 0;
  raw_ptr<base::RunLoop> pin_dialog_shown_run_loop_ = nullptr;
  base::WeakPtrFactory<SecurityTokenSamlTest> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_SECURITY_TOKEN_SAML_TEST_H_
