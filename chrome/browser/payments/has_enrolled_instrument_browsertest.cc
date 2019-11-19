// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/payments/personal_data_manager_test_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

// TODO(https://crbug.com/994799): Unify error messages between desktop and
// Android.
const char kNotSupportedMessage[] =
#if defined(OS_ANDROID)
    "NotSupportedError: Payment method not supported. "
#else
    "NotSupportedError: The payment method \"basic-card\" is not supported. "
#endif  // OS_ANDROID
    "User does not have valid information on file.";

autofill::CreditCard GetCardWithBillingAddress(
    const autofill::AutofillProfile& profile) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  return card;
}

class HasEnrolledInstrumentTest : public PlatformBrowserTest {
 public:
  HasEnrolledInstrumentTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~HasEnrolledInstrumentTest() override {}

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/has_enrolled_instrument.html")));
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  const std::string& not_supported_message() const {
    return not_supported_message_;
  }

 private:
  PaymentRequestTestController test_controller_;
  net::EmbeddedTestServer https_server_;
  std::string not_supported_message_ = kNotSupportedMessage;

  DISALLOW_COPY_AND_ASSIGN(HasEnrolledInstrumentTest);
};

class HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument
    : public HasEnrolledInstrumentTest {
 public:
  HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument},
        /*disabled_features=*/{
          features::kPaymentRequestSkipToGPay,
#if defined(OS_ANDROID)
              ::chrome::android::kNoCreditCardAbort,
#endif
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoCard) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    NoCard) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoBillingAddress) {
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    NoBillingAddress) {
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveShippingNoBillingAddress) {
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           autofill::test::GetFullProfile());
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    HaveShippingNoBillingAddress) {
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           autofill::test::GetFullProfile());
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    HaveShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  base::ASCIIToUTF16("1111111111111111"));
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  base::ASCIIToUTF16("1111111111111111"));
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetExpirationYear(2000);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetExpirationYear(2000);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

// TODO(https://crbug.com/994799): Unify autofill data validation and returned
// data across platforms.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveNoNameShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();

  address.SetRawInfo(autofill::ServerFieldType::NAME_FIRST, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_LAST, base::string16());

  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    HaveNoNameShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();

  address.SetRawInfo(autofill::ServerFieldType::NAME_FIRST, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_LAST, base::string16());

  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

// TODO(https://crbug.com/994799): Unify autofill data requirements between
// desktop and Android.
#if defined(OS_ANDROID)
  // Android requires the billing address to have a name.
  bool is_no_name_billing_address_valid = false;
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
#else
  // Desktop does not require the billing address to have a name.
  bool is_no_name_billing_address_valid = true;
#endif  // OS_ANDROID

  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  // Shipping address requires recipient name on all platforms.
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));

  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(), "show()"));
  EXPECT_EQ(
      not_supported_message(),
      content::EvalJs(GetActiveWebContents(), "show({requestShipping:true})"));
  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::ASCIIToUTF16("this-is-not-a-valid-email-address"));
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
}

IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentTestWithStrictHasEnrolledAutofillInstrument,
    InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::ASCIIToUTF16("this-is-not-a-valid-email-address"));
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));

  EXPECT_EQ(not_supported_message(),
            content::EvalJs(GetActiveWebContents(),
                            "show({requestPayerEmail:true})"));
}

}  // namespace
}  // namespace payments
