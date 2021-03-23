// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_credential_enrollment_controller.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

std::vector<uint8_t> GetEncodedIcon(const std::string& icon_file_name) {
  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
  std::string icon_as_string;
  base::FilePath icon_file_path =
      base_path.AppendASCII("components/test/data/payments")
          .AppendASCII(icon_file_name);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::PathExists(icon_file_path));
    CHECK(base::ReadFileToString(icon_file_path, &icon_as_string));
  }

  return std::vector<uint8_t>(icon_as_string.begin(), icon_as_string.end());
}

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase,
      public WebDataServiceConsumer {
 public:
  SecurePaymentConfirmationTest() {
    // Enable the browser-side feature flag as it's disabled by default on
    // non-origin trial platforms.
    feature_list_.InitAndEnableFeature(features::kSecurePaymentConfirmation);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override {
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(BOOL_RESULT, result->GetType());
    EXPECT_TRUE(static_cast<WDResult<bool>*>(result.get())->GetValue());
    database_write_responded_ = true;
  }

  void OnAppListReady() override {
    PaymentRequestPlatformBrowserTestBase::OnAppListReady();
    if (confirm_payment_)
      ASSERT_TRUE(test_controller()->ConfirmPayment());
  }

  bool database_write_responded_ = false;
  bool confirm_payment_ = false;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      "getSecurePaymentConfirmationStatus()"));
}

#if defined(OS_ANDROID)
// TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for Android,
// so secure payment confirmation can be tested on Android as well.
#define MAYBE_NoInstrumentInStorage DISABLED_NoInstrumentInStorage
#define MAYBE_CheckInstrumentInStorageAfterCanMakePayment \
  DISABLED_CheckInstrumentInStorageAfterCanMakePayment
#define MAYBE_PaymentSheetShowsApp DISABLED_PaymentSheetShowsApp
#define MAYBE_CanMakePayment_HasAuthenticator \
  DISABLED_CanMakePayment_HasAuthenticator
#else
#define MAYBE_NoInstrumentInStorage NoInstrumentInStorage
#define MAYBE_CheckInstrumentInStorageAfterCanMakePayment \
  CheckInstrumentInStorageAfterCanMakePayment
#define MAYBE_PaymentSheetShowsApp PaymentSheetShowsApp
#define MAYBE_CanMakePayment_HasAuthenticator CanMakePayment_HasAuthenticator
#endif  // OS_ANDROID

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_NoInstrumentInStorage) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      "getSecurePaymentConfirmationStatus()"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_CheckInstrumentInStorageAfterCanMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(
          GetActiveWebContents(),
          base::StringPrintf(
              "getSecurePaymentConfirmationStatusAfterCanMakePayment()")));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_PaymentSheetShowsApp) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  std::vector<uint8_t> credential_id = {'c', 'r', 'e', 'd'};
  std::vector<uint8_t> icon = GetEncodedIcon("icon.png");
  WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()),
      ServiceAccessType::EXPLICIT_ACCESS)
      ->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              std::move(credential_id), "relying-party.example", u"Stub label",
              std::move(icon)),
          /*consumer=*/this);
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);
  ExecuteScriptAsync(GetActiveWebContents(),
                     "getSecurePaymentConfirmationStatus()");

  WaitForObservedEvent();
  EXPECT_TRUE(database_write_responded_);
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("Stub label", test_controller()->app_descriptions().front().label);
}

// canMakePayment() and hasEnrolledInstrument() should return false on platforms
// without a compatible authenticator.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
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

// canMakePayment() and hasEnrolledInstrument() should return true on platforms
// with a compatible authenticator regardless of the presence of payment
// credentials.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_CanMakePayment_HasAuthenticator) {
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
    : public PaymentRequestPlatformBrowserTestBase {};

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

