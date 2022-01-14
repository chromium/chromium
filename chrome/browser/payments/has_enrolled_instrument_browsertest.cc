// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/personal_data_manager_test_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class HasEnrolledInstrumentBaseTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  HasEnrolledInstrumentBaseTest(const HasEnrolledInstrumentBaseTest&) = delete;
  HasEnrolledInstrumentBaseTest& operator=(
      const HasEnrolledInstrumentBaseTest&) = delete;

  ~HasEnrolledInstrumentBaseTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/has_enrolled_instrument.html");
  }

  // Helper function to test that all variations of hasEnrolledInstrument()
  // returns |expected|.
  void ExpectHasEnrolledInstrumentIs(
      bool expected,
      const std::string& payment_method = "basic-card") {
    EXPECT_EQ(expected, content::EvalJs(
                            GetActiveWebContents(),
                            content::JsReplace("hasEnrolledInstrument({}, $1)",
                                               payment_method)));
    EXPECT_EQ(
        expected,
        content::EvalJs(GetActiveWebContents(),
                        content::JsReplace(
                            "hasEnrolledInstrument({requestShipping:true}, $1)",
                            payment_method)));
    EXPECT_EQ(expected,
              content::EvalJs(
                  GetActiveWebContents(),
                  content::JsReplace(
                      "hasEnrolledInstrument({requestPayerEmail:true}, $1)",
                      payment_method)));
  }

 protected:
  HasEnrolledInstrumentBaseTest() = default;
  base::test::ScopedFeatureList feature_list_;
};

class HasEnrolledInstrumentBasicCardTest
    : public HasEnrolledInstrumentBaseTest {
 public:
  HasEnrolledInstrumentBasicCardTest() {
    feature_list_.InitAndEnableFeature(features::kPaymentRequestBasicCard);
  }

  HasEnrolledInstrumentBasicCardTest(
      const HasEnrolledInstrumentBasicCardTest&) = delete;
  HasEnrolledInstrumentBasicCardTest& operator=(
      const HasEnrolledInstrumentBasicCardTest&) = delete;

  ~HasEnrolledInstrumentBasicCardTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest, NoCard) {
  ExpectHasEnrolledInstrumentIs(false);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest, NoBillingAddress) {
  AddCreditCard(autofill::test::GetCreditCard());
  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest,
                       HaveShippingNoBillingAddress) {
  CreateAndAddAutofillProfile();
  AddCreditCard(autofill::test::GetCreditCard());

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest,
                       HaveShippingAndBillingAddress) {
  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest, InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  u"1111111111111111");
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(false);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest, ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetExpirationYear(2000);
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(true);
}

// TODO(https://crbug.com/994799): Unify autofill data validation and returned
// data across platforms.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest,
                       HaveNoNameShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::NAME_FIRST, std::u16string());
  address.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE, std::u16string());
  address.SetRawInfo(autofill::ServerFieldType::NAME_LAST, std::u16string());
  // For structured names, it is necessary to explicitly reset the full name
  // and the full name with the prefix.
  address.SetInfo(autofill::ServerFieldType::NAME_FULL, std::u16string(),
                  "en-US");
  address.SetInfo(autofill::ServerFieldType::NAME_FULL_WITH_HONORIFIC_PREFIX,
                  std::u16string(), "en-US");
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest,
                       HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest, NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentBasicCardTest,
                       InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     u"this-is-not-a-valid-email-address");
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

class HasEnrolledInstrumentPaymentHandlerTest
    : public HasEnrolledInstrumentBaseTest {
 public:
  HasEnrolledInstrumentPaymentHandlerTest() {
    feature_list_.InitAndDisableFeature(features::kPaymentRequestBasicCard);
  }

  HasEnrolledInstrumentPaymentHandlerTest(
      const HasEnrolledInstrumentPaymentHandlerTest&) = delete;
  HasEnrolledInstrumentPaymentHandlerTest& operator=(
      const HasEnrolledInstrumentPaymentHandlerTest&) = delete;

  ~HasEnrolledInstrumentPaymentHandlerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       FalseWithoutPaymentHandler) {
  ExpectHasEnrolledInstrumentIs(false);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       FalseWithPaymentHandlerAvailableToInstallJustInTime) {
  std::string payment_method =
      https_server()->GetURL("nickpay.com", "/nickpay.com/pay").spec();

  ExpectHasEnrolledInstrumentIs(false, payment_method);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       TrueWithInstalledPaymentHandler) {
  std::string payment_method;
  InstallPaymentApp("nickpay.com", "/nickpay.com/app.js", &payment_method);
  NavigateTo("/has_enrolled_instrument.html");

  ExpectHasEnrolledInstrumentIs(true, payment_method);
}

}  // namespace
}  // namespace payments
