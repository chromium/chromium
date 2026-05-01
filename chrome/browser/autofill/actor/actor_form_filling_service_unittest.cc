// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_form_filling_service.h"

#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/autofill/actor/actor_filling_observer.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service_impl.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service_impl_test_api.h"
#include "chrome/browser/autofill/actor/actor_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

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
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::Truly;

using FillRequest = ActorFormFillingService::FillRequest;
using GetSuggestionsFuture =
    base::test::TestFuture<base::expected<std::vector<ActorFormFillingRequest>,
                                          ActorFormFillingError>>;
using FillSuggestionsFuture =
    base::test::TestFuture<base::expected<void, ActorFormFillingError>>;

gfx::Image CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

[[nodiscard]] Matcher<ActorSuggestion> NonEmptyActorSuggestion() {
  return AnyOf(Field(&ActorSuggestion::title, Not(IsEmpty())),
               Field(&ActorSuggestion::details, Not(IsEmpty())));
}

[[nodiscard]] Matcher<ActorSuggestion> ActorSuggestionHasNonEmptyIcon() {
  return Field(&ActorSuggestion::icon,
               Optional(Property(&gfx::Image::IsEmpty, false)));
}

[[nodiscard]] Matcher<ActorSuggestion> ActorSuggestionIconEquals(
    gfx::Image expected_image) {
  return Field(&ActorSuggestion::icon,
               Optional(Truly([expected_image](const gfx::Image& actual_image) {
                 return gfx::test::AreBitmapsEqual(expected_image.AsBitmap(),
                                                   actual_image.AsBitmap());
               })));
}

[[nodiscard]] Matcher<ActorFormFillingRequest> IsActorFormFillingRequest(
    ActorFormFillingRequest::RequestedData requested_data) {
  return AllOf(Field(&ActorFormFillingRequest::requested_data, requested_data),
               Field(&ActorFormFillingRequest::suggestions,
                     Each(NonEmptyActorSuggestion())));
}

[[nodiscard]] Matcher<ActorFormFillingRequest>
IsActorFormFillingRequestWithOrigin(const url::Origin request_origin) {
  return Field("request_origin", &ActorFormFillingRequest::request_origin,
               request_origin);
}

// Returns a fill-request with a non-sensical (because null) field id.
FillRequest UnfindableFillRequest() {
  return {ActorFormFillingRequest::RequestedData::kAddress, {FieldGlobalId()}};
}

FillRequest AddressFillRequest(std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kAddress,
          std::move(field_ids)};
}

FillRequest CreditCardFillRequest(std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kCreditCard,
          std::move(field_ids)};
}

FillRequest ContactInformationFillRequest(
    std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kContactInformation,
          std::move(field_ids)};
}

FillRequest BillingAddressFillRequest(std::vector<FieldGlobalId> field_ids) {
  return {ActorFormFillingRequest::RequestedData::kBillingAddress,
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

class MockJournalObserver : public ::actor::AggregatedJournal::Observer {
 public:
  explicit MockJournalObserver(::actor::AggregatedJournal& journal)
      : journal_(journal.GetSafeRef()) {
    journal.AddObserver(this);
  }
  ~MockJournalObserver() override { journal_->RemoveObserver(this); }

  MOCK_METHOD(void,
              WillAddJournalEntry,
              (const ::actor::AggregatedJournal::Entry& entry),
              (override));

 private:
  base::SafeRef<::actor::AggregatedJournal> journal_;
};

class ActorFormFillingServiceTest : public ActorTestBase {
 public:
  void SetUp() override {
    ActorTestBase::SetUp();
    // Enable AutofillWalletImport so that custom credit card art is available.
    client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .SetAutofillWalletImportEnabled(true);
    client().GetPersonalDataManager().address_data_manager().AddProfile(
        GetProfile1());
    client().GetPersonalDataManager().address_data_manager().AddProfile(
        GetProfile2());
  }

 protected:
  payments::TestPaymentsAutofillClient& payments_client() {
    return *client().GetPaymentsAutofillClient();
  }

  // Returns an address that is available in `AddressDataManager`.
  AutofillProfile GetProfile1() { return test::GetFullProfile(); }
  AutofillProfile GetProfile2() { return test::GetFullProfile2(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
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
                  ActorFormFillingRequest::RequestedData::kAddress))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));

  EXPECT_THAT(last_filled_values(),
              Contains(std::pair(form.fields()[0].global_id(),
                                 GetFillValue(GetProfile1(), NAME_FULL))));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
}

