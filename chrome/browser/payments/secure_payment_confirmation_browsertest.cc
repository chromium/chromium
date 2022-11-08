// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/secure_payment_confirmation_browsertest.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"

namespace payments {

void SecurePaymentConfirmationTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
}

void SecurePaymentConfirmationTest::OnAppListReady() {
  PaymentRequestPlatformBrowserTestBase::OnAppListReady();
  if (confirm_payment_)
    ASSERT_TRUE(test_controller()->ConfirmPayment());
}

void SecurePaymentConfirmationTest::OnErrorDisplayed() {
  PaymentRequestPlatformBrowserTestBase::OnErrorDisplayed();
  if (close_dialog_on_error_)
    ASSERT_TRUE(test_controller()->CloseDialog());
}

void SecurePaymentConfirmationTest::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  ASSERT_NE(nullptr, result);
  ASSERT_EQ(BOOL_RESULT, result->GetType());
  EXPECT_TRUE(static_cast<WDResult<bool>*>(result.get())->GetValue());
  database_write_responded_ = true;
}

namespace {

std::string GetIconDownloadErrorMessage() {
  return "The payment method \"secure-payment-confirmation\" is not supported. "
         "The \"instrument.icon\" either could not be downloaded or decoded.";
}

std::string GetWebAuthnErrorMessage() {
  return "The operation either timed out or was not allowed. See: "
         "https://www.w3.org/TR/webauthn-2/"
         "#sctn-privacy-considerations-client.";
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  close_dialog_on_error_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(GetWebAuthnErrorMessage(),
            content::EvalJs(GetActiveWebContents(),
                            "getSecurePaymentConfirmationStatus()"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, NoInstrumentInStorage) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  close_dialog_on_error_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(GetWebAuthnErrorMessage(),
            content::EvalJs(GetActiveWebContents(),
                            "getSecurePaymentConfirmationStatus()"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       CheckInstrumentInStorageAfterCanMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  close_dialog_on_error_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(
          GetActiveWebContents(),
          base::StringPrintf(
              "getSecurePaymentConfirmationStatusAfterCanMakePayment()")));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, WrongCredentialRpId) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  std::vector<uint8_t> credential_id = {'c', 'r', 'e', 'd'};
  std::vector<uint8_t> user_id = {'u', 's', 'e', 'r'};
  webdata_services::WebDataServiceWrapperFactory::
      GetPaymentManifestWebDataServiceForBrowserContext(
          GetActiveWebContents()->GetBrowserContext(),
          ServiceAccessType::EXPLICIT_ACCESS)
          ->AddSecurePaymentConfirmationCredential(
              std::make_unique<SecurePaymentConfirmationCredential>(
                  std::move(credential_id), "relying-party.example",
                  std::move(user_id)),
              /*consumer=*/this);

  // getSecurePaymentConfirmationStatus creates a SPC credential with RP ID
  // a.com, which doesn't match the stored credential's relying-party.example,
  // so an error dialog will be displayed.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");

  WaitForObservedEvent();
  EXPECT_TRUE(database_write_responded_);
  EXPECT_TRUE(test_controller()->app_descriptions().empty());
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, PaymentSheetShowsApp) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  std::vector<uint8_t> credential_id = {'c', 'r', 'e', 'd'};
  std::vector<uint8_t> user_id = {'u', 's', 'e', 'r'};
  webdata_services::WebDataServiceWrapperFactory::
      GetPaymentManifestWebDataServiceForBrowserContext(
          GetActiveWebContents()->GetBrowserContext(),
          ServiceAccessType::EXPLICIT_ACCESS)
          ->AddSecurePaymentConfirmationCredential(
              std::make_unique<SecurePaymentConfirmationCredential>(
                  std::move(credential_id), "a.com", std::move(user_id)),
              /*consumer=*/this);
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");

  WaitForObservedEvent();
  EXPECT_TRUE(database_write_responded_);
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("display_name_for_instrument",
            test_controller()->app_descriptions().front().label);
}

// Tests that a failed icon download immediately rejects the show() promise,
// without any browser UI being shown.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, IconDownloadFailure) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // We test both with and without a matching credential, so add a credential
  // for the former case.
  std::vector<uint8_t> credential_id = {'c', 'r', 'e', 'd'};
  std::vector<uint8_t> user_id = {'u', 's', 'e', 'r'};
  webdata_services::WebDataServiceWrapperFactory::
      GetPaymentManifestWebDataServiceForBrowserContext(
          GetActiveWebContents()->GetBrowserContext(),
          ServiceAccessType::EXPLICIT_ACCESS)
          ->AddSecurePaymentConfirmationCredential(
              std::make_unique<SecurePaymentConfirmationCredential>(
                  std::move(credential_id), "relying-party.example",
                  std::move(user_id)),
              /*consumer=*/this);

