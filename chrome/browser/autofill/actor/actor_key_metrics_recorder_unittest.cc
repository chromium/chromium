// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_key_metrics_recorder.h"

#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service_impl.h"
#include "chrome/browser/autofill/actor/actor_test_utils.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;

using GetSuggestionsFuture =
    base::test::TestFuture<base::expected<std::vector<ActorFormFillingRequest>,
                                          ActorFormFillingError>>;
using FillSuggestionsFuture =
    base::test::TestFuture<base::expected<void, ActorFormFillingError>>;

// Returns a fill-request with a non-sensical (because null) field id.
ActorFormFillingService::FillRequest UnfindableFillRequest() {
  return {ActorFormFillingRequest::RequestedData::kAddress, {FieldGlobalId()}};
}

ActorFormFillingService::FillRequest AddressFillRequest(
    std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kAddress,
          std::move(field_ids)};
}

ActorFormFillingService::FillRequest CreditCardFillRequest(
    std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kCreditCard,
          std::move(field_ids)};
}

class ActorKeyMetricsRecorderTest : public ActorTestBase {
 public:
  void SetUp() override {
    ActorTestBase::SetUp();
    client()
        .GetPaymentsAutofillClient()
        ->set_multiple_request_payments_network_interface(
            std::make_unique<
                payments::MockMultipleRequestPaymentsNetworkInterface>(
                client().GetURLLoaderFactory(),
                *client().GetIdentityManager()));
  }
};

// Tests that FillingAssistance metrics are correctly recorded when the actor
// fills an address form.
TEST_F(ActorKeyMetricsRecorderTest, FillingAssistanceMetrics_AddressFilled) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingAssistance.Address", true, 1);
}

// Tests that FillingAssistance metrics are correctly recorded as false when the
// actor does not fill an address form that the user was capable of filling.
TEST_F(ActorKeyMetricsRecorderTest, FillingAssistanceMetrics_AddressNotFilled) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  // Trigger suggestions to ensure the manager is observed.
  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());

  // Do NOT fill.

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingAssistance.Address", false, 1);
}

// Tests that FillingAssistance metrics are correctly recorded when the actor
// fills a credit card form.
TEST_F(ActorKeyMetricsRecorderTest, FillingAssistanceMetrics_CreditCardFilled) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(fill_future.Get(), HasValue());

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingAssistance.CreditCard", true, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded as true when all
// fields filled by the actor are submitted unchanged.
TEST_F(ActorKeyMetricsRecorderTest, FillingCorrectnessMetrics_AddressCorrect) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Simulate all fields being submitted as autofilled (unchanged).
  std::vector<FormFieldData> fields = form.ExtractFields();
  for (auto& field : fields) {
    field.set_is_autofilled_according_to_renderer(true);
  }
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.Address", true, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded as false when at
// least one field filled by the actor is modified by the user.
TEST_F(ActorKeyMetricsRecorderTest,
       FillingCorrectnessMetrics_AddressIncorrect) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Simulate one field being modified.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(false);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                    base::TimeTicks::Now());
  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.Address", false, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded for credit cards
// if all fields are submitted unchanged.
TEST_F(ActorKeyMetricsRecorderTest,
       FillingCorrectnessMetrics_CreditCardCorrect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Simulate all fields being submitted as autofilled.
  std::vector<FormFieldData> fields = form.ExtractFields();
  for (auto& field : fields) {
    field.set_is_autofilled_according_to_renderer(true);
  }
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.CreditCard", true, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded for credit cards
// if one field is modified by the user.
TEST_F(ActorKeyMetricsRecorderTest,
       FillingCorrectnessMetrics_CreditCardIncorrect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Simulate one field being modified.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[1].set_is_autofilled_according_to_renderer(false);
  form.set_fields(std::move(fields));

  manager().OnTextFieldValueChanged(form, form.fields()[1].global_id(),
                                    base::TimeTicks::Now());
  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.CreditCard", false, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded for both
// products in a mixed form.
TEST_F(ActorKeyMetricsRecorderTest, FillingCorrectnessMetrics_MixedForm) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = CREDIT_CARD_NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  // Fill address.
  GetSuggestionsFuture addr_future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           addr_future.GetCallback());
  std::vector<ActorFormFillingRequest> addr_requests =
      addr_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(addr_requests[0].suggestions[0].id));
  FillSuggestionsFuture addr_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(addr_requests[0].suggestions[0].id)},
      addr_fill_future.GetCallback());
  EXPECT_THAT(addr_fill_future.Get(), HasValue());

  // Fill credit card.
  GetSuggestionsFuture cc_future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[2].global_id()})},
      cc_future.GetCallback());
  std::vector<ActorFormFillingRequest> cc_requests = cc_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(cc_requests[0].suggestions[0].id));
  FillSuggestionsFuture cc_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(cc_requests[0].suggestions[0].id)},
      cc_fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(cc_fill_future.Get(), HasValue());

  // Simulate address being modified, but CC remains unchanged.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(false);
  fields[1].set_is_autofilled_according_to_renderer(true);
  fields[2].set_is_autofilled_according_to_renderer(true);
  fields[3].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                    base::TimeTicks::Now());
  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.Address", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.CreditCard", true, 1);
}