// Tests that the origin of the web contents is returned in the form filling
// request.
TEST_F(ActorFormFillingServiceTest, SpecificOriginAddressForm) {
  const GURL url("https://example.test");
  NavigateAndCommit(url);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(IsActorFormFillingRequestWithOrigin(
                  url::Origin::Create(url)))));
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
  EXPECT_THAT(
      future.Get(),
      ValueIs(ElementsAre(IsActorFormFillingRequest(
          ActorFormFillingRequest::RequestedData::kContactInformation))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  EXPECT_THAT(
      last_filled_values(),
      IsSupersetOf({std::pair(form.fields()[0].global_id(),
                              GetFillValue(GetProfile1(), NAME_FULL)),
                    std::pair(form.fields()[1].global_id(),
                              GetFillValue(GetProfile1(), EMAIL_ADDRESS))}));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
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
  EXPECT_THAT(
      future.Get(),
      ValueIs(ElementsAre(IsActorFormFillingRequest(
          ActorFormFillingRequest::RequestedData::kContactInformation))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));

  // Expect that all fields, including address fields, are filled.
  EXPECT_THAT(
      last_filled_values(),
      IsSupersetOf(
          {std::pair(form.fields()[0].global_id(),
                     GetFillValue(GetProfile1(), NAME_FULL)),
           std::pair(form.fields()[1].global_id(),
                     GetFillValue(GetProfile1(), EMAIL_ADDRESS)),
           std::pair(form.fields()[2].global_id(),
                     GetFillValue(GetProfile1(), ADDRESS_HOME_LINE1))}));
}

// Tests that a mixed form section (Contact Info + Address) returns only one
// request and fills everything if kAutofillActorFormFillingSplitOutContactInfo
// is disabled.
TEST_F(ActorFormFillingServiceTest, MixedForm_SectionSplitting_Disabled) {
  feature_list_.InitAndDisableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());
  // Should return exactly one request (ADDRESS).
  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  EXPECT_THAT(requests, ElementsAre(IsActorFormFillingRequest(
                            ActorFormFillingRequest::RequestedData::kAddress)));

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));

  // Everything should be filled.
  EXPECT_THAT(last_filled_values(),
              IsSupersetOf(
                  {std::pair(form.fields()[0].global_id(),
                             GetFillValue(GetProfile1(), NAME_FULL)),
                   std::pair(form.fields()[1].global_id(),
                             GetFillValue(GetProfile1(), EMAIL_ADDRESS)),
                   std::pair(form.fields()[2].global_id(),
                             GetFillValue(GetProfile1(), ADDRESS_HOME_LINE1)),
                   std::pair(form.fields()[3].global_id(),
                             GetFillValue(GetProfile1(), ADDRESS_HOME_CITY))}));
}

// Tests that a mixed form section (Contact Info + Address) returns two requests
// and fills selectively when the section splitting feature is enabled.
TEST_F(ActorFormFillingServiceTest, MixedForm_SectionSplitting_Enabled) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {BillingAddressFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  // Should return two requests: CONTACT_INFORMATION and BILLING_ADDRESS.
  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  EXPECT_THAT(
      requests,
      ElementsAre(
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kContactInformation),
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kBillingAddress)));

  // Mock out the user having selected profile #2 for the contact part, and
  // profile #1 for the address part.
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[1].id));
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[1].suggestions[0].id));

  // Verify that fields were filled accordingly; Name and Email with profile
  // #2, and the address fields with profile #1.
  EXPECT_THAT(last_filled_values(),
              Contains(Pair(form.fields()[0].global_id(),
                            GetFillValue(GetProfile2(), NAME_FULL))));
  EXPECT_THAT(last_filled_values(),
              Contains(Pair(form.fields()[1].global_id(),
                            GetFillValue(GetProfile2(), EMAIL_ADDRESS))));
  EXPECT_THAT(last_filled_values(),
              Contains(Pair(form.fields()[2].global_id(),
                            GetFillValue(GetProfile1(), ADDRESS_HOME_LINE1))));
  EXPECT_THAT(last_filled_values(),
              Contains(Pair(form.fields()[3].global_id(),
                            GetFillValue(GetProfile1(), ADDRESS_HOME_CITY))));
}

