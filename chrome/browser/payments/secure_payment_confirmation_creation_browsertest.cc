// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/payments/secure_payment_confirmation_browsertest.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/test/browser_test.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

// Creation tests do not work on Android because there is not a way to
// override authenticator creation.
#if BUILDFLAG(IS_ANDROID)
#error "These tests are unsupported on Android"
#endif

namespace payments {
namespace {

class SecurePaymentConfirmationCreationTest
    : public SecurePaymentConfirmationTest,
      public content::WebContentsObserver {
 public:
  enum Event : int {
    AUTHENTICATOR_REQUEST,
    WEB_CONTENTS_DESTROYED,
  };

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
    // Strip the trailing slash ("/") from the merchant origin in
    // serialization to match the implementation behavior.
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
  std::unique_ptr<autofill::EventWaiter<Event>> event_waiter_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       SuccessfulEnrollment) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  std::string expected_response = "OK";

  EXPECT_EQ(expected_response,
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("createPaymentCredential($1)",
                                               GetDefaultIconURL())));

  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          0);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kCanceled, 0);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest, CredentialType) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ(
      "PublicKeyCredential",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("createCredentialAndReturnItsType($1)",
                                         GetDefaultIconURL())));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       CreatePaymentCredential) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  EXPECT_EQ("webauthn.create",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "createCredentialAndReturnClientDataType($1)",
                                GetDefaultIconURL())));

  // Verify that credential id size gets recorded.
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes", 1U);
  int expected_enroll_histogram_value_ = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value_);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value_);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

class SecurePaymentConfirmationCreationDisableDebugTest
    : public SecurePaymentConfirmationCreationTest {
 public:
  SecurePaymentConfirmationCreationDisableDebugTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSecurePaymentConfirmation},
        /*disabled_features=*/{features::kSecurePaymentConfirmationDebug});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationDisableDebugTest,
                       RequireUserVerifyingPlatformAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  EXPECT_EQ(
      "NotSupportedError: A user verifying platform authenticator with "
      "resident key support is required for 'payment' extension.",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("createPaymentCredential($1)",
                                         GetDefaultIconURL())));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       LookupPaymentCredential) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
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

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       PaymentExtension) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  std::string first_credential_identifier =
      content::EvalJs(GetActiveWebContents(),
                      "createPublicKeyCredentialWithPaymentExtensionAndReturnIt"
                      "sId('user_123')")
          .ExtractString();
  ASSERT_EQ(std::string::npos, first_credential_identifier.find("Error"))
      << first_credential_identifier;

  std::string second_credential_identifier =
      content::EvalJs(GetActiveWebContents(),
                      "createPublicKeyCredentialWithPaymentExtensionAndReturnIt"
                      "sId('user_456')")
          .ExtractString();
  ASSERT_EQ(std::string::npos, second_credential_identifier.find("Error"))
      << second_credential_identifier;
  ASSERT_NE(first_credential_identifier, second_credential_identifier);

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;
  std::string expected_result = "0.01";

  EXPECT_EQ(expected_result,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("getTotalAmountFromClientData($1, $2);",
                                   first_credential_identifier, "0.01")));
  EXPECT_EQ(expected_result,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("getTotalAmountFromClientData($1, $2);",
                                   second_credential_identifier, "0.01")));
}

