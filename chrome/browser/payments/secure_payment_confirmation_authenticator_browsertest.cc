// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/payments/secure_payment_confirmation_browsertest.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/test/browser_test.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

// Authenticator tests do not work on Android because there is not currently a
// way to install a virtual authenticator.
#if BUILDFLAG(IS_ANDROID)
#error "These tests are unsupported on Android"
#endif

// TODO(crbug.com/40870879): Temporarily disable the tests on macOS since they
// do not yet work with current WebAuthn UI.
#if !BUILDFLAG(IS_MAC)

namespace payments {
namespace {

using Event2 = payments::JourneyLogger::Event2;

struct PaymentCredentialInfo {
  std::string webidl_type;
  std::string type;
  std::string id;
};

// Base class for Secure Payment Confirmation tests that use a virtual FIDO
// authenticator in order to test the end-to-end flow.
class SecurePaymentConfirmationAuthenticatorTestBase
    : public SecurePaymentConfirmationTest,
      public content::WebContentsObserver {
 public:
  enum Event : int {
    AUTHENTICATOR_REQUEST,
    WEB_CONTENTS_DESTROYED,
  };

  // Installs a virtual FIDO authenticator device for the tests.
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting>
  ReplaceFidoDiscoveryFactory(bool should_succeed, bool should_hang = false) {
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
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

    return std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
        std::move(virtual_device_factory));
  }

  // Creates an SPC-enabled WebAuthn credential, and places information about it
  // in `out_info`. The `out_info` parameter may be nullptr, in which case the
  // credential is created and checked to have succeeded, but no information is
  // returned.
  //
  // The optional input `user_id` parameter comes after the output parameter as
  // most callers will want to set `out_info` but not `user_id`.
  void CreatePaymentCredential(PaymentCredentialInfo* out_info = nullptr,
                               const std::string& user_id = "user_123") {
    std::string response =
        content::EvalJs(
            GetActiveWebContents(),
            content::JsReplace("createPaymentCredential($1)", user_id))
            .ExtractString();
    ASSERT_EQ(std::string::npos, response.find("Error")) << response;

    std::optional<base::Value> value = base::JSONReader::Read(response);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value->is_dict());
    const auto& value_dict = value->GetDict();

    const std::string* webidl_type = value_dict.FindString("webIdlType");
    ASSERT_NE(nullptr, webidl_type) << response;

    const std::string* type = value_dict.FindString("type");
    ASSERT_NE(nullptr, type) << response;

    const std::string* id = value_dict.FindString("id");
    ASSERT_NE(nullptr, id) << response;

    if (out_info) {
      *out_info = {*webidl_type, *type, *id};
    }
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

  std::unique_ptr<autofill::EventWaiter<Event>> event_waiter_;
};

using SecurePaymentConfirmationAuthenticatorCreateTest =
    SecurePaymentConfirmationAuthenticatorTestBase;

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorCreateTest,
                       CreatePaymentCredential) {
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  PaymentCredentialInfo info;
  CreatePaymentCredential(&info);

  // The created credential should be a normal WebAuthn credential, of the right
  // WebIDL and internal type.
  EXPECT_EQ("PublicKeyCredential", info.webidl_type);
  EXPECT_EQ("webauthn.create", info.type);

  // Verify that the correct metrics are recorded.
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes", 1U);

  // Check that we can create a second credential, and that the tracked metrics
  // update.
  CreatePaymentCredential();
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes", 2U);
}

// b.com cannot create a credential with RP = "a.com".
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorCreateTest,
                       RelyingPartyIsEnforced) {
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("b.com", "/secure_payment_confirmation.html");
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(), "createPaymentCredential()")
          .ExtractString(),
      testing::HasSubstr("SecurityError: The relying party ID is not"));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorCreateTest,
                       WebContentsClosedDuringEnrollmentOSPrompt) {
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true,
                                                     /*should_hang=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  std::list<Event> expected_events_ =
      std::list<Event>{Event::AUTHENTICATOR_REQUEST};
  event_waiter_ =
      std::make_unique<autofill::EventWaiter<Event>>(expected_events_);
  ExecuteScriptAsync(GetActiveWebContents(), "createPaymentCredential()");
  ASSERT_TRUE(event_waiter_->Wait());

  // Expect no crash when the web contents is destroyed during enrollment while
  // the OS enrollment prompt is showing.
  ObserveWebContentsDestroyed();
  GetActiveWebContents()->Close();
  ASSERT_TRUE(event_waiter_->Wait());
}

class SecurePaymentConfirmationAuthenticatorCreateDisableDebugTest
    : public SecurePaymentConfirmationAuthenticatorCreateTest {
 public:
  SecurePaymentConfirmationAuthenticatorCreateDisableDebugTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{::features::kSecurePaymentConfirmation},
        /*disabled_features=*/{::features::kSecurePaymentConfirmationDebug});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SecurePaymentConfirmationAuthenticatorCreateDisableDebugTest,
    RequireUserVerifyingPlatformAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  EXPECT_EQ(
      "NotSupportedError: A user verifying platform authenticator with "
      "resident key support is required for 'payment' extension.",
      content::EvalJs(GetActiveWebContents(), "createPaymentCredential()"));
}