// Tests that if section splitting occurs for a CONTACT_INFORMATION type,
// we change the type of the second part to ADDRESS rather than have repeat
// CONTACT_INFORMATION requests.
TEST_F(ActorFormFillingServiceTest,
       MixedForm_SectionSplitting_ContactInfoRequested) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {ContactInformationFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());
  // Should return two requests: CONTACT_INFORMATION and ADDRESS.
  EXPECT_THAT(
      future.Get(),
      ValueIs(ElementsAre(
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kContactInformation),
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kAddress))));
}

// Tests that if section splitting occurs, we properly retarget the trigger
// field for both sub-requests.
TEST_F(ActorFormFillingServiceTest, MixedForm_SectionSplitting_Retargeting) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = PHONE_HOME_WHOLE_NUMBER},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  // Trigger on NAME_FULL (index 0).
  service().GetSuggestions(tab(),
                           {AddressFillRequest({form.fields()[0].global_id()})},
                           future.GetCallback());

  ASSERT_THAT(future.Get(), ValueIs(SizeIs(2)));
  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  // The CONTACT_INFORMATION request should be retargeted to the EMAIL_ADDRESS
  // field.
  EXPECT_EQ(requests[0].requested_data,
            ActorFormFillingRequest::RequestedData::kContactInformation);
  EXPECT_THAT(
      requests[0].suggestions[0].title,
      HasSubstr(base::UTF16ToUTF8(GetFillValue(GetProfile1(), EMAIL_ADDRESS))));

  // The ADDRESS request should be retargeted to the ADDRESS_HOME_LINE1 field.
  EXPECT_EQ(requests[1].requested_data,
            ActorFormFillingRequest::RequestedData::kAddress);
  EXPECT_THAT(requests[1].suggestions[0].title,
              HasSubstr(base::UTF16ToUTF8(
                  GetFillValue(GetProfile1(), ADDRESS_HOME_LINE1))));
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
                  ActorFormFillingRequest::RequestedData::kAddress))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  EXPECT_THAT(last_filled_values(),
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
                  ActorFormFillingRequest::RequestedData::kCreditCard))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  EXPECT_THAT(last_filled_values(),
              Contains(std::pair(form.fields()[0].global_id(),
                                 GetFillValue(card, CREDIT_CARD_NAME_FULL))));

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
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

// Tests that a suggestion is returned when invoking on a credit card form and
// that the suggestion has a non-empty network icon.
TEST_F(ActorFormFillingServiceTest, CreditCardFormWithNetworkIcon) {
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
  // Verify that the suggestion has a non-empty icon.
  EXPECT_THAT(
      future.Get(),
      ValueIs(ElementsAre(Field(&ActorFormFillingRequest::suggestions,
                                Each(ActorSuggestionHasNonEmptyIcon())))));
}

