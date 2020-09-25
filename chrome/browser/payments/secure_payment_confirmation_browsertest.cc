// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webdata/common/web_data_service_consumer.h"
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
    "    credentialIds: [Uint8Array.from('cred', c => c.charCodeAt(0))],"
    "    networkData: Uint8Array.from('network_data', c => c.charCodeAt(0)),"
    "    timeout: 60000,"
    "    fallbackUrl: 'https://fallback.example/url'"
    "}}]";

std::string getInvokePaymentRequestSnippet() {
  return base::StringPrintf("getStatusForMethodData(%s)", kTestMethodData);
}

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

#if !defined(OS_ANDROID)
std::string getPaymentCreationOptions(const std::string& icon_url) {
  return base::StrCat(
      {"var PAYMENT_INSTRUMENT = {"
       "    displayName: 'display_name_for_instrument',"
       "    icon: '",
       icon_url,
       "'};"
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
       "};"});
}

static constexpr char kCreatePaymentCredential[] =
    "navigator.credentials.create({ payment : PAYMENT_CREATION_OPTIONS })"
    "    .then(c => window.domAutomationController.send("
    "              'paymentCredential: OK'),"
    "          e => window.domAutomationController.send("
    "              'paymentCredential: ' + e.toString()));";
#endif

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
    databse_write_responded_ = true;
  }

  void OnAppListReady() override {
    PaymentRequestPlatformBrowserTestBase::OnAppListReady();
    if (confirm_payment_)
      ASSERT_TRUE(test_controller()->ConfirmPayment());
  }

  bool databse_write_responded_ = false;
  bool confirm_payment_ = false;
  base::test::ScopedFeatureList feature_list_;
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
#define MAYBE_NoInstrumentInStorage DISABLED_NoInstrumentInStorage
#define MAYBE_CheckInstrumentInStorageAfterCanMakePayment \
  DISABLED_CheckInstrumentInStorageAfterCanMakePayment
#define MAYBE_PaymentSheetShowsApp DISABLED_PaymentSheetShowsApp
#else
#define MAYBE_NoInstrumentInStorage NoInstrumentInStorage
#define MAYBE_CheckInstrumentInStorageAfterCanMakePayment \
  CheckInstrumentInStorageAfterCanMakePayment
#define MAYBE_PaymentSheetShowsApp PaymentSheetShowsApp
#endif  // OS_ANDROID

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_NoInstrumentInStorage) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_CheckInstrumentInStorageAfterCanMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(
          GetActiveWebContents(),
          base::StringPrintf("getStatusForMethodDataAfterCanMakePayment(%s, "
                             "/*checkCanMakePaymentFirst=*/true)",
                             kTestMethodData)));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_PaymentSheetShowsApp) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");
  std::vector<uint8_t> credential_id = {'c', 'r', 'e', 'd'};
  std::vector<uint8_t> icon = GetEncodedIcon("icon.png");
  WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()),
      ServiceAccessType::EXPLICIT_ACCESS)
      ->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              std::move(credential_id), "relying-party.example",
              base::ASCIIToUTF16("Stub label"), std::move(icon)),
          /*consumer=*/this);
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // ExecJs starts executing JavaScript and immediately returns, not waiting for
  // any promise to return.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              getInvokePaymentRequestSnippet()));

  WaitForObservedEvent();
  EXPECT_TRUE(databse_write_responded_);
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
        "canMakePaymentForMethodDataTwice(%s)", kTestMethodData);
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

// Test that the feature can be disabled by the browser-side Finch flag
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
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledByFinchTest,
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

  const std::string GetDefaultIconURL() {
    return https_server()->GetURL("a.com", "/icon.png").spec();
  }
};

#if defined(OS_WIN)
// TODO(kenrb): This experiment is currently only available on Mac, but this
// test should work on all non-Android platforms. There is a Windows failure
// that still needs to be investigated.
#define MAYBE_CreatePaymentCredential DISABLED_CreatePaymentCredential
#define MAYBE_LookupPaymentCredential DISABLED_LookupPaymentCredential
#define MAYBE_ConfirmPaymentInCrossOriginIframe \
  DISABLED_ConfirmPaymentInCrossOriginIframe
#else
#define MAYBE_CreatePaymentCredential CreatePaymentCredential
#define MAYBE_LookupPaymentCredential LookupPaymentCredential
#define MAYBE_ConfirmPaymentInCrossOriginIframe \
  ConfirmPaymentInCrossOriginIframe
#endif
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       MAYBE_CreatePaymentCredential) {
  ReplaceFidoDiscoveryFactory();
  NavigateTo("a.com", "/payment_handler_status.html");

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(),
      base::StrCat({getPaymentCreationOptions(GetDefaultIconURL()),
                    kCreatePaymentCredential}),
      &result));
  EXPECT_EQ(result, "paymentCredential: OK");
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       MAYBE_LookupPaymentCredential) {
  ReplaceFidoDiscoveryFactory();
  NavigateTo("a.com", "/payment_handler_status.html");
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          base::StrCat(
              {"async function createPaymentCredential() {",
               getPaymentCreationOptions(GetDefaultIconURL()),
               "  const c = await navigator.credentials.create("
               "    {payment: PAYMENT_CREATION_OPTIONS});"
               "  return btoa(String.fromCharCode(...new Uint8Array(c.rawId)));"
               "};"
               "createPaymentCredential();"}))
          .ExtractString();
  std::string script = content::JsReplace(
      "getStatusForMethodData([{"
      "  supportedMethods: 'secure-payment-confirmation',"
      "  data: {"
      "    action: 'authenticate',"
      "    credentialIds: [Uint8Array.from(atob($1), b => b.charCodeAt(0))],"
      "    networkData: new TextEncoder().encode('network_data'),"
      "    timeout: 60000,"
      "    fallbackUrl: 'https://fallback.example/url'"
      "}}])",
      credentialIdentifier);

  // Cross the origin boundary.
  NavigateTo("b.com", "/payment_handler_status.html");
  test_controller()->SetHasAuthenticator(true);
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // ExecJs starts executing JavaScript and immediately returns, not waiting for
  // any promise to return.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), script));

  WaitForObservedEvent();
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("display_name_for_instrument",
            test_controller()->app_descriptions().front().label);
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationCreationTest,
                       MAYBE_ConfirmPaymentInCrossOriginIframe) {
  NavigateTo("a.com", "/payment_handler_status.html");
  ReplaceFidoDiscoveryFactory();
  std::string credentialIdentifier =
      content::EvalJs(
          GetActiveWebContents(),
          base::StrCat(
              {"async function createPaymentCredential() {",
               getPaymentCreationOptions(GetDefaultIconURL()),
               "  const c = await navigator.credentials.create("
               "    {payment: PAYMENT_CREATION_OPTIONS});"
               "  return btoa(String.fromCharCode(...new Uint8Array(c.rawId)));"
               "};"
               "createPaymentCredential();"}))
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
}
#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace payments