// Test that the feature can be disabled by the browser-side Finch flag.
class SecurePaymentConfirmationDisabledByFinchTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  SecurePaymentConfirmationDisabledByFinchTest() {
    // The feature should get disabled by the feature state despite experimental
    // web platform features being enabled.
    feature_list_.InitAndDisableFeature(features::kSecurePaymentConfirmation);
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

// Creation tests do not work on Android because there is not a way to
// override authenticator creation.
#if !defined(OS_ANDROID)
class SecurePaymentConfirmationCreationTest
    : public SecurePaymentConfirmationTest,
      public PaymentCredentialEnrollmentController::ObserverForTest,
      public content::WebContentsObserver {
 public:
  enum Event : int {
    AUTHENTICATOR_REQUEST,
    WEB_CONTENTS_DESTROYED,
  };

  void RespondToFutureEnrollments(bool confirm) {
    confirm_enroll_ = confirm;
    PaymentCredentialEnrollmentController::CreateForWebContents(
        GetActiveWebContents());
    PaymentCredentialEnrollmentController::FromWebContents(
        GetActiveWebContents())
        ->set_observer_for_test(this);
  }

  // PaymentCredentialEnrollmentController::ObserverForTest
  void OnDialogOpened() override {
    auto* controller = PaymentCredentialEnrollmentController::FromWebContents(
        GetActiveWebContents());
    EXPECT_EQ(nullptr, controller->GetTokenIfAvailable());
    if (confirm_enroll_)
      controller->OnConfirm();
    else
      controller->OnCancel();
  }

  // PaymentCredential creation uses the normal Web Authentication code path
  // for creating the public key credential, rather than using
  // IntenralAuthenticator. This stubs out authenticator instantiation in
  // content.
  void ReplaceFidoDiscoveryFactory(bool should_succeed,
                                   bool should_hang = false) {
    auto owned_virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    auto* virtual_device_factory = owned_virtual_device_factory.get();
    content::AuthenticatorEnvironment::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(owned_virtual_device_factory));
    virtual_device_factory->SetTransport(
        device::FidoTransportProtocol::kInternal);
    virtual_device_factory->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);
    virtual_device_factory->mutable_state()->fingerprints_enrolled = true;

