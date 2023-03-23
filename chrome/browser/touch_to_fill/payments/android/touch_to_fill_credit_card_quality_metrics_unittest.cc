// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/touch_to_fill/payments//android/touch_to_fill_delegate_android_impl.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::NiceMock;
using ::testing::TestWithParam;

namespace autofill::autofill_metrics {

class MockFastCheckoutClient : public FastCheckoutClient {
 public:
  MockFastCheckoutClient() = default;
  ~MockFastCheckoutClient() override = default;
  MOCK_METHOD(bool,
              TryToStart,
              (const GURL&,
               const autofill::FormData&,
               const autofill::FormFieldData&,
               base::WeakPtr<autofill::AutofillManager>),
              (override));
  MOCK_METHOD(void, Stop, (bool), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(void, OnNavigation, (const GURL&, bool), (override));
  MOCK_METHOD(bool,
              IsSupported,
              (const autofill::FormData&,
               const autofill::FormFieldData&,
               const autofill::AutofillManager&),
              (override));
  MOCK_METHOD(bool, IsNotShownYet, (), (const, override));
};

struct TouchToFillForCreditCardsTestCase {
  std::vector<ServerFieldType> field_types;
  std::vector<bool> fields_have_autofilled_values;
  bool is_all_autofilled;
  bool is_all_accepted;
};

class TouchToFillForCreditCardsTest
    : public AutofillMetricsBaseTest,
      public TestWithParam<TouchToFillForCreditCardsTestCase> {
 public:
  void SetUp() override {
    SetUpHelper();
    ON_CALL(*autofill_client_, ShowTouchToFillCreditCard)
        .WillByDefault(testing::Return(true));
    ON_CALL(*autofill_client_, IsTouchToFillCreditCardSupported)
        .WillByDefault(testing::Return(true));
    ON_CALL(fast_checkout_client_, IsNotShownYet)
        .WillByDefault(testing::Return(true));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<TouchToFillDelegateImpl>(&autofill_manager(),
                                                  &fast_checkout_client_));
  }

  void TearDown() override { TearDownHelper(); }

  // Generates credit card's fields for testing by the fields' types given.
  std::vector<FormFieldData> GetFields(
      std::vector<ServerFieldType> field_types) {
    std::vector<FormFieldData> fields_to_return;
    fields_to_return.reserve(field_types.size());
    for (const auto& type : field_types) {
      switch (type) {
        case CREDIT_CARD_NAME_FULL:
          fields_to_return.emplace_back(
              CreateField("Name on card", "cardName", "", "text"));
          break;
        case CREDIT_CARD_NUMBER:
          fields_to_return.emplace_back(
              CreateField("Credit card number", "cardNumber", "", "text"));
          break;
        case CREDIT_CARD_EXP_MONTH:
          fields_to_return.push_back(
              CreateField("Expiration date", "cc_exp", "", "text"));
          break;
        case CREDIT_CARD_VERIFICATION_CODE:
          fields_to_return.emplace_back(CreateField("CVC", "CVC", "", "text"));
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    return fields_to_return;
  }

  // Simulate autofilling the form with the credit card data.
  void SetFieldsAutofilledValues(
      FormData& form,
      const std::vector<bool>& fields_have_autofilled_values,
      const std::vector<ServerFieldType>& server_field_types) {
    ASSERT_EQ(form.fields.size(), fields_have_autofilled_values.size());
    ASSERT_EQ(form.fields.size(), server_field_types.size());
    for (size_t i = 0; i < fields_have_autofilled_values.size(); i++) {
      form.fields[i].is_autofilled = fields_have_autofilled_values[i];
      CreditCard test_card = test::GetCreditCard();
      form.fields[i].value =
          server_field_types[i] != CREDIT_CARD_VERIFICATION_CODE
              ? test_card.GetRawInfo(server_field_types[i])
              : u"123";
    }
  }

  TouchToFillDelegateImpl& touch_to_fill_delegate() {
    return *static_cast<TouchToFillDelegateImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

 private:
  NiceMock<MockFastCheckoutClient> fast_checkout_client_;
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
TEST_P(TouchToFillForCreditCardsTest,
       AllAutofilledAndAccepted_TouchToFill_CreditCards) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*include_full_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  TouchToFillForCreditCardsTestCase test_case = GetParam();
  FormData form = CreateForm(GetFields(test_case.field_types));

  SeeForm(form);
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0], {},
                                              AutoselectFirstSuggestion(false),
                                              FormElementWasClicked(true));

  base::HistogramTester histogram_tester;
  // Simulate user selection in the payments bottom sheet.
  touch_to_fill_delegate().SuggestionSelected(/*unique_id=*/kTestLocalCardId,
                                              /*is_virtual=*/false);
  touch_to_fill_delegate().OnDismissed(/*dismissed_by_user=*/false);
  // Simulate that fields were autofilled.
  SetFieldsAutofilledValues(form, test_case.fields_have_autofilled_values,
                            test_case.field_types);
  // Simulate user made change to autofilled field.
  if (!test_case.is_all_accepted) {
    SimulateUserChangedTextField(form, form.fields[0]);
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
    TouchToFillForCreditCardsTest,
    testing::Values(
        // All autofilled and nothing edited manually
        TouchToFillForCreditCardsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH},
            /*fields_is_autofilled_values=*/{true, true, true},
            /*is_all_autofilled=*/true,
            /*is_all_accepted=*/true},
        // Not all autofilled and nothing edited manually
        TouchToFillForCreditCardsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
             CREDIT_CARD_VERIFICATION_CODE},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/true},
        // Not all autofilled and something edited manually
        TouchToFillForCreditCardsTestCase{
            {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
             CREDIT_CARD_VERIFICATION_CODE},
            /*fields_is_autofilled_values=*/{true, true, true, false},
            /*is_all_autofilled=*/false,
            /*is_all_accepted=*/false}));

}  // namespace autofill::autofill_metrics
