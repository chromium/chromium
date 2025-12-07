// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_filling_service.h"

#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/glic/actor_filling_observer.h"
#include "chrome/browser/autofill/glic/actor_form_filling_service_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;

using FillRequest = ActorFormFillingService::FillRequest;
using GetSuggestionsFuture =
    base::test::TestFuture<base::expected<std::vector<ActorFormFillingRequest>,
                                          ActorFormFillingError>>;
using FillSuggestionsFuture =
    base::test::TestFuture<base::expected<void, ActorFormFillingError>>;

[[nodiscard]] Matcher<ActorSuggestion> NonEmptyActorSuggestion() {
  return AnyOf(Field(&ActorSuggestion::title, Not(IsEmpty())),
               Field(&ActorSuggestion::details, Not(IsEmpty())));
}

[[nodiscard]] Matcher<ActorFormFillingRequest> IsActorFormFillingRequest(
    ActorFormFillingRequest::RequestedData requested_data) {
  return AllOf(Field(&ActorFormFillingRequest::requested_data, requested_data),
               Field(&ActorFormFillingRequest::suggestions,
                     Each(NonEmptyActorSuggestion())));
}

// Returns a fill-request with a non-sensical (because null) field id.
FillRequest UnfindableFillRequest() {
  return {ActorFormFillingRequest::RequestedData::
              FormFillingRequest_RequestedData_ADDRESS,
          {FieldGlobalId()}};
}

FillRequest AddressFillRequest(std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::
              FormFillingRequest_RequestedData_ADDRESS,
          std::move(field_ids)};
}

FillRequest CreditCardFillRequest(std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::
              FormFillingRequest_RequestedData_CREDIT_CARD,
          std::move(field_ids)};
}

FillRequest ContactInformationFillRequest(
    std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::
              FormFillingRequest_RequestedData_CONTACT_INFORMATION,
          std::move(field_ids)};
}

// Returns the value that `group` would fill into a field with a certain `type`.
std::u16string GetFillValue(const FormGroup& group, FieldType type) {
  return group.GetInfo(AutofillType(FieldTypeSet({type})), "en-us");
}

constexpr ActorFormFillingError kActorFormFillingSuccessForMetrics =
    static_cast<ActorFormFillingError>(0);

void ExpectGetSuggestionsOutcome(ActorFormFillingError error,
                                 base::HistogramTester& histogram_tester,
                                 const base::Location& location = FROM_HERE) {
  histogram_tester.ExpectUniqueSample("Autofill.Actor.GetSuggestions.Outcome",
                                      error, 1, location);
  histogram_tester.ExpectTotalCount("Autofill.Actor.GetSuggestions.Latency", 1,
                                    location);
}

void ExpectFillSuggestionsOutcome(bool is_payments_fill,
                                  ActorFormFillingError error,
                                  base::HistogramTester& histogram_tester,
                                  const base::Location& location = FROM_HERE) {
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.FillSuggestions.Any.Outcome", error, 1, location);
  histogram_tester.ExpectTotalCount(
      "Autofill.Actor.FillSuggestions.Any.Latency", 1, location);
  if (is_payments_fill) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.FillSuggestions.WithPaymentInformation.Outcome", error,
        1, location);
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.FillSuggestions.WithPaymentInformation.Latency", 1,
        location);
  } else {
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.FillSuggestions.WithoutPaymentInformation.Outcome",
        error, 1, location);
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.FillSuggestions.WithoutPaymentInformation.Latency", 01,
        location);
  }
}

class RecordingTestContentAutofillDriver : public TestContentAutofillDriver {
 public:
  using TestContentAutofillDriver::TestContentAutofillDriver;
  ~RecordingTestContentAutofillDriver() override = default;

  // TestContentAutofillDriver:
  base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> fields,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map,
      const Section& section_for_clear_form_on_ios) override {
    base::flat_set<FieldGlobalId> filled_fields =
        TestContentAutofillDriver::ApplyFormAction(
            action_type, action_persistence, fields, triggered_origin,
            field_type_map, section_for_clear_form_on_ios);
    for (const FormFieldData& field : fields) {
      if (filled_fields.contains(field.global_id())) {
        last_filled_values_[field.global_id()] = field.value();
      }
    }
    return filled_fields;
  }

  // Returns the values that this driver would have sent to the renderer for
  // filling.
  const absl::flat_hash_map<FieldGlobalId, std::u16string>& last_filled_values()
      const {
    return last_filled_values_;
  }

 private:
  absl::flat_hash_map<FieldGlobalId, std::u16string> last_filled_values_;
};

