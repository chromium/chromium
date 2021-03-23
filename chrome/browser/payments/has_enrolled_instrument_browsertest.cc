// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
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

enum HasEnrolledInstrumentMode {
  STRICT_HAS_ENROLLED_INSTRUMENT,
  LEGACY_HAS_ENROLLED_INSTRUMENT,
};

// A parameterized test to test both values of
// features::kStrictHasEnrolledAutofillInstrument.
class HasEnrolledInstrumentTest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<HasEnrolledInstrumentMode> {
 public:
  HasEnrolledInstrumentTest() {
    if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument},
          /*disabled_features=*/{features::kPaymentRequestSkipToGPay});
    }
  }

  ~HasEnrolledInstrumentTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/has_enrolled_instrument.html");
  }

  std::string not_supported_message() const {
    return "NotSupportedError: The payment method \"basic-card\" is not "
           "supported. User does not have valid information on file.";
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

  // Helper function to test that all variants of show() rejects with
  // not_supported_message().
  void ExpectShowRejects() {
    // Only check show() if feature is on.
    if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
      base::HistogramTester histogram_tester;
      base::HistogramBase::Count expected_count =
          histogram_tester.GetBucketCount(
              "PaymentRequest.CheckoutFunnel.NoShow",
              JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);

      // Check code path where show() is called before instruments are ready.
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(), "show()"));
      // TODO(crbug.com/1027322): Fix NoShow logging on Android.
#if !defined(OS_ANDROID)
      expected_count++;
#endif
      histogram_tester.ExpectBucketCount(
          "PaymentRequest.CheckoutFunnel.NoShow",
          JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD,
          expected_count);
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "show({requestShipping:true})"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "show({requestPayerEmail:true})"));

      // Check code path where show() is called after instruments are ready.
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(), "delayedShow()"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "delayedShow({requestShipping:true})"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "delayedShow({requestPayerEmail:true})"));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HasEnrolledInstrumentTest);
};

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoCard) {
  ExpectHasEnrolledInstrumentIs(false);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoBillingAddress) {
  AddCreditCard(autofill::test::GetCreditCard());
  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveShippingNoBillingAddress) {
  CreateAndAddAutofillProfile();
  AddCreditCard(autofill::test::GetCreditCard());

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveShippingAndBillingAddress) {
  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  u"1111111111111111");
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(false);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);
  autofill::CreditCard card = CreatCreditCardForProfile(address);
  card.SetExpirationYear(2000);
  AddCreditCard(card);

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

// TODO(https://crbug.com/994799): Unify autofill data validation and returned
// data across platforms.
IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
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

  // Recipient name is required for shipping address in strict mode.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestShipping:true})"));
  }

  // Recipient name should be required for billing address in strict mode, but
  // current desktop implementation doesn't match this requirement.
  // TODO(https://crbug.com/994799): Unify autofill data requirements between
  // desktop and Android.
#if defined(OS_ANDROID)
  bool is_no_name_billing_address_valid =
      GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT;
#else
  bool is_no_name_billing_address_valid = true;
#endif

  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (!is_no_name_billing_address_valid) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(), "show()"));
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     std::u16string());
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  // StrictHasEnrolledAutofillInstrument considers a profile with missing email
  // address as invalid.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     u"this-is-not-a-valid-email-address");
  AddAutofillProfile(address);
  CreateAndAddCreditCardForProfile(address);

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));

  // StrictHasEnrolledAutofillInstrument considers a profile with missing email
  // address as invalid.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

// Run all tests with both values for
// features::kStrictHasEnrolledAutofillInstrument.
INSTANTIATE_TEST_SUITE_P(All,
                         HasEnrolledInstrumentTest,
                         ::testing::Values(STRICT_HAS_ENROLLED_INSTRUMENT,
                                           LEGACY_HAS_ENROLLED_INSTRUMENT));
}  // namespace
}  // namespace payments