// Tests that a suggestion is returned when invoking on a credit card form and
// that the suggestion has a custom card art icon.
TEST_F(ActorFormFillingServiceTest, CreditCardFormWithCardArtIcon) {
  CreditCard card = test::GetCreditCard();
  card.set_card_art_url(GURL("https://example.test/card_art.png"));
  payments_data_manager().AddCreditCard(card);

  gfx::Image test_image = CreateTestImage(100, 50);
  client().GetPersonalDataManager().test_payments_data_manager().CacheImage(
      card.card_art_url(), test_image);

  FormData form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {CreditCardFillRequest({form.fields()[0].global_id()})},
      future.GetCallback());

  // Verify that the suggestion's icon matches the test image pixel by pixel.
  EXPECT_THAT(future.Get(),
              ValueIs(ElementsAre(Field(
                  &ActorFormFillingRequest::suggestions,
                  Each(ActorSuggestionIconEquals(std::move(test_image)))))));
}

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
                  ActorFormFillingRequest::RequestedData::kCreditCard))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  // Now we notify observers that a credit card fetch was started.
  using Observer = CreditCardAccessManager::Observer;
  test_api(credit_card_access_manager())
      .NotifyObservers(&Observer::OnCreditCardFetchStarted, card);

  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetMaximumTimeout(), base::Seconds(1));
  task_environment()->FastForwardBy(ActorFillingObserver::GetMaximumTimeout() -
                                    base::Seconds(1));
  EXPECT_FALSE(fill_future.IsReady());

  // Simulate successful fetching.
  ASSERT_TRUE(credit_card_access_manager().RunCreditCardFetchedCallback(card));
  test_api(credit_card_access_manager())
      .NotifyObservers(&Observer::OnCreditCardFetchSucceeded, card);

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fill_future.IsReady());
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(last_filled_values(),
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
                  ActorFormFillingRequest::RequestedData::kCreditCard))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  // Now we notify observers that a credit card fetch was started.
  test_api(credit_card_access_manager())
      .NotifyObservers(
          &CreditCardAccessManager::Observer::OnCreditCardFetchStarted, card);

  FillSuggestionsFuture fill_future;
  service().FillSuggestions(
      tab(), {ActorFormFillingSelection(requests[0].suggestions[0].id)},
      fill_future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetMaximumTimeout(), base::Seconds(2));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(fill_future.IsReady());
  task_environment()->FastForwardBy(ActorFillingObserver::GetMaximumTimeout() -
                                    base::Seconds(2));
  EXPECT_FALSE(fill_future.IsReady());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fill_future.IsReady());
  EXPECT_THAT(fill_future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
  EXPECT_THAT(last_filled_values(), IsEmpty());

  ExpectGetSuggestionsOutcome(kActorFormFillingSuccessForMetrics,
                              histogram_tester);
}

// Tests that a `kOther` error is returned if an invalid suggestion id is passed
// for filling.
TEST_F(ActorFormFillingServiceTest, FillWithInvalidSuggestionId) {
  FillSuggestionsFuture fill_future;
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(ActorSuggestionId(123)));
  EXPECT_THAT(test_api(service()).FillingErrors(),
              ElementsAre(ActorFormFillingError::kOther));
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
                  ActorFormFillingRequest::RequestedData::kAddress))));

  std::vector<ActorFormFillingRequest> requests = request_future.Take().value();
  ASSERT_THAT(requests, Not(IsEmpty()));
  service().FillForm(tab(), /*form_index=*/0,
                     ActorFormFillingSelection(requests[0].suggestions[0].id));
  EXPECT_THAT(last_filled_values(),
              Contains(Pair(form.fields()[0].global_id(), u"US")));
}

// Tests that a suggestion is returned when invoking on an address form and
// that the suggestion can be used for filling.
TEST_F(ActorFormFillingServiceTest, FillOrPreview) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), {BillingAddressFillRequest({form.fields().front().global_id()})},
      future.GetCallback());

  ASSERT_THAT(
      future.Get(),
      ValueIs(ElementsAre(
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kContactInformation),
          IsActorFormFillingRequest(
              ActorFormFillingRequest::RequestedData::kBillingAddress))));

  std::vector<ActorFormFillingRequest> requests = future.Take().value();

  // TODO(crbug.com/480936584): Expect actual fields to be previewed/filled
  // instead of passing `_` when the splitting logic is finalized.
  EXPECT_CALL(driver(), ApplyFormAction(_, mojom::ActionPersistence::kPreview,
                                        _, _, _, _, _, _))
      .Times(1);
  EXPECT_CALL(driver(), ApplyFormAction(_, mojom::ActionPersistence::kFill, _,
                                        _, _, _, _, _))
      .Times(1);

  ASSERT_EQ(manager().last_trigger_field_id(), FieldGlobalId());
  service().PreviewForm(tab(), /*form_index=*/0,
                        requests.front().suggestions.front().id);

  // TODO(crbug.com/480936584): Expect actual trigger field IDs for split
  // sections.
  EXPECT_EQ(manager().last_trigger_field_id(), form.fields()[0].global_id());
  service().FillForm(
      tab(), /*form_index=*/1,
      ActorFormFillingSelection(requests.back().suggestions.front().id));
  EXPECT_EQ(manager().last_trigger_field_id(), form.fields()[0].global_id());
}