// Tests that FillingCorrectness metrics are correctly recorded as true when the
// actor only filled some fields, and the user manually filled the rest (but did
// not modify the actor-filled ones).
TEST_F(ActorKeyMetricsRecorderTest, FillingCorrectnessMetrics_PartialFilling) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm(
      {.fields = {{.server_type = NAME_FULL}, {.server_type = UNKNOWN_TYPE}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Verify that only the NAME_FULL field was filled by the actor.
  EXPECT_THAT(last_filled_values(),
              AllOf(Contains(Pair(form.fields()[0].global_id(), _)),
                    Not(Contains(Pair(form.fields()[1].global_id(), _)))));

  // Simulate submission:
  // - Field 0 (NAME_FULL): Filled by actor, unchanged (is_autofilled = true).
  // - Field 1 (UNKNOWN_TYPE): Filled by user manually (is_autofilled = false).
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(false);
  fields[1].set_value(u"User Content");
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  // Should be recorded as Correct (true) because the actor-filled field was not
  // modified.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingCorrectness.Address", true, 1);
}

// Tests that the state for recorded forms is cleared when the forms are
// removed (e.g. on navigation).
TEST_F(ActorKeyMetricsRecorderTest,
       FillingAssistanceMetrics_StateClearedOnNavigation) {
  base::HistogramTester histogram_tester;
  // Use stable IDs so that the form has the same GlobalId when re-added.
  test::FormDescription form_desc = {
      .fields = {{.role = NAME_FULL, .renderer_id = FieldRendererId(1)},
                 {.role = ADDRESS_HOME_LINE1,
                  .renderer_id = FieldRendererId(2)}},
      .host_frame = driver().GetFrameToken(),
      .renderer_id = FormRendererId(1)};
  FormData form = SeeForm(form_desc);

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingAssistance.Address", true, 1);

  // Now "navigate" away, which removes forms.
  manager().OnFormsSeen(/*updated_forms=*/{},
                        /*removed_forms=*/{form.global_id()});

  // Re-add the same form (simulating coming back to the page).
  FormData form2 = SeeForm(form_desc);
  ASSERT_EQ(form.global_id(), form2.global_id());

  // Record at submission. Should record false now because it's a new session
  // and it wasn't filled by actor in this session.
  manager().OnFormSubmitted(form2, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.KeyMetrics.FillingAssistance.Address", false, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.Actor.KeyMetrics.FillingAssistance.Address", 2);
}

// Tests that FillingReadiness metrics are correctly recorded when actor
// suggestions are available.
TEST_F(ActorKeyMetricsRecorderTest,
       FillingReadinessMetrics_SuggestionsAvailable) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingReadiness.Address", true, 1);
}

// Tests that FillingReadiness metrics are correctly recorded when actor
// suggestions are NOT available.
TEST_F(ActorKeyMetricsRecorderTest,
       FillingReadinessMetrics_SuggestionsNotAvailable) {
  base::HistogramTester histogram_tester;
  // Use a form type that won't have suggestions (e.g. by using an unfindable
  // request or similar, but SeeForm needs to be called to cache it).
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL}}});

  GetSuggestionsFuture future;
  // Trigger on a field that doesn't exist in any form to fail suggestion
  // generation.
  service().GetSuggestions(tab(), {UnfindableFillRequest()},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoSuggestions));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingReadiness.Address", false, 1);
}

// Tests that FillingReadiness metrics are correctly recorded for credit cards.
TEST_F(ActorKeyMetricsRecorderTest, FillingReadinessMetrics_CreditCard) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.KeyMetrics.FillingReadiness.CreditCard", true, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as true when all
// fields filled by the actor are submitted unchanged.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_Address_Perfect) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate perfect filling (no user edits).
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      true, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as false when at
// least one field filled by the actor is modified by the user.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_Address_Imperfect) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate imperfect filling (user edit).
  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                    base::TimeTicks::Now());

  // Simulate submission.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(false);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      false, 1);
}

// Tests that PerfectFilling metrics are correctly recorded for credit cards
// if all fields are submitted unchanged.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_CreditCard_Perfect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate perfect filling.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.PerfectFilling.CreditCard", true, 1);
}

// Tests that PerfectFilling metrics are correctly recorded for credit cards
// if one field is modified by the user.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_CreditCard_Imperfect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate imperfect filling.
  manager().OnTextFieldValueChanged(form, form.fields()[1].global_id(),
                                    base::TimeTicks::Now());

  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[1].set_is_autofilled_according_to_renderer(false);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.PerfectFilling.CreditCard", false, 1);
}

