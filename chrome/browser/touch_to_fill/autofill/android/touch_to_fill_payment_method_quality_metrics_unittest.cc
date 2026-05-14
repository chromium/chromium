// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/touch_to_fill/autofill//android/touch_to_fill_delegate_android_impl.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::NiceMock;
using ::testing::TestWithParam;

struct TouchToFillForPaymentMethodsTestCase {
  bool is_all_autofilled;
  bool is_all_accepted;
};

class TouchToFillForPaymentMethodsTest
    : public AutofillMetricsBaseTest,
      public TestWithParam<TouchToFillForPaymentMethodsTestCase> {
 public:
  void SetUp() override {
    SetUpHelper();
    ON_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
        .WillByDefault(testing::Return(true));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<TouchToFillDelegateAndroidImpl>(&autofill_manager()));
    autofill_client().set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
  }

  void TearDown() override { TearDownHelper(); }

  TouchToFillDelegateAndroidImpl& touch_to_fill_delegate() {
    return *static_cast<TouchToFillDelegateAndroidImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return *static_cast<MockPaymentsAutofillClient*>(
        autofill_client().GetPaymentsAutofillClient());
  }
};

// Tests that filling correctness and perfect filling are logged correctly on
// various situations.
TEST_P(TouchToFillForPaymentMethodsTest,
       AllAutofilledAndAccepted_TouchToFill_CreditCards) {
  TouchToFillForPaymentMethodsTestCase test_case = GetParam();

  // Add a credit card without a CVC.
  test_paydm().ClearCreditCards();
  CreditCard credit_card = test::GetCreditCard();
  paydm().AddCreditCard(credit_card);

  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL},
                                    {.role = CREDIT_CARD_NUMBER},
                                    {.role = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                                    {.role = CREDIT_CARD_VERIFICATION_CODE}}});
  SeeForm(form);

  // Simulate suggestion triggering and user selection in the bottom sheet.
  autofill_manager().OnAskForValuesToFillTest(
      form, form.fields()[0].global_id(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  touch_to_fill_delegate().CreditCardSuggestionSelected(
      /*unique_id=*/credit_card.guid(),
      /*is_virtual=*/false);
  touch_to_fill_delegate().OnDismissed(/*dismissed_by_user=*/false,
                                       /*should_reshow=*/false);

  if (!test_case.is_all_autofilled) {
    // Simulate manually filling the empty CVC field.
    SimulateUserChangedField(form, form.fields()[3]);
  }
  if (!test_case.is_all_accepted) {
    // Simulate editing the autofilled CC_NAME field.
    SimulateUserChangedField(form, form.fields()[0]);
  }

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.TouchToFill.CreditCard.PerfectFilling"),
      BucketsAre(
          Bucket(test_case.is_all_autofilled && test_case.is_all_accepted, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.TouchToFill.CreditCard.FillingCorrectness"),
              BucketsAre(Bucket(test_case.is_all_accepted, 1)));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTest,
    TouchToFillForPaymentMethodsTest,
    testing::Values(
        // All autofilled and nothing edited manually.
        TouchToFillForPaymentMethodsTestCase{/*is_all_autofilled=*/true,
                                             /*is_all_accepted=*/true},
        // All autofilled and something edited manually.
        TouchToFillForPaymentMethodsTestCase{/*is_all_autofilled=*/true,
                                             /*is_all_accepted=*/false},
        // Not all autofilled and nothing edited manually.
        TouchToFillForPaymentMethodsTestCase{/*is_all_autofilled=*/false,
                                             /*is_all_accepted=*/true},
        // Not all autofilled and something edited manually.
        TouchToFillForPaymentMethodsTestCase{/*is_all_autofilled=*/false,
                                             /*is_all_accepted=*/false}));

}  // namespace

}  // namespace autofill::autofill_metrics