    if (should_hang) {
      virtual_device_factory->mutable_state()->simulate_press_callback =
          base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
            event_waiter_->OnEvent(AUTHENTICATOR_REQUEST);
            return false;
          });
    }

    // Currently this only supports tests relying on user-verifying platform
    // authenticators.
    device::VirtualCtap2Device::Config config;
    config.is_platform_authenticator = true;
    config.internal_uv_support = true;
    config.user_verification_succeeds = should_succeed;
    virtual_device_factory->SetCtap2Config(config);
  }

  const std::string GetDefaultIconURL() {
    return https_server()->GetURL("a.com", "/icon.png").spec();
  }

  const std::string GetMerchantOrigin() {
    // Strip the trailing slash ("/") from the merchant origin in serialization
    // to match the implementation behavior.
    std::string merchant_origin = https_server()->GetURL("b.com", "/").spec();
    EXPECT_EQ('/', merchant_origin[merchant_origin.length() - 1]);
    merchant_origin = merchant_origin.substr(0, merchant_origin.length() - 1);
    EXPECT_NE('/', merchant_origin[merchant_origin.length() - 1]);

    return merchant_origin;
  }

  void ExpectNoEnrollDialogShown() {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogShown", 0);
  }

  void ExpectEnrollDialogShown(
      SecurePaymentConfirmationEnrollDialogShown result,
      int count) {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogShown",
        count);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogShown",
        result, count);
  }

  void ExpectNoEnrollDialogResult() {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogResult",
        0);
  }

  void ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult result,
      int count) {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogResult",
        count);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel.EnrollDialogResult",
        result, count);
  }

  void ExpectNoEnrollSystemPromptResult() {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "EnrollSystemPromptResult",
        0);
  }

  void ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult result,
      int count) {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "EnrollSystemPromptResult",
        count);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "EnrollSystemPromptResult",
        result, count);
  }

  void ExpectNoFunnelCount() {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "SystemPromptResult",
        0);
  }

  void ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult result,
                         int count) {
    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "SystemPromptResult",
        count);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "SystemPromptResult",
        result, count);
  }

  void ExpectJourneyLoggerEvent(bool spc_confirm_logged) {
    std::vector<base::Bucket> buckets =
        histogram_tester_.GetAllSamples("PaymentRequest.Events");
    EXPECT_EQ(
        spc_confirm_logged,
        buckets.size() == 1 &&
            buckets[0].min &
                JourneyLogger::EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION);
  }

  void ObserveEvent(Event event) {
    event_waiter_ =
        std::make_unique<autofill::EventWaiter<Event>>(std::list<Event>{event});
  }

  void ObserveWebContentsDestroyed() {
    ObserveEvent(WEB_CONTENTS_DESTROYED);
    Observe(GetActiveWebContents());
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    event_waiter_->OnEvent(WEB_CONTENTS_DESTROYED);
  }

  base::HistogramTester histogram_tester_;
  bool confirm_enroll_ = false;
  std::unique_ptr<autofill::EventWaiter<Event>> event_waiter_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest, UserCancel) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/false);

  EXPECT_EQ("AbortError: Request has been aborted.",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("createPaymentCredential($1)",
                                               GetDefaultIconURL())));
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kCanceled, 1);
  ExpectNoEnrollSystemPromptResult();
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       CreatePaymentCredential) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);

  EXPECT_EQ("OK",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("createPaymentCredential($1)",
                                               GetDefaultIconURL())));

  // Verify that credential id size gets recorded.
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes", 1U);
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       LookupPaymentCredential) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  // Cross the origin boundary.
  NavigateTo("b.com", "/secure_payment_confirmation.html");
  test_controller()->SetHasAuthenticator(true);
  ResetEventWaiterForSingleEvent(TestEvent::kUIDisplayed);

  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("getSecurePaymentConfirmationStatus($1)",
                         credentialIdentifier));

  WaitForObservedEvent();
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("display_name_for_instrument",
            test_controller()->app_descriptions().front().label);
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       ConfirmPaymentInCrossOriginIframe) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  NavigateTo("b.com", "/iframe_poster.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;
  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "postToIframe($1, $2);",
              https_server()->GetURL("c.com", "/iframe_receiver.html").spec(),
              credentialIdentifier)));
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       ChallengeIsReturned) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  // The `networkData` field is the base64 encoding of 'hello world', which is
  // set in `get_challenge.js`.
  EXPECT_EQ("{\"merchantData\":{\"merchantOrigin\":\"" + GetMerchantOrigin() +
                "\",\"total\":{\"currency\":\"USD\"," +
                "\"value\":\"0.01\"}},\"networkData\":\"aGVsbG8gd29ybGQ=\"}",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getChallenge($1, $2);",
                                               credentialIdentifier, "0.01")));

  // Verify that passing a promise into PaymentRequest.show() that updates the
  // `total` price will result in the challenge price being set only after the
  // promise resolves with the finalized price.
  EXPECT_EQ("{\"merchantData\":{\"merchantOrigin\":\"" + GetMerchantOrigin() +
                "\",\"total\":{\"currency\":\"USD\"," +
                "\"value\":\"0.02\"}},\"networkData\":\"aGVsbG8gd29ybGQ=\"}",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("getChallengeWithShowPromise($1, $2, $3);",
                                   credentialIdentifier, "0.01", "0.02")));

  // Verify that the returned challenge correctly reflects the modified amount.
  EXPECT_EQ(
      "{\"merchantData\":{\"merchantOrigin\":\"" + GetMerchantOrigin() +
          "\",\"total\":{\"currency\":\"USD\"," +
          "\"value\":\"0.03\"}},\"networkData\":\"aGVsbG8gd29ybGQ=\"}",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("getChallengeWithModifier($1, $2);",
                                         credentialIdentifier, "0.03")));

  // Verify that the returned challenge correctly reflects the modified amount
  // that is set when the promised passed into PaymentRequest.show() resolves.
  EXPECT_EQ(
      "{\"merchantData\":{\"merchantOrigin\":\"" + GetMerchantOrigin() +
          "\",\"total\":{\"currency\":\"USD\"," +
          "\"value\":\"0.04\"}},\"networkData\":\"aGVsbG8gd29ybGQ=\"}",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("getChallengeWithModifierAndShowPromise($1, $2);",
                             credentialIdentifier, "0.04")));

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 4);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       UserVerificationFails) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  // Make the authenticator fail to simulate the user cancelling.
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/false);
  confirm_payment_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ("Authenticator returned NOT_ALLOWED_ERROR.",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getChallenge($1, $2);",
                                               credentialIdentifier, "0.01")));

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kCanceled, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest, NonexistentIcon) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  EXPECT_EQ(
      "a JavaScript error: \"NetworkError: Unable to download payment "
      "instrument icon.\"\n",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "createCredentialAndReturnItsIdentifier($1)",
              https_server()->GetURL("a.com", "/nonexistent.png").spec()))
          .error);

  ExpectEnrollDialogShown(
      SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow, 1);
  ExpectNoEnrollDialogResult();
  ExpectNoEnrollSystemPromptResult();
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest, InsecureIcon) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  // Get the instrument icon from an insecure http server.
  ASSERT_TRUE(embedded_test_server()->Start());
  std::string icon_url =
      embedded_test_server()->GetURL("a.com", "/icon.png").spec();

  EXPECT_EQ(
      "a JavaScript error: \"SecurityError: 'instrument.icon' should be a "
      "secure URL\"\n",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             icon_url))
          .error);
  ExpectNoEnrollDialogShown();
  ExpectNoEnrollDialogResult();
  ExpectNoEnrollSystemPromptResult();
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       CreatePaymentCredentialTwice) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);

  EXPECT_EQ("OK",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("createPaymentCredential($1)",
                                               GetDefaultIconURL())));

  EXPECT_EQ("OK",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("createPaymentCredential($1)",
                                               GetDefaultIconURL())));

  // Verify that credential id size gets recorded.
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes", 2U);

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          2);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 2);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 2);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       WebContentsClosedDuringEnrollment) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true, /*should_hang=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);

  ObserveEvent(AUTHENTICATOR_REQUEST);
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("createPaymentCredential($1)", GetDefaultIconURL()));
  event_waiter_->Wait();

  // Expect no crash when the web contents is destroyed during enrollment.
  ObserveWebContentsDestroyed();
  GetActiveWebContents()->Close();
  event_waiter_->Wait();

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       UserVerificationFailsThenSucceeds) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  RespondToFutureEnrollments(/*confirm=*/true);
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/false);
  EXPECT_EQ(
      "a JavaScript error: \"NotAllowedError: The operation either timed out "
      "or was not allowed. See: "
      "https://www.w3.org/TR/webauthn-2/"
      "#sctn-privacy-considerations-client.\"\n",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .error);

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          1);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 1);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled, 1);

  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  // The `networkData` field is the base64 encoding of 'hello world', which is
  // set in `get_challenge.js`.
  EXPECT_EQ("{\"merchantData\":{\"merchantOrigin\":\"" + GetMerchantOrigin() +
                "\",\"total\":{\"currency\":\"USD\"," +
                "\"value\":\"0.01\"}},\"networkData\":\"aGVsbG8gd29ybGQ=\"}",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getChallenge($1, $2);",
                                               credentialIdentifier, "0.01")));

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          2);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted, 2);
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      2);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled, 1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);

  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