using SecurePaymentConfirmationAuthenticatorGetTest =
    SecurePaymentConfirmationAuthenticatorTestBase;

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       ConfirmPaymentInCrossOriginIframe) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  PaymentCredentialInfo credential_info;
  CreatePaymentCredential(&credential_info);

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
      content::EvalJs(
          iframe, content::JsReplace("requestPayment($1);", credential_info.id))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  std::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const base::Value::Dict& value_dict = value->GetDict();
  const std::string* type = value_dict.FindString("type");
  ASSERT_NE(nullptr, type) << response;
  EXPECT_EQ("payment.get", *type);

  const std::string* origin = value_dict.FindString("origin");
  ASSERT_NE(nullptr, origin) << response;
  EXPECT_EQ(https_server()->GetURL("b.com", "/"), GURL(*origin));

  std::optional<bool> cross_origin = value_dict.FindBool("crossOrigin");
  ASSERT_TRUE(cross_origin.has_value()) << response;
  EXPECT_TRUE(cross_origin.value());

  const std::string* payee_name =
      value_dict.FindStringByDottedPath("payment.payeeName");
  ASSERT_EQ(nullptr, payee_name) << response;

  const std::string* payee_origin =
      value_dict.FindStringByDottedPath("payment.payeeOrigin");
  ASSERT_NE(nullptr, payee_origin) << response;
  EXPECT_EQ(GURL("https://example-payee-origin.test"), GURL(*payee_origin));

  const std::string* top_origin =
      value_dict.FindStringByDottedPath("payment.topOrigin");
  ASSERT_NE(nullptr, top_origin) << response;
  EXPECT_EQ(https_server()->GetURL("a.com", "/"), GURL(*top_origin));
  const std::string* rpId = value_dict.FindStringByDottedPath("payment.rpId");
  ASSERT_NE(nullptr, rpId) << response;
  EXPECT_EQ("a.com", *rpId);

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown, Event2::kCompleted,
                         Event2::kPayClicked, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation,
                         Event2::kSelectedSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       ConfirmPaymentInCrossOriginIframeWithPayeeName) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  PaymentCredentialInfo credential_info;
  CreatePaymentCredential(&credential_info);

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
                                         credential_info.id))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  std::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  const base::Value::Dict& dict = value->GetDict();
  const std::string* payee_name =
      dict.FindStringByDottedPath("payment.payeeName");
  ASSERT_NE(nullptr, payee_name) << response;
  EXPECT_EQ("Example Payee", *payee_name);

  const std::string* payee_origin =
      dict.FindStringByDottedPath("payment.payeeOrigin");
  ASSERT_EQ(nullptr, payee_origin) << response;

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown, Event2::kCompleted,
                         Event2::kPayClicked, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation,
                         Event2::kSelectedSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(
    SecurePaymentConfirmationAuthenticatorGetTest,
    ConfirmPaymentInCrossOriginIframeWithPayeeNameAndOrigin) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  PaymentCredentialInfo credential_info;
  CreatePaymentCredential(&credential_info);

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
                                  credential_info.id))
          .ExtractString();

  ASSERT_EQ(std::string::npos, response.find("Error"));
  std::optional<base::Value> value = base::JSONReader::Read(response);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  const base::Value::Dict& dict = value->GetDict();
  const std::string* payee_name =
      dict.FindStringByDottedPath("payment.payeeName");
  ASSERT_NE(nullptr, payee_name) << response;
  EXPECT_EQ("Example Payee", *payee_name);

  const std::string* payee_origin =
      dict.FindStringByDottedPath("payment.payeeOrigin");
  ASSERT_NE(nullptr, payee_origin) << response;
  EXPECT_EQ(GURL("https://example-payee-origin.test"), GURL(*payee_origin));

  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown, Event2::kCompleted,
                         Event2::kPayClicked, Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation,
                         Event2::kSelectedSecurePaymentConfirmation});
}