// Tests that PerfectFilling metrics are correctly recorded for both products
// in a mixed form if all fields are submitted unchanged.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_MixedForm_Perfect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  // Fill address.
  GetSuggestionsFuture addr_future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           addr_future.GetCallback());
  std::vector<ActorFormFillingRequest> addr_requests =
      addr_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(addr_requests[0].suggestions[0].id));
  FillSuggestionsFuture addr_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(addr_requests[0].suggestions[0].id)},
      addr_fill_future.GetCallback());
  ASSERT_THAT(addr_fill_future.Get(), HasValue());

  // Fill credit card.
  GetSuggestionsFuture cc_future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[1].global_id()})},
      cc_future.GetCallback());
  std::vector<ActorFormFillingRequest> cc_requests = cc_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(cc_requests[0].suggestions[0].id));
  FillSuggestionsFuture cc_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(cc_requests[0].suggestions[0].id)},
      cc_fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  ASSERT_THAT(cc_fill_future.Get(), HasValue());

  // Perfect filling.
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.PerfectFilling.CreditCard", true, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as false for both
// products in a mixed form, if one field is modified by the user.
TEST_F(ActorKeyMetricsRecorderTest, PerfectFilling_MixedForm_Imperfect) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetCreditCard();
  payments_data_manager().AddCreditCard(card);
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  // Fill address.
  GetSuggestionsFuture addr_future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           addr_future.GetCallback());
  std::vector<ActorFormFillingRequest> addr_requests =
      addr_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(addr_requests[0].suggestions[0].id));
  FillSuggestionsFuture addr_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(addr_requests[0].suggestions[0].id)},
      addr_fill_future.GetCallback());
  ASSERT_THAT(addr_fill_future.Get(), HasValue());

  // Fill credit card.
  GetSuggestionsFuture cc_future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[1].global_id()})},
      cc_future.GetCallback());
  std::vector<ActorFormFillingRequest> cc_requests = cc_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(cc_requests[0].suggestions[0].id));
  FillSuggestionsFuture cc_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(cc_requests[0].suggestions[0].id)},
      cc_fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  ASSERT_THAT(cc_fill_future.Get(), HasValue());

  // Imperfect filling (address field edited).
  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                    base::TimeTicks::Now());

  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(false);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  // Both should be false because the form as a whole is imperfect.
  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.PerfectFilling.CreditCard", false, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as false when the
// actor did not fill all fields and the user manually filled the remaining one.
TEST_F(ActorKeyMetricsRecorderTest,
       PerfectFilling_Address_PartialFilling_ManualFallback) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY},
                                      {.server_type = UNKNOWN_TYPE}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate submission.
  std::vector<FormFieldData> fields = form.ExtractFields();
  // First 3 filled by actor (unchanged).
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  fields[2].set_is_autofilled_according_to_renderer(true);
  // 4th filled manually.
  fields[3].set_is_autofilled_according_to_renderer(false);
  fields[3].set_value(u"User Content");
  form.set_fields(std::move(fields));

  manager().OnTextFieldValueChanged(form, form.fields()[3].global_id(),
                                    base::TimeTicks::Now());
  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      false, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as false when the
// actor partially fills a form, and the user uses standard (non-actor) Autofill
// to complete the rest.
TEST_F(ActorKeyMetricsRecorderTest,
       PerfectFilling_PartialFilling_StandardAutofillFallback) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = CREDIT_CARD_NUMBER}}});

  // Fill ONLY the address via Actor.
  GetSuggestionsFuture addr_future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           addr_future.GetCallback());
  std::vector<ActorFormFillingRequest> addr_requests =
      addr_future.Take().value();
  service().FillForm(
      tab(), /*form_index=*/0,
      ActorFormFillingSelection(addr_requests[0].suggestions[0].id));
  FillSuggestionsFuture addr_fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(addr_requests[0].suggestions[0].id)},
      addr_fill_future.GetCallback());
  ASSERT_THAT(addr_fill_future.Get(), HasValue());

  // Manually add the kAutofill modifier to the cached field to perfectly
  // simulate standard Autofill stepping in where the Actor left off.
  manager()
      .FindCachedFormById(form.global_id())
      ->fields()[1]
      ->AddFieldModifier(FieldModifier::kAutofill);

  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  // Should fail because the CC field has an autofill modifier, but wasn't
  // filled by any Actor.
  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      false, 1);
}

// Tests that PerfectFilling metrics are correctly recorded as true when the
// actor fills its relevant fields, and the remaining fields are left completely
// empty (or untouched by the user/autofill).
TEST_F(ActorKeyMetricsRecorderTest,
       PerfectFilling_Address_WithEmptyIgnoredFields) {
  base::HistogramTester histogram_tester;
  // Form has an extra unknown field that will just be left alone.
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = UNKNOWN_TYPE}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_THAT(fill_future.Get(), HasValue());

  // Simulate submission. The actor fields are marked as autofilled.
  // The UNKNOWN_TYPE field is completely untouched (empty modifiers).
  std::vector<FormFieldData> fields = form.ExtractFields();
  fields[0].set_is_autofilled_according_to_renderer(true);
  fields[1].set_is_autofilled_according_to_renderer(true);
  fields[2].set_is_autofilled_according_to_renderer(false);  // Left empty
  form.set_fields(std::move(fields));

  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  // Should succeed because the untouched empty field does not penalize the
  // metric.
  histogram_tester.ExpectUniqueSample("Autofill.Actor.PerfectFilling.Address",
                                      true, 1);
}

}  // namespace
}  // namespace autofill