// A simple `CreditCardAccessManager` test class that allows intercepting the
// `FetchCreditCard` request.
class TestCreditCardAccessManager : public CreditCardAccessManager {
 public:
  explicit TestCreditCardAccessManager(BrowserAutofillManager* manager)
      : CreditCardAccessManager(manager) {}
  ~TestCreditCardAccessManager() override = default;

  void PrepareToFetchCreditCard() override {}

  void FetchCreditCard(const CreditCard*,
                       OnCreditCardFetchedCallback callback) override {
    callback_ = std::move(callback);
  }

  [[nodiscard]] bool RunCreditCardFetchedCallback(const CreditCard& card) {
    if (!callback_) {
      return false;
    }
    std::move(callback_).Run(card);
    return true;
  }

 private:
  OnCreditCardFetchedCallback callback_;
};

// A `TestBrowserAutofillManager` with a custom `CreditCardAccessManager`.
class TestBrowserAutofillManagerWithTestCCAM
    : public TestBrowserAutofillManager {
 public:
  explicit TestBrowserAutofillManagerWithTestCCAM(AutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {
    test_api(*this).set_credit_card_access_manager(
        std::make_unique<TestCreditCardAccessManager>(this));
  }
  ~TestBrowserAutofillManagerWithTestCCAM() override = default;

  void Reset() override {
    TestBrowserAutofillManager::Reset();
    test_api(*this).set_credit_card_access_manager(
        std::make_unique<TestCreditCardAccessManager>(this));
  }
};

class ActorFormFillingServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorFormFillingServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab, GetContents()).WillByDefault(Return(web_contents()));
    NavigateAndCommit(GURL("about:blank"));
    client().GetPersonalDataManager().address_data_manager().AddProfile(
        GetProfile1());
  }

  FormData SeeForm(test::FormDescription form_description) {
    FormData form = test::GetFormData(form_description);
    manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                          test::GetServerTypes(form_description));
    return form;
  }

 protected:
  TestContentAutofillClient& client() {
    return CHECK_DEREF(autofill_client_injector_[web_contents()]);
  }
  TestCreditCardAccessManager& credit_card_access_manager() {
    return CHECK_DEREF(static_cast<TestCreditCardAccessManager*>(
        manager().GetCreditCardAccessManager()));
  }
  payments::TestPaymentsAutofillClient& payments_client() {
    return CHECK_DEREF(client().GetPaymentsAutofillClient());
  }
  PaymentsDataManager& payments_data_manager() {
    return client().GetPersonalDataManager().payments_data_manager();
  }
  RecordingTestContentAutofillDriver& driver() {
    return CHECK_DEREF(autofill_driver_injector_[web_contents()]);
  }
  TestBrowserAutofillManager& manager() {
    return CHECK_DEREF(autofill_manager_injector_[web_contents()]);
  }
  ActorFormFillingService& service() { return service_; }
  tabs::TabInterface& tab() { return mock_tab; }

  // Returns an address that is available in `AddressDataManager`.
  AutofillProfile GetProfile1() { return test::GetFullProfile(); }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  tabs::MockTabInterface mock_tab;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<RecordingTestContentAutofillDriver>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManagerWithTestCCAM>
      autofill_manager_injector_;
  ActorFormFillingServiceImpl service_;
};

// Tests that a `kNoSuggestions` error is returned if we cannot find the form
// specified in the request.
TEST_F(ActorFormFillingServiceTest, UnfindableForm) {
  base::HistogramTester histogram_tester;
  GetSuggestionsFuture future;

  service().GetSuggestions(tab(), {UnfindableFillRequest()},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoSuggestions));

  ExpectGetSuggestionsOutcome(ActorFormFillingError::kNoSuggestions,
                              histogram_tester);
}

// Tests that a `kOther` error is returned if the fill request is empty.
TEST_F(ActorFormFillingServiceTest, EmptyFillRequest) {
  base::HistogramTester histogram_tester;
  GetSuggestionsFuture future;

  service().GetSuggestions(tab(), /*fill_requests=*/{}, future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kOther));

  ExpectGetSuggestionsOutcome(ActorFormFillingError::kOther, histogram_tester);
}