  // canMakePayment does not check for a valid icon, so should return true.
  EXPECT_EQ("true",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePayment(window."
                            "location.origin + '/non-existant-icon.png')"));

  // The show() promise, however, should reject without showing any UX -
  // whether or not a valid credential is passed.
  std::string credBase64 = "Y3JlZA==";
  EXPECT_EQ(
      GetIconDownloadErrorMessage(),
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace(
                          "getSecurePaymentConfirmationStatus($1, "
                          "window.location.origin + '/non-existant-icon.png')",
                          credBase64)));

  std::string invalidCred = "ZGVyYw==";
  EXPECT_EQ(
      GetIconDownloadErrorMessage(),
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace(
                          "getSecurePaymentConfirmationStatus($1, "
                          "window.location.origin + '/non-existant-icon.png')",
                          invalidCred)));
}

class SecurePaymentConfirmationDisableDebugTest
    : public SecurePaymentConfirmationTest {
 public:
  SecurePaymentConfirmationDisableDebugTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{::features::kSecurePaymentConfirmation},
        /*disabled_features=*/{::features::kSecurePaymentConfirmationDebug});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// canMakePayment() and hasEnrolledInstrument() should return false on
// platforms without a compatible authenticator.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisableDebugTest,
                       CanMakePayment_NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ("false",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePayment()"));
  EXPECT_EQ("false", content::EvalJs(
                         GetActiveWebContents(),
                         "securePaymentConfirmationHasEnrolledInstrument()"));
}

// canMakePayment() and hasEnrolledInstrument() should return true on
// platforms with a compatible authenticator regardless of the presence of
// payment credentials.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       CanMakePayment_HasAuthenticator) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ("true",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePayment()"));
  EXPECT_EQ("true",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePaymentTwice()"));
  EXPECT_EQ("true", content::EvalJs(
                        GetActiveWebContents(),
                        "securePaymentConfirmationHasEnrolledInstrument()"));
}

// Intentionally do not enable the "SecurePaymentConfirmation" Blink runtime
// feature or the browser-side Finch flag.
class SecurePaymentConfirmationDisabledTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  SecurePaymentConfirmationDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{::features::kSecurePaymentConfirmation});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       PaymentMethodNotSupported) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      "getSecurePaymentConfirmationStatus()"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       CannotMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ("false",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePayment()"));
  EXPECT_EQ("false", content::EvalJs(
                         GetActiveWebContents(),
                         "securePaymentConfirmationHasEnrolledInstrument()"));
}

// Test that the feature can be disabled by the browser-side Finch flag, even if
// the Blink runtime feature is enabled.
class SecurePaymentConfirmationDisabledByFinchTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  SecurePaymentConfirmationDisabledByFinchTest() {
    // The feature should get disabled by the feature state despite
    // experimental web platform features being enabled.
    feature_list_.InitAndDisableFeature(::features::kSecurePaymentConfirmation);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledByFinchTest,
                       PaymentMethodNotSupported) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      "getSecurePaymentConfirmationStatus()"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledByFinchTest,
                       CannotMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ("false",
            content::EvalJs(GetActiveWebContents(),
                            "securePaymentConfirmationCanMakePayment()"));
  EXPECT_EQ("false", content::EvalJs(
                         GetActiveWebContents(),
                         "securePaymentConfirmationHasEnrolledInstrument()"));
}

}  // namespace
}  // namespace payments
