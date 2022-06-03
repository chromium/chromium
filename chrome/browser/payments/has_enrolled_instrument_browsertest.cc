// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/personal_data_manager_test_util.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class HasEnrolledInstrumentTest : public PaymentRequestPlatformBrowserTestBase {
 public:
  HasEnrolledInstrumentTest() = default;

  HasEnrolledInstrumentTest(const HasEnrolledInstrumentTest&) = delete;
  HasEnrolledInstrumentTest& operator=(const HasEnrolledInstrumentTest&) =
      delete;

  ~HasEnrolledInstrumentTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/has_enrolled_instrument.html");
  }

  // Helper function to test that all variations of hasEnrolledInstrument()
  // returns |expected|.
  void ExpectHasEnrolledInstrumentIs(bool expected) {
    EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(),
                                        "hasEnrolledInstrument()"));
    EXPECT_EQ(expected,
              content::EvalJs(GetActiveWebContents(),
                              "hasEnrolledInstrument({requestShipping:true})"));
    EXPECT_EQ(expected, content::EvalJs(
                            GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoCard) {
  ExpectHasEnrolledInstrumentIs(false);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoBillingAddress) {
  AddCreditCard(autofill::test::GetCreditCard());
  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveShippingNoBillingAddress) {
  CreateAndAddAutofillProfile();
  AddCreditCard(autofill::test::GetCreditCard());

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveShippingAndBillingAddress) {
  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  u"1111111111111111");
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(false);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetExpirationYear(2000);
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(true);
}

// TODO(https://crbug.com/994799): Unify autofill data validation and returned
// data across platforms.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
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

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest,
                       HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentTest, InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     u"this-is-not-a-valid-email-address");
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(true);
}

}  // namespace
}  // namespace payments