// Tests that a `kOther` error is returned if invalid request data is passed.
TEST_F(ActorFormFillingServiceTest, InvalidRequestData) {
  base::HistogramTester histogram_tester;
  GetSuggestionsFuture future;

  service().GetSuggestions(
      tab(), /*fill_requests=*/
      {FillRequest{static_cast<ActorFormFillingRequest::RequestedData>(234),
                   {FieldGlobalId()}}},
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kOther));

  ExpectGetSuggestionsOutcome(ActorFormFillingError::kOther, histogram_tester);
}

// Tests that a suggestion is returned when invoking on an address form and
// that the suggestion can be used for filling.
TEST_F(ActorFormFillingServiceTest, SimpleAddressForm) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_ADDRESS))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              Contains(std::pair(form.fields()[0].global_id(),
                                 GetFillValue(GetProfile1(), NAME_FULL))));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
  ExpectFillSuggestionsOutcome(/*is_payments_fill=*/false,
                               kActorFormFillingSuccessForMetrics,
                               histogram_tester);
}

// Tests that a suggestion is returned when invoking on a contact form and
// that the suggestion can be used for filling.
TEST_F(ActorFormFillingServiceTest, ContactInformationForm) {
  base::HistogramTester histogram_tester;
  FormData form =
      SeeForm({.fields = {{.server_type = NAME_FULL},
                          {.server_type = EMAIL_ADDRESS},
                          {.server_type = PHONE_HOME_WHOLE_NUMBER}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {ContactInformationFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_CONTACT_INFORMATION))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(
      driver().last_filled_values(),
      IsSupersetOf({std::pair(form.fields()[0].global_id(),
                              GetFillValue(GetProfile1(), NAME_FULL)),
                    std::pair(form.fields()[1].global_id(),
                              GetFillValue(GetProfile1(), EMAIL_ADDRESS))}));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
  ExpectFillSuggestionsOutcome(/*is_payments_fill=*/false,
                               kActorFormFillingSuccessForMetrics,
                               histogram_tester);
}

// Tests that a `CONTACT_INFORMATION` request on a mixed form still fills all
// address-related fields.
TEST_F(ActorFormFillingServiceTest, ContactInformationRequestOnMixedForm) {
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  // Trigger the request from a contact-specific field.
  service().GetSuggestions(
      tab(), {ContactInformationFillRequest({form.fields()[1].global_id()})},
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_CONTACT_INFORMATION))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());

  // Expect that all fields, including address fields, are filled.
  EXPECT_THAT(
      driver().last_filled_values(),
      IsSupersetOf(
          {std::pair(form.fields()[0].global_id(),
                     GetFillValue(GetProfile1(), NAME_FULL)),
           std::pair(form.fields()[1].global_id(),
                     GetFillValue(GetProfile1(), EMAIL_ADDRESS)),
           std::pair(form.fields()[2].global_id(),
                     GetFillValue(GetProfile1(), ADDRESS_HOME_LINE1))}));
}

// Tests that filling an "actor form" that is split across two Autofill forms
// works.
TEST_F(ActorFormFillingServiceTest, SplitAddressForm) {
  FormData form1 = SeeForm({.fields = {{.server_type = NAME_FIRST},
                                       {.server_type = NAME_LAST},
                                       {.server_type = EMAIL_ADDRESS},
                                       {.server_type = PHONE_HOME_NUMBER}}});
  FormData form2 = SeeForm({.fields = {{.server_type = ADDRESS_HOME_LINE1},
                                       {.server_type = ADDRESS_HOME_CITY},
                                       {.server_type = ADDRESS_HOME_ZIP},
                                       {.server_type = ADDRESS_HOME_COUNTRY}}});
  FieldGlobalId form_1_trigger_id = form1.fields()[0].global_id();
  FieldGlobalId form_2_trigger_id = form2.fields()[0].global_id();

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {AddressFillRequest({form_1_trigger_id, form_2_trigger_id})},
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_ADDRESS))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              IsSupersetOf({std::pair(form_1_trigger_id,
                                      GetFillValue(GetProfile1(), NAME_FIRST)),
                            std::pair(form_2_trigger_id,
                                      GetFillValue(GetProfile1(),
                                                   ADDRESS_HOME_LINE1))}));
}

