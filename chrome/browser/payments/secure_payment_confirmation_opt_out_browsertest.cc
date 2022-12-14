// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/secure_payment_confirmation_browsertest.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using SecurePaymentConfirmationOptOutTest = SecurePaymentConfirmationTest;

using Event2 = JourneyLogger::Event2;

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutOfferedAndTaken) {
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

  // Initiate SPC, with opt-out enabled.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  const bool show_opt_out = true;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // The opt-out link should be visible and clickable. Clicking it should cause
  // the PaymentRequest to be aborted.
  EXPECT_TRUE(test_controller()->ClickOptOut());
  EXPECT_EQ(
      "OptOutError: User opted out of the process.",
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));

  // Verify that opt out was recorded as both offered and chosen.
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kHadInitialFormOfPayment,
                         Event2::kOptOutOffered, Event2::kUserOptedOut,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutOfferedButNotTaken) {
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

  // Initiate SPC, with opt-out enabled.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  const bool show_opt_out = true;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was recorded as offered but not taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kHadInitialFormOfPayment,
                         Event2::kOptOutOffered,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutNotOffered) {
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

  // Initiate SPC, with opt-out disabled.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  const bool show_opt_out = false;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // The opt-out link should not be available.
  EXPECT_FALSE(test_controller()->ClickOptOut());

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was not recorded as offered or taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutDefaultsToNotOffered) {
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

  // Initiate SPC, without specifying a value for showOptOut.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");
  WaitForObservedEvent();

  // The default for showOptOut is false, so the link shouldn't be available.
  EXPECT_FALSE(test_controller()->ClickOptOut());

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was not recorded as offered or taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutNoMatchingCredsOfferedAndTaken) {
  // Don't install a credential, so that the 'No Matching Credentials' UI will
  // be shown.
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // Initiate SPC, with opt-out enabled.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  const bool show_opt_out = true;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // The no matching creds UI should render the opt-out link.
  EXPECT_TRUE(test_controller()->ClickOptOut());
  EXPECT_EQ(
      "OptOutError: User opted out of the process.",
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));

  // Verify that opt out was recorded as both offered and chosen.
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kOptOutOffered, Event2::kUserOptedOut,
                         Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutNoMatchingCredsOfferedButNotTaken) {
  // Don't install a credential, so that the 'No Matching Credentials' UI will
  // be shown.
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // Initiate SPC, with opt-out enabled.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  const bool show_opt_out = true;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was recorded as offered but not taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kOptOutOffered,
                         Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutNoMatchingCredsNotOffered) {
  // Don't install a credential, so that the 'No Matching Credentials' UI will
  // be shown.
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // Initiate SPC, with opt-out disabled.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  const bool show_opt_out = false;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // The no matching creds UI should not render the opt-out link.
  EXPECT_FALSE(test_controller()->ClickOptOut());

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was not recorded as offered or taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutTest,
                       ShowOptOutNoMatchingCredsDefaultsToNotOffered) {
  // Don't install a credential, so that the 'No Matching Credentials' UI will
  // be shown.
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // Initiate SPC, without specifying a value for showOptOut.
  ResetEventWaiterForSingleEvent(TestEvent::kErrorDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");
  WaitForObservedEvent();

  // The no matching creds UI should not render the opt-out link.
  EXPECT_FALSE(test_controller()->ClickOptOut());

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was not recorded as offered or taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kNoMatchingCredentials,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

class SecurePaymentConfirmationOptOutDisabledTest
    : public SecurePaymentConfirmationOptOutTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurePaymentConfirmationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kDisableBlinkFeatures,
                                    "SecurePaymentConfirmationOptOut");
  }
};

// The SPC opt-out experience should not be available if the Blink runtime flag
// is disabled.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationOptOutDisabledTest,
                       RequiresRuntimeFlag) {
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

  // Initiate SPC, with opt-out enabled.
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  const bool show_opt_out = true;
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "getSecurePaymentConfirmationStatus(undefined, undefined, $1)",
          show_opt_out));
  WaitForObservedEvent();

  // Because the runtime flag isn't set, showOptOut should still have been set
  // to false, and so there is no opt-out link to click.
  EXPECT_FALSE(test_controller()->ClickOptOut());

  // Close the dialog to trigger JourneyLogger metrics, and verify that opt out
  // was not recorded as offered or taken.
  test_controller()->CloseDialog();
  EXPECT_EQ(
      GetWebAuthnErrorMessage(),
      content::EvalJs(GetActiveWebContents(), "getOutstandingStatusPromise()"));
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kUserAborted, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation});
}

}  // namespace
}  // namespace payments
