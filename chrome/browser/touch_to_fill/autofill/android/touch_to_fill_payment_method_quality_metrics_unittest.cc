// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/touch_to_fill/autofill//android/touch_to_fill_delegate_android_impl.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using ::base::Bucket;
using ::base::BucketsAre;
using test::CreateTestFormField;
using ::testing::NiceMock;
using ::testing::TestWithParam;

struct TouchToFillForPaymentMethodsTestCase {
  std::vector<FieldType> field_types;
  std::vector<bool> fields_have_autofilled_values;
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
    MockFastCheckoutClient* fast_checkout_client =
        static_cast<MockFastCheckoutClient*>(
            autofill_client_->GetFastCheckoutClient());
    ON_CALL(*fast_checkout_client, IsNotShownYet)
        .WillByDefault(testing::Return(true));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<TouchToFillDelegateAndroidImpl>(&autofill_manager()));
  }

  void TearDown() override { TearDownHelper(); }

  // Generates credit card's fields for testing by the fields' types given.
  std::vector<FormFieldData> GetFields(std::vector<FieldType> field_types) {
    std::vector<FormFieldData> fields_to_return;
    fields_to_return.reserve(field_types.size());
    for (const auto& type : field_types) {
      switch (type) {
        case CREDIT_CARD_NAME_FULL:
          fields_to_return.emplace_back(CreateTestFormField(
              "Name on card", "cardName", "", FormControlType::kInputText));
          break;
        case CREDIT_CARD_NUMBER:
          fields_to_return.emplace_back(
              CreateTestFormField("Credit card number", "cardNumber", "",
                                  FormControlType::kInputText));
          break;
        case CREDIT_CARD_EXP_MONTH:
          fields_to_return.push_back(CreateTestFormField(
              "Expiration date", "cc_exp", "", FormControlType::kInputText));
          break;
        case CREDIT_CARD_VERIFICATION_CODE:
          fields_to_return.emplace_back(CreateTestFormField(
              "CVC", "CVC", "", FormControlType::kInputText));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
    return fields_to_return;
  }

  // Simulate autofilling the form with the credit card data.
  void SetFieldsAutofilledValues(
      FormData& form,
      const std::vector<bool>& fields_have_autofilled_values,
      const std::vector<FieldType>& field_types) {
    ASSERT_EQ(form.fields().size(), fields_have_autofilled_values.size());
    ASSERT_EQ(form.fields().size(), field_types.size());
    for (size_t i = 0; i < fields_have_autofilled_values.size(); i++) {
      test_api(form).field(i).set_is_autofilled(
          fields_have_autofilled_values[i]);
      CreditCard test_card = test::GetCreditCard();
      test_api(form).field(i).set_value(
          field_types[i] != CREDIT_CARD_VERIFICATION_CODE
              ? test_card.GetRawInfo(field_types[i])
              : u"123");
    }
  }

  TouchToFillDelegateAndroidImpl& touch_to_fill_delegate() {
    return *static_cast<TouchToFillDelegateAndroidImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return *static_cast<MockPaymentsAutofillClient*>(
        autofill_client_->GetPaymentsAutofillClient());
  }
};

// The test workflow:
// - Test credit cards are created and saved simulating a user having some saved
// credit cards.
// - Autofill sees the credit card form and asks for the values to fill.
// - Simulates card selection in the payments bottom sheet.
// - Simulats autofilling the form.
// - Simulates user manually changing a field (if needed depending on the test
// parameters).
// - Submits the form.
// - The perfect filling and filling correctness metrics are expected to be
// logged correctly here.
TEST_P(TouchToFillForPaymentMethodsTest,
       AllAutofilledAndAccepted_TouchToFill_CreditCards) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  TouchToFillForPaymentMethodsTestCase test_case = GetParam();
  FormData form = CreateForm(GetFields(test_case.field_types));

  SeeForm(form);
  autofill_manager().OnAskForValuesToFillTest(
      form, form.fields()[0].global_id(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  base::HistogramTester histogram_tester;
  // Simulate user selection in the payments bottom sheet.
  touch_to_fill_delegate().CreditCardSuggestionSelected(
      /*unique_id=*/kTestLocalCardId,
      /*is_virtual=*/false);
  touch_to_fill_delegate().OnDismissed(/*dismissed_by_user=*/false);
  // Simulate that fields were autofilled.
  SetFieldsAutofilledValues(form, test_case.fields_have_autofilled_values,
                            test_case.field_types);
  // Simulate user made change to autofilled field.
  if (!test_case.is_all_accepted) {
    SimulateUserChangedTextField(form, form.fields()[0]);
  }

  SubmitForm(form);
  Bucket expected_perfect_fill =
      Bucket(test_case.is_all_autofilled && test_case.is_all_accepted, 1);
  Bucket expected_filling_correctness = Bucket(test_case.is_all_accepted, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.TouchToFill.CreditCard.PerfectFilling"),
              BucketsAre(expected_perfect_fill));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.TouchToFill.CreditCard.FillingCorrectness"),
              BucketsAre(expected_filling_correctness));
}

// Test cases for checking that perfect filling and filling correctness metrics
// are correctly logged for the payment bottom sheet.
INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsTest,
    TouchToFillForPaymentMethodsTest,
    testing::Values(
        // All autofilled and nothing edited manually
        TouchToFillForPaymentMethodsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH},
            /*fields_is_autofilled_values=*/{true, true, true},
            /*is_all_autofilled=*/true,
            /*is_all_accepted=*/true},
        // Not all autofilled and nothing edited manually
        TouchToFillForPaymentMethodsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
             CREDIT_CARD_VERIFICATION_CODE},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/true},
        // Not all autofilled and something edited manually
        TouchToFillForPaymentMethodsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
             CREDIT_CARD_VERIFICATION_CODE},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/false}));

}  // namespace autofill::autofill_metrics