// Tests that a suggestion generation and filling work with simple credit card
// forms.
TEST_F(ActorFormFillingServiceTest, SimpleCreditCardForm) {
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
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_CREDIT_CARD))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              Contains(std::pair(form.fields()[0].global_id(),
                                 GetFillValue(card, CREDIT_CARD_NAME_FULL))));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
  ExpectFillSuggestionsOutcome(/*is_payments_fill=*/true,
                               kActorFormFillingSuccessForMetrics,
                               histogram_tester);
}

// Tests that our suggestion generation simulates triggering on the credit card
// number field even if the triggering field passed by the model is a different
// one as long as kAutofillActorRewriteCreditCardTriggerField is enabled.
TEST_F(ActorFormFillingServiceTest, CreditCardFormWithNumberField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillActorRewriteCreditCardTriggerField);
  const CreditCard card = test::GetCreditCard();
  ASSERT_THAT(base::UTF16ToUTF8(card.number()), EndsWith("1111"));
  payments_data_manager().AddCreditCard(card);
  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  // The suggestion title should contain the last four digits of the credit card
  // number.
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(Field(
                  &ActorFormFillingRequest::suggestions,
                  Each(Field(&ActorSuggestion::title, HasSubstr("1111")))))));
}

class ActorFormFillingServiceWithOptimizationGuideTest
    : public ActorFormFillingServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsIframeUrlAllowlisted() const { return GetParam(); }
};

// Tests that we only switch the trigger field to a credit card number field of
// different origin if the origin of the number field is allowlisted.
TEST_P(ActorFormFillingServiceWithOptimizationGuideTest,
       CreditCardFormWithNumberFieldAndIframe) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillActorRewriteCreditCardTriggerField};
  const CreditCard card = test::GetCreditCard();
  ASSERT_THAT(base::UTF16ToUTF8(card.number()), EndsWith("1111"));
  payments_data_manager().AddCreditCard(card);

  const auto origin1 = url::Origin::Create(GURL("https://aaaa.com"));
  const auto origin2 = url::Origin::Create(GURL("https://bbbb.com"));
  FormData form = SeeForm(
      {.fields = {{.server_type = CREDIT_CARD_NAME_FULL, .origin = origin1},
                  {.server_type = CREDIT_CARD_NUMBER, .origin = origin2},
                  {.server_type = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                   .origin = origin2}}});

  EXPECT_CALL(*client().GetAutofillOptimizationGuideDecider(),
              IsIframeUrlAllowlistedForActor(origin2.GetURL()))
      .WillRepeatedly(Return(IsIframeUrlAllowlisted()));

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());

  // The suggestion title should contain the last four digits of the credit card
  // number only if the iframe origin is allowlisted.
  auto expected_result = IsIframeUrlAllowlisted()
                             ? Matcher<std::string>(HasSubstr("1111"))
                             : Matcher<std::string>(Not(HasSubstr("1111")));
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(Field(
                  &ActorFormFillingRequest::suggestions,
                  Each(Field(&ActorSuggestion::title, expected_result))))));
}

INSTANTIATE_TEST_SUITE_P(,
                         ActorFormFillingServiceWithOptimizationGuideTest,
                         ::testing::Bool());

// Tests that filling a credit card after fetching it from the server works.
TEST_F(ActorFormFillingServiceTest, FillAfterFetchingServerCard) {
  const CreditCard card = test::GetMaskedServerCard();
  payments_data_manager().AddCreditCard(card);
  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_CREDIT_CARD))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetMaximumTimeout(), base::Seconds(2));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(fill_future.IsReady());

  // Now we notify observers that a credit card fetch was started.
  using Observer = CreditCardAccessManager::Observer;
  test_api(credit_card_access_manager())
      .NotifyObservers(&Observer::OnCreditCardFetchStarted, card);
  task_environment()->FastForwardBy(ActorFillingObserver::GetMaximumTimeout() -
                                    base::Seconds(2));
  EXPECT_FALSE(fill_future.IsReady());

  // Simulate successful fetching.
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  test_api(credit_card_access_manager())
      .NotifyObservers(&Observer::OnCreditCardFetchSucceeded, card);

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fill_future.IsReady());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              Contains(std::pair(form.fields()[0].global_id(),
                                 GetFillValue(card, CREDIT_CARD_NAME_FULL))));
}

