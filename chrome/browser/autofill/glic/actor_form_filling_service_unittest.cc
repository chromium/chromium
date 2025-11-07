// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_filling_service.h"

#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
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
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Return;
using ::testing::SizeIs;

using FillRequest = ActorFormFillingService::FillRequest;
using GetSuggestionsFuture =
    base::test::TestFuture<base::expected<std::vector<ActorFormFillingRequest>,
                                          ActorFormFillingError>>;
using FillSuggestionsFuture =
    base::test::TestFuture<base::expected<void, ActorFormFillingError>>;

[[nodiscard]] Matcher<ActorSuggestion> NonEmptyActorSuggestion() {
  return AllOf(Field(&ActorSuggestion::title, Not(IsEmpty())),
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

// Returns the value that `group` would fill into a field with a certain `type`.
std::u16string GetFillValue(const FormGroup& group, FieldType type) {
  return group.GetInfo(AutofillType(FieldTypeSet({type})), "en-us");
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

class ActorFormFillingServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorFormFillingServiceTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab, GetContents()).WillByDefault(Return(web_contents()));
    NavigateAndCommit(GURL("about:blank"));
    client().GetPersonalDataManager().address_data_manager().AddProfile(
        GetProfile1());
    client().GetPersonalDataManager().payments_data_manager().AddCreditCard(
        GetCreditCard1());
  }

  FormData SeeForm(test::FormDescription form_description) {
    FormData form = test::GetFormData(form_description);
    manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                          test::GetServerTypes(form_description));
    return form;
  }

 protected:
  AutofillClient& client() {
    return CHECK_DEREF(autofill_client_injector_[web_contents()]);
  }
  RecordingTestContentAutofillDriver& driver() {
    return CHECK_DEREF(autofill_driver_injector_[web_contents()]);
  }
  TestBrowserAutofillManager& manager() {
    return CHECK_DEREF(autofill_manager_injector_[web_contents()]);
  }
  ActorFormFillingService& service() { return service_; }
  tabs::TabInterface& tab() { return mock_tab; }

  // Returns the test data available in `AddressDataManager`.
  AutofillProfile GetProfile1() { return test::GetFullProfile(); }

  // Returns the test data available in `PaymentsDataManager`.
  CreditCard GetCreditCard1() { return test::GetCreditCard(); }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  tabs::MockTabInterface mock_tab;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<RecordingTestContentAutofillDriver>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  ActorFormFillingServiceImpl service_;
};

// Tests that a `kNoSuggestions` error is returned if we cannot find the form
// specified in the request.
TEST_F(ActorFormFillingServiceTest, UnfindableForm) {
  GetSuggestionsFuture future;
  service().GetSuggestions(tab(), {UnfindableFillRequest()},
                           future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoSuggestions));
}

// Tests that a `kOther` error is returned if the fill request is empty.
TEST_F(ActorFormFillingServiceTest, EmptyFillRequest) {
  GetSuggestionsFuture future;
  service().GetSuggestions(tab(), /*fill_requests=*/{}, future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kOther));
}

// Tests that a `kOther` error is returned if invalid request data is passed.
TEST_F(ActorFormFillingServiceTest, InvalidRequestData) {
  GetSuggestionsFuture future;
  service().GetSuggestions(
      tab(), /*fill_requests=*/
      {FillRequest{static_cast<ActorFormFillingRequest::RequestedData>(234),
                   {FieldGlobalId()}}},
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kOther));
}

// Tests that a suggestion is returned when invoking on an address form and
// that the suggestion can be used for filling.
TEST_F(ActorFormFillingServiceTest, SimpleAddressForm) {
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
  EXPECT_THAT(fill_future.Get(), HasValue());
  EXPECT_THAT(driver().last_filled_values(),
              Contains(std::pair(
                  form.fields()[0].global_id(),
                  GetFillValue(GetCreditCard1(), CREDIT_CARD_NAME_FULL))));
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
}

// Tests that `kAutofillNotAvailable` is returned if the tab has no web
// contents.
TEST(ActorFormFillingServiceWithoutAutofillTest, NoWebContents) {
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(nullptr));

  GetSuggestionsFuture future;
  ActorFormFillingServiceImpl service;
  service.GetSuggestions(mock_tab, {UnfindableFillRequest()},
                         future.GetCallback());
  EXPECT_THAT(future.Get(),
              ErrorIs(ActorFormFillingError::kAutofillNotAvailable));
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
