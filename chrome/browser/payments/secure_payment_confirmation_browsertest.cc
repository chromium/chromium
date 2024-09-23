// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/secure_payment_confirmation_browsertest.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
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

void SecurePaymentConfirmationTest::ExpectEvent2Histogram(
    std::set<JourneyLogger::Event2> events,
    int count) {
  std::vector<base::Bucket> buckets =
      histogram_tester_.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());

  int64_t expected_events = 0;
  for (const JourneyLogger::Event2& event : events) {
    expected_events |= static_cast<int>(event);
  }
  EXPECT_EQ(buckets[0].min, expected_events);
  EXPECT_EQ(count, buckets[0].count);
}

// static
std::string SecurePaymentConfirmationTest::GetWebAuthnErrorMessage() {
  return "NotAllowedError: The operation either timed out or was not allowed. "
         "See: https://www.w3.org/TR/webauthn-2/"
         "#sctn-privacy-considerations-client.";
}

namespace {

using Event2 = payments::JourneyLogger::Event2;

std::string GetIconDownloadErrorMessage() {
  return "NotSupportedError: The payment method "
         "\"secure-payment-confirmation\" is not supported. "
         "The \"instrument.icon\" either could not be downloaded or decoded.";
}

// Tests that show() will display the Transaction UX, if there is a matching
// credential.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, Show_TransactionUX) {
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

  // As these tests mock out the authenticator, we cannot continue on the
  // Transaction UX and instead must cancel out. For tests that test the
  // continue flow, see secure_payment_confirmation_authenticator_browsertest.cc
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

// Tests that calling show() on a platform without an authenticator will trigger
// the No Matching Credentials UX.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, Show_NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  close_dialog_on_error_ = true;
  EXPECT_EQ(GetWebAuthnErrorMessage(),
            content::EvalJs(GetActiveWebContents(),
                            "getSecurePaymentConfirmationStatus()"));

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

// Tests that calling show() with no matching credentials will trigger the No
// Matching Credentials UX.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       Show_NoMatchingCredential) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  close_dialog_on_error_ = true;
  EXPECT_EQ(GetWebAuthnErrorMessage(),
            content::EvalJs(GetActiveWebContents(),
                            "getSecurePaymentConfirmationStatus()"));

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

// Tests that a credential with the correct credential ID but wrong RP ID will
// not match.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       Show_WrongCredentialRpId) {
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
  // so the No Matching Credentials dialog will be displayed.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");

  WaitForObservedEvent();
  EXPECT_TRUE(database_write_responded_);
  EXPECT_TRUE(test_controller()->app_descriptions().empty());

  test_controller()->CloseDialog();

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
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

  // Both scenarios should log the same histogram.
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kCouldNotShow,
                         Event2::kRequestMethodSecurePaymentConfirmation},
                        /*count=*/2);
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

// canMakePayment() and hasEnrolledInstrument() should return true on platforms
// with a compatible authenticator regardless of the value of the
// "prefs.can_make_payment_enabled" pref.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       CanMakePayment_CanMakePaymentEnabledPref) {
  test_controller()->SetHasAuthenticator(true);
  test_controller()->SetCanMakePaymentEnabledPref(false);

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

#if !BUILDFLAG(IS_ANDROID)
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
      "NotSupportedError: The payment method \"secure-payment-confirmation\" "
      "is not supported.",
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
      "NotSupportedError: The payment method \"secure-payment-confirmation\" "
      "is not supported.",
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
#endif  // !BUILDFLAG(IS_ANDROID)

// Test that Secure Payment Confirmation allows one call to show() without a
// user activation.
class SecurePaymentConfirmationActivationlessShowTest
    : public SecurePaymentConfirmationTest {
 public:
  void ExpectEvent2(JourneyLogger::Event2 event, bool expected) {
    std::vector<base::Bucket> buckets =
        histogram_tester_.GetAllSamples("PaymentRequest.Events2");
    EXPECT_EQ(expected, (buckets[0].min & static_cast<int>(event)) != 0);
  }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationActivationlessShowTest,
                       ActivationlessShow) {
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

  // The first call to show() without a user gesture succeeds.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsyncWithoutUserGesture(GetActiveWebContents(),
                                       "getSecurePaymentConfirmationStatus()");
  WaitForObservedEvent();
  test_controller()->CloseDialog();

  // A second call to show() without a user gesture gives an error.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(),
                      "getSecurePaymentConfirmationStatus()",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString(),
      ::testing::HasSubstr(errors::kCannotShowWithoutUserActivation));
  ExpectEvent2(Event2::kActivationlessShow, true);
}

// TODO(crbug.com/40266119): This test does not work on Android as it is
// difficult to wait for the bottom sheet to finish showing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShowAfterActivationlessShow DISABLED_ShowAfterActivationlessShow
#else
#define MAYBE_ShowAfterActivationlessShow ShowAfterActivationlessShow
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationActivationlessShowTest,
                       MAYBE_ShowAfterActivationlessShow) {
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

  // The first call to show() without a user gesture succeeds.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsyncWithoutUserGesture(GetActiveWebContents(),
                                       "getSecurePaymentConfirmationStatus()");
  WaitForObservedEvent();
  test_controller()->CloseDialog();

  // A following call to show() with a user gesture succeeds.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");
  WaitForObservedEvent();
  test_controller()->CloseDialog();
}

}  // namespace
}  // namespace payments