// Tests that even if a credit card fetch is ongoing, there is a timeout if
// fills do not complete after a minute.
TEST_F(ActorFormFillingServiceTest, TimeoutWithFetching) {
  base::HistogramTester histogram_tester;
  const CreditCard card = test::GetMaskedServerCard();
  payments_data_manager().AddCreditCard(card);
  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_CREDIT_CARD))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetMaximumTimeout(), base::Seconds(2));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(fill_future.IsReady());

  // Now we notify observers that a credit card fetch was started.
  test_api(credit_card_access_manager())
      .NotifyObservers(
          &CreditCardAccessManager::Observer::OnCreditCardFetchStarted, card);
  task_environment()->FastForwardBy(ActorFillingObserver::GetMaximumTimeout() -
                                    base::Seconds(2));
  EXPECT_FALSE(fill_future.IsReady());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fill_future.IsReady());
  EXPECT_THAT(fill_future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
  EXPECT_THAT(driver().last_filled_values(), IsEmpty());

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
  ExpectFillSuggestionsOutcome(/*is_payments_fill=*/true,
                               ActorFormFillingError::kNoForm,
                               histogram_tester);
}

// Tests that a `kOther` error is returned if an invalid suggestion id is passed
// for filling.
TEST_F(ActorFormFillingServiceTest, FillWithInvalidSuggestionId) {
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(tab(),
                            {ActorFormFillingSelection(ActorSuggestionId(123))},
                            fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), ErrorIs(ActorFormFillingError::kOther));
}

// Tests that a `kOther` error is returned if an invalid suggestion id is passed
// for filling.
TEST_F(ActorFormFillingServiceTest, FillButFormIsGone) {
  base::HistogramTester histogram_tester;
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(SizeIs(1)));

  manager().OnFormsSeen(/*updated_forms=*/{},
                        /*removed_forms=*/{form.global_id()});
  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), ErrorIs(ActorFormFillingError::kNoForm));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
  ExpectFillSuggestionsOutcome(/*is_payments_fill=*/false,
                               ActorFormFillingError::kNoForm,
                               histogram_tester);
}

// Tests that suggestions are generated and filled if the trigger field is a
// select field.
TEST_F(ActorFormFillingServiceTest, TriggerOnSelect) {
  FormData form = SeeForm(
      {.fields = {{.server_type = ADDRESS_HOME_COUNTRY,
                   .form_control_type = FormControlType::kSelectOne,
                   .select_options =
                       {SelectOption{.value = u"US", .text = u"United States"},
                        SelectOption{.value = u"CA", .text = u"Canada"},
                        SelectOption{.value = u"DE", .text = u"Germany"}}},
                  {.server_type = ADDRESS_HOME_LINE1},
                  {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture request_future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           request_future.GetCallback());
  EXPECT_THAT(request_future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequest(
                  ActorFormFillingRequest::RequestedData::
                      FormFillingRequest_RequestedData_ADDRESS))));

  std::vector<ActorFormFillingRequest> requests = request_future.Take().value();
  ASSERT_THAT(requests, Not(IsEmpty()));
  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              Contains(Pair(form.fields()[0].global_id(), u"US")));
}

// Tests that `kAutofillNotAvailable` is returned if the tab has no web
// contents.
TEST(ActorFormFillingServiceWithoutAutofillTest, NoWebContents) {
  base::HistogramTester histogram_tester;
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(nullptr));

  GetSuggestionsFuture future;
  ActorFormFillingServiceImpl service;
  service.GetSuggestions(mock_tab, {UnfindableFillRequest()},
                         future.GetCallback());
  EXPECT_THAT(future.Get(),
              ErrorIs(ActorFormFillingError::kAutofillNotAvailable));

  ExpectGetSuggestionsOutcome(ActorFormFillingError::kAutofillNotAvailable,
                              histogram_tester);
}

// Tests that `kAutofillNotAvailable` is returned if the tab has no
// `AutofillClient`.
TEST(ActorFormFillingServiceWithoutAutofillTest, NoAutofillClient) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::TestWebContentsFactory test_web_contents_factory;
  content::WebContents* web_contents =
      test_web_contents_factory.CreateWebContents(&profile);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(web_contents));
  ASSERT_FALSE(ContentAutofillClient::FromWebContents(web_contents));

  GetSuggestionsFuture future;
  ActorFormFillingServiceImpl service;
  service.GetSuggestions(mock_tab, {UnfindableFillRequest()},
                         future.GetCallback());
  EXPECT_THAT(future.Get(),
              ErrorIs(ActorFormFillingError::kAutofillNotAvailable));
}

}  // namespace

}  // namespace autofill