// Tests that `kAutofillNotAvailable` is returned if the tab has no web
// contents.
TEST(ActorFormFillingServiceWithoutAutofillTest, NoWebContents) {
  base::HistogramTester histogram_tester;
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(nullptr));

  GetSuggestionsFuture future;
  ::actor::AggregatedJournal journal;
  ActorFormFillingServiceImpl service(journal.GetSafeRef(), ::actor::TaskId(1));
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
  ::actor::AggregatedJournal journal;
  ActorFormFillingServiceImpl service(journal.GetSafeRef(), ::actor::TaskId(1));
  service.GetSuggestions(mock_tab, {UnfindableFillRequest()},
                         future.GetCallback());
  EXPECT_THAT(future.Get(),
              ErrorIs(ActorFormFillingError::kAutofillNotAvailable));
}

// Tests that requesting to clear the form preview correctly routes the call
// to `AutofillDriver`.
TEST_F(ActorFormFillingServiceTest, ClearFormPreview) {
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm()).Times(1);
  service().ClearFormPreview(tab(), /*form_index=*/0);
}

// Tests that requesting to scroll into a form correctly routes the call to
// `AutofillDriver` and with the correct `FieldGlobalId`.
TEST_F(ActorFormFillingServiceTest, ScrollToForm) {
  payments_data_manager().AddCreditCard(test::GetMaskedServerCard());

  FormData address_form =
      SeeForm({.fields = {{.server_type = NAME_FULL},
                          {.server_type = ADDRESS_HOME_LINE1},
                          {.server_type = ADDRESS_HOME_CITY}}});
  FormData credit_card_form =
      SeeForm({.fields = {{.server_type = CREDIT_CARD_NAME_FULL},
                          {.server_type = CREDIT_CARD_NUMBER},
                          {.server_type = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}}});

  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(),
      {AddressFillRequest({address_form.fields()[0].global_id()}),
       CreditCardFillRequest({credit_card_form.fields()[0].global_id()})},
      future.GetCallback());

  ASSERT_THAT(future.Get(),
              ValueIs(ElementsAre(
                  IsActorFormFillingRequest(
                      ActorFormFillingRequest::RequestedData::kAddress),
                  IsActorFormFillingRequest(
                      ActorFormFillingRequest::RequestedData::kCreditCard))));

  {
    testing::InSequence seq;

    EXPECT_CALL(driver(),
                ScrollFieldIntoView(address_form.fields()[0].global_id()))
        .Times(1);

    EXPECT_CALL(driver(),
                ScrollFieldIntoView(credit_card_form.fields()[1].global_id()))
        .Times(1);
  }

  service().ScrollToForm(tab(), /*form_index=*/0);
  service().ScrollToForm(tab(), /*form_index=*/1);
}

auto JournalEntryWithError(std::string_view expected_message) {
  return testing::Field(
      &::actor::AggregatedJournal::Entry::data,
      testing::Pointee(testing::Field(
          &::actor::mojom::JournalEntry::details,
          testing::Contains(testing::Pointee(testing::AllOf(
              testing::Field(&::actor::mojom::JournalDetails::key, "error"),
              testing::Field(&::actor::mojom::JournalDetails::value,
                             testing::HasSubstr(expected_message))))))));
}

TEST(ActorFormFillingServiceJournalTest,
     FillWithInvalidSuggestionId_LogsToJournal) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::TestWebContentsFactory test_web_contents_factory;
  content::WebContents* web_contents =
      test_web_contents_factory.CreateWebContents(&profile);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(testing::Return(web_contents));

  ::actor::AggregatedJournal journal;
  MockJournalObserver observer(journal);

  EXPECT_CALL(observer, WillAddJournalEntry(JournalEntryWithError(
                            "Autofill manager not available")));

  autofill::ActorFormFillingServiceImpl service(journal.GetSafeRef(),
                                                ::actor::TaskId(1));

  service.FillForm(
      mock_tab, /*form_index=*/0,
      autofill::ActorFormFillingSelection(autofill::ActorSuggestionId(123)));
}

}  // namespace

}  // namespace autofill