// b.com cannot create a credential with RP = "a.com".
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       RelyingPartyIsEnforced) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("b.com", "/secure_payment_confirmation.html");
  EXPECT_EQ(
      "SecurityError: The relying party ID is not a registrable domain suffix "
      "of, nor equal to the current domain.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString());
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       ConfirmPaymentInCrossOriginIframe) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  // Load a cross-origin iframe that can initiate SPC.
  content::WebContents* tab = GetActiveWebContents();
  GURL iframe_url = https_server()->GetURL(
      "b.com", "/secure_payment_confirmation_iframe.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // Trigger SPC and capture the response.
  // EvalJs waits for JavaScript promise to resolve.
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  std::string response =
      content::EvalJs(iframe, content::JsReplace("requestPayment($1);",
                                                 credentialIdentifier))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  std::string* type = value->FindStringKey("type");
  ASSERT_NE(nullptr, type) << response;
  EXPECT_EQ("payment.get", *type);

  std::string* origin = value->FindStringKey("origin");
  ASSERT_NE(nullptr, origin) << response;
  EXPECT_EQ(https_server()->GetURL("b.com", "/"), GURL(*origin));

  absl::optional<bool> cross_origin = value->FindBoolKey("crossOrigin");
  ASSERT_TRUE(cross_origin.has_value()) << response;
  EXPECT_TRUE(cross_origin.value());

  std::string* payee_name = value->FindStringPath("payment.payeeName");
  ASSERT_EQ(nullptr, payee_name) << response;

  std::string* payee_origin = value->FindStringPath("payment.payeeOrigin");
  ASSERT_NE(nullptr, payee_origin) << response;
  EXPECT_EQ(GURL("https://example-payee-origin.test"), GURL(*payee_origin));

  std::string* top_origin = value->FindStringPath("payment.topOrigin");
  ASSERT_NE(nullptr, top_origin) << response;
  EXPECT_EQ(https_server()->GetURL("a.com", "/"), GURL(*top_origin));

  std::string* rp = value->FindStringPath("payment.rp");
  ASSERT_NE(nullptr, rp) << response;
  EXPECT_EQ("a.com", *rp);

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       ConfirmPaymentInCrossOriginIframeWithPayeeName) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  // Load a cross-origin iframe that can initiate SPC.
  content::WebContents* tab = GetActiveWebContents();
  GURL iframe_url = https_server()->GetURL(
      "b.com", "/secure_payment_confirmation_iframe.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // Trigger SPC and capture the response.
  // EvalJs waits for JavaScript promise to resolve.
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  std::string response =
      content::EvalJs(iframe,
                      content::JsReplace("requestPaymentWithPayeeName($1);",
                                         credentialIdentifier))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  std::string* payee_name = value->FindStringPath("payment.payeeName");
  ASSERT_NE(nullptr, payee_name) << response;
  EXPECT_EQ("Example Payee", *payee_name);

  std::string* payee_origin = value->FindStringPath("payment.payeeOrigin");
  ASSERT_EQ(nullptr, payee_origin) << response;

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(
    SecurePaymentConfirmationCreationTest,
    ConfirmPaymentInCrossOriginIframeWithPayeeNameAndOrigin) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  // Load a cross-origin iframe that can initiate SPC.
  content::WebContents* tab = GetActiveWebContents();
  GURL iframe_url = https_server()->GetURL(
      "b.com", "/secure_payment_confirmation_iframe.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // Trigger SPC and capture the response.
  // EvalJs waits for JavaScript promise to resolve.
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  std::string response =
      content::EvalJs(iframe, content::JsReplace(
                                  "requestPaymentWithPayeeNameAndOrigin($1);",
                                  credentialIdentifier))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  std::string* payee_name = value->FindStringPath("payment.payeeName");
  ASSERT_NE(nullptr, payee_name) << response;
  EXPECT_EQ("Example Payee", *payee_name);

  std::string* payee_origin = value->FindStringPath("payment.payeeOrigin");
  ASSERT_NE(nullptr, payee_origin) << response;
  EXPECT_EQ(GURL("https://example-payee-origin.test"), GURL(*payee_origin));

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       ChallengeIsReturned) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
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
  EXPECT_EQ("0.01", content::EvalJs(GetActiveWebContents(),
                                    content::JsReplace(
                                        "getTotalAmountFromClientData($1, $2);",
                                        credentialIdentifier, "0.01")));

  // Verify that passing a promise into PaymentRequest.show() that updates the
  // `total` price will result in the client data price being set only after the
  // promise resolves with the finalized price.
  EXPECT_EQ("0.02",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getTotalAmountFromClientDataWithShowPromise($1, $2);",
                    credentialIdentifier, "0.02")));

  // Verify that the returned client data correctly reflects the modified
  // amount.
  EXPECT_EQ("0.03", content::EvalJs(
                        GetActiveWebContents(),
                        content::JsReplace(
                            "getTotalAmountFromClientDataWithModifier($1, $2);",
                            credentialIdentifier, "0.03")));

  // Verify that the returned client data correctly reflects the modified amount
  // that is set when the promised passed into PaymentRequest.show() resolves.
  EXPECT_EQ(
      "0.04",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "getTotalAmountFromClientDataWithModifierAndShowPromise($1, $2);",
              credentialIdentifier, "0.04")));

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 4);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       UserVerificationFails) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
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
  EXPECT_EQ(
      "The operation either timed out or was not allowed. See: "
      "https://www.w3.org/TR/webauthn-2/#sctn-privacy-considerations-client.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("getTotalAmountFromClientData($1, $2);",
                             credentialIdentifier, "0.01")));

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);
  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kCanceled, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       CreatePaymentCredentialTwice) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

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

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectEnrollSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 2);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       WebContentsClosedDuringEnrollmentOSPrompt) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true, /*should_hang=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  std::list<Event> expected_events_ =
      std::list<Event>{Event::AUTHENTICATOR_REQUEST};
  event_waiter_ =
      std::make_unique<autofill::EventWaiter<Event>>(expected_events_);
  ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("createPaymentCredential($1)", GetDefaultIconURL()));
  event_waiter_->Wait();

  // Expect no crash when the web contents is destroyed during enrollment while
  // the OS enrollment prompt is showing.
  ObserveWebContentsDestroyed();
  GetActiveWebContents()->Close();
  event_waiter_->Wait();

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  ExpectNoFunnelCount();
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/false);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       UserVerificationSucceeds) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");

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
  EXPECT_EQ("0.01", content::EvalJs(GetActiveWebContents(),
                                    content::JsReplace(
                                        "getTotalAmountFromClientData($1, $2);",
                                        credentialIdentifier, "0.01")));

  int expected_enroll_histogram_value = 0;
  ExpectEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown,
                          expected_enroll_histogram_value);
  ExpectEnrollDialogResult(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted,
      expected_enroll_histogram_value);
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled, 0);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted, 1);

  ExpectFunnelCount(SecurePaymentConfirmationSystemPromptResult::kAccepted, 1);
  ExpectJourneyLoggerEvent(/*spc_confirm_logged=*/true);
}

// Test allowing a failed icon download with iconMustBeShown option
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       IconMustBeShownFalse) {
  ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("createCredentialAndReturnItsIdentifier($1)",
                             GetDefaultIconURL()))
          .ExtractString();

  // First ensure the icon URL is successfully parsed from clientData for a
  // valid icon.
  std::string icon_data =
      "data:image/"
      "png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAAACnej3aAAAAAXRS"
      "TlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=";
  EXPECT_EQ(icon_data,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getSecurePaymentConfirmationResponseIconWithInstrument({"
                    "  displayName: 'display_name_for_instrument',"
                    "  icon: $1,"
                    "  iconMustBeShown: false,"
                    "}, $2)",
                    icon_data, credentialIdentifier)));

  // Now verify that the icon string is cleared from clientData for an invalid
  // icon.
  EXPECT_EQ("",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getSecurePaymentConfirmationResponseIconWithInstrument({"
                    "  displayName: 'display_name_for_instrument',"
                    "  icon: 'https://example.com/invalid-icon.png',"
                    "  iconMustBeShown: false,"
                    "}, $1)",
                    credentialIdentifier)));
}

}  // namespace
}  // namespace payments
