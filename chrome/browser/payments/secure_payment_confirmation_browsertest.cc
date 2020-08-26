// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

static constexpr char kTestMethodData[] =
    "[{ "
    "  supportedMethods: 'secure-payment-confirmation',"
    "  data: {"
    "    action: 'authenticate',"
    "    instrumentId: 'x',"
    "    networkData: Uint8Array.from('x', c => c.charCodeAt(0)),"
    "    timeout: 60000,"
    "    fallbackUrl: 'https://fallback.example/url'"
    "}}]";

std::string getInvokePaymentRequestSnippet() {
  return base::StringPrintf("getStatusForMethodData(%s)", kTestMethodData);
}

#if !defined(OS_ANDROID)
static constexpr char kCreatePaymentCredential[] =
    "var PAYMENT_INSTRUMENT = {"
    "    displayName: 'display_name_for_instrument',"
    "    icon: 'https://pics.acme.com/00/p/aBjjjpqPb.png'"
    "};"
    "var PUBLIC_KEY_RP = {"
    "    id: 'a.com',"
    "    name: 'Acme'"
    "};"
    "var PUBLIC_KEY_PARAMETERS =  [{"
    "    type: 'public-key',"
    "    alg: -7,"
    "},];"
    "var PAYMENT_CREATION_OPTIONS = {"
    "    rp: PUBLIC_KEY_RP,"
    "    instrument: PAYMENT_INSTRUMENT,"
    "    challenge: new TextEncoder().encode('climb a mountain'),"
    "    pubKeyCredParams: PUBLIC_KEY_PARAMETERS,"
    "};"
    "navigator.credentials.create({ payment : PAYMENT_CREATION_OPTIONS })"
    "    .then(c => window.domAutomationController.send("
    "              'paymentCredential: OK'),"
    "          e => window.domAutomationController.send("
    "              'paymentCredential: ' + e.toString()));";
#endif

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

#if defined(OS_ANDROID)
// TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for Android,
// so this behavior can be tested on Android as well.
#define MAYBE_PaymentSheetShowsApp DISABLED_PaymentSheetShowsApp
#else
#define MAYBE_PaymentSheetShowsApp PaymentSheetShowsApp
#endif  // OS_ANDROID
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_PaymentSheetShowsApp) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // ExecJs starts executing JavaScript and immediately returns, not waiting for
  // any promise to return.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              getInvokePaymentRequestSnippet()));

  WaitForObservedEvent();
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("Stub label", test_controller()->app_descriptions().front().label);
}

// canMakePayment() and hasEnrolledInstrument() should return false on platforms
// without a compatible authenticator.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       CanMakePayment_NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

// canMakePayment() and hasEnrolledInstrument() should return true on platforms
// with a compatible authenticator regardless of the presence of payment
// credentials.
#if defined(OS_ANDROID)
// TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for Android,
// so this behavior can be tested on Android as well.
#define MAYBE_CanMakePayment_HasAuthenticator \
  DISABLED_CanMakePayment_HasAuthenticator
#else
#define MAYBE_CanMakePayment_HasAuthenticator CanMakePayment_HasAuthenticator
#endif  // OS_ANDROID
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_CanMakePayment_HasAuthenticator) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("true", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("true", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

// Intentionally do not enable the "SecurePaymentConfirmation" Blink runtime
// feature.
class SecurePaymentConfirmationDisabledTest
    : public PaymentRequestPlatformBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       PaymentMethodNotSupported) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       CannotMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

// Creation tests do not work on Android because there is not a way to
// override authenticator creation.
#if !defined(OS_ANDROID)
class SecurePaymentConfirmationCreationTest
    : public SecurePaymentConfirmationTest {
 public:
  // PaymentCredential creation uses the normal Web Authentication code path
  // for creating the public key credential, rather than using
  // IntenralAuthenticator. This stubs out authenticator instantiation in
  // content.
  void ReplaceFidoDiscoveryFactory() {
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

    // Currently this only supports tests relying on user-verifying platform
    // authenticators.
    device::VirtualCtap2Device::Config config;
    config.is_platform_authenticator = true;
    config.internal_uv_support = true;
    virtual_device_factory->SetCtap2Config(config);
  }
};

#if defined(OS_WIN)
// TODO(kenrb): This experiment is currently only available on Mac, but this
// test should work on all non-Android platforms. There is a Windows failure
// that still needs to be investigated.
#define MAYBE_CreatePaymentCredential DISABLED_CreatePaymentCredential
#else
#define MAYBE_CreatePaymentCredential CreatePaymentCredential
#endif
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       MAYBE_CreatePaymentCredential) {
  ReplaceFidoDiscoveryFactory();
  NavigateTo("a.com", "/payment_handler_status.html");

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), kCreatePaymentCredential, &result));
  EXPECT_EQ(result, "paymentCredential: OK");
}
#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace payments