std::unique_ptr<net::test_server::HttpResponse> HangRequest(
    const base::RepeatingClosure& on_called,
    const net::test_server::HttpRequest& request) {
  EXPECT_EQ(request.relative_url, "/icon.png");
  on_called.Run();
  return std::make_unique<net::test_server::HungResponse>();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       WebContentsClosedDuringIconDownload) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  net::EmbeddedTestServer hanging_server(net::EmbeddedTestServer::TYPE_HTTPS);
  hanging_server.ServeFilesFromSourceDirectory("components/test/data/payments");

  // A RunLoop must be used to wait for HangRequest instead of the EventWaiter
  // because HangRequest is executed on a different thread.
  base::RunLoop wait_for_icon_download;
  hanging_server.RegisterRequestHandler(
      base::BindRepeating(HangRequest, wait_for_icon_download.QuitClosure()));
  ASSERT_TRUE(hanging_server.Start());

  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("createPaymentCredential($1)",
                         hanging_server.GetURL("a.com", "/icon.png")));
  wait_for_icon_download.Run();

  // Expect no crash when closing the web contents mid-request.
  ObserveWebContentsDestroyed();
  GetActiveWebContents()->Close();
  event_waiter_->Wait();

  ExpectEnrollDialogShown(
      SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow, 1);
  ExpectNoEnrollDialogResult();
  ExpectNoEnrollSystemPromptResult();
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

// Expect that an error is returned when there is no RP ID. This is a regression
// test for crbug.com/1183559.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest, MissingRpId) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ(
      "a JavaScript error: \"NotSupportedError: Required parameters missing in "
      "`options.payment`.\"\n",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("createCredentialWithNoRpId($1)",
                                         GetDefaultIconURL()))
          .error);

  ExpectNoEnrollDialogShown();
  ExpectNoEnrollDialogResult();
  ExpectNoEnrollSystemPromptResult();
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace payments