// Test allowing a failed icon download with iconMustBeShown option
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       IconMustBeShownFalse) {
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  PaymentCredentialInfo credential_info;
  CreatePaymentCredential(&credential_info);

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
                    icon_data, credential_info.id)));

  // Now verify that the icon string is cleared from clientData for an invalid
  // icon.
  EXPECT_EQ("",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getSecurePaymentConfirmationResponseIconWithInstrument({"
                    "  displayName: 'display_name_for_instrument',"
                    "  icon: 'https://example.test/invalid-icon.png',"
                    "  iconMustBeShown: false,"
                    "}, $1)",
                    credential_info.id)));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       MultipleRegisteredCredentials) {
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
  NavigateTo("a.com", "/secure_payment_confirmation.html");

  PaymentCredentialInfo first_info;
  CreatePaymentCredential(&first_info, "user_123");
  PaymentCredentialInfo second_info;
  CreatePaymentCredential(&second_info, "user_456");

  ASSERT_NE(first_info.id, second_info.id);

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;
  std::string expected_result = "0.01";

  EXPECT_EQ(expected_result,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("getTotalAmountFromClientData($1, $2);",
                                   first_info.id, "0.01")));
  EXPECT_EQ(expected_result,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("getTotalAmountFromClientData($1, $2);",
                                   second_info.id, "0.01")));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       UserVerificationFails) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  PaymentCredentialInfo credential_info;

  {
    auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);
    CreatePaymentCredential(&credential_info);
  }

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);

  // Make the authenticator fail to simulate the user cancelling out of the
  // WebAuthn dialog.
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/false);
  confirm_payment_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The operation either timed out or was not allowed. See: "
      "https://www.w3.org/TR/webauthn-2/#sctn-privacy-considerations-client.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("getTotalAmountFromClientData($1, $2);",
                             credential_info.id, "0.01")));

  // WebAuthn dialog failure is recorded as kOtherAborted. Since we made it
  // past the Transaction UX to the WebAuthn dialog, we should still log
  // kPayClicked and kSelectedSecurePaymentConfirmation.
  ExpectEvent2Histogram({Event2::kInitiated, Event2::kShown,
                         Event2::kOtherAborted, Event2::kPayClicked,
                         Event2::kHadInitialFormOfPayment,
                         Event2::kRequestMethodSecurePaymentConfirmation,
                         Event2::kSelectedSecurePaymentConfirmation});
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationAuthenticatorGetTest,
                       HandlesShowPromisesAndModifiers) {
  NavigateTo("a.com", "/secure_payment_confirmation.html");
  auto scoped_auth_env = ReplaceFidoDiscoveryFactory(/*should_succeed=*/true);

  PaymentCredentialInfo credential_info;
  CreatePaymentCredential(&credential_info);

  NavigateTo("b.com", "/get_challenge.html");
  test_controller()->SetHasAuthenticator(true);
  confirm_payment_ = true;

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ("0.01", content::EvalJs(GetActiveWebContents(),
                                    content::JsReplace(
                                        "getTotalAmountFromClientData($1, $2);",
                                        credential_info.id, "0.01")));

  // Verify that passing a promise into PaymentRequest.show() that updates the
  // `total` price will result in the client data price being set only after the
  // promise resolves with the finalized price.
  EXPECT_EQ("0.02",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getTotalAmountFromClientDataWithShowPromise($1, $2);",
                    credential_info.id, "0.02")));

  // Verify that the returned client data correctly reflects the modified
  // amount.
  EXPECT_EQ("0.03", content::EvalJs(
                        GetActiveWebContents(),
                        content::JsReplace(
                            "getTotalAmountFromClientDataWithModifier($1, $2);",
                            credential_info.id, "0.03")));

  // Verify that the returned client data correctly reflects the modified amount
  // that is set when the promised passed into PaymentRequest.show() resolves.
  EXPECT_EQ(
      "0.04",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "getTotalAmountFromClientDataWithModifierAndShowPromise($1, $2);",
              credential_info.id, "0.04")));
}

}  // namespace
}  // namespace payments

#endif  // !BUILDFLAG(IS_MAC)
