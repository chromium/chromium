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

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
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

class ActorFormFillingServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorFormFillingServiceTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab, GetContents()).WillByDefault(Return(web_contents()));
    NavigateAndCommit(GURL("about:blank"));
    client().GetPersonalDataManager().address_data_manager().AddProfile(
        test::GetFullProfile());
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
  TestBrowserAutofillManager& manager() {
    return CHECK_DEREF(autofill_manager_injector_[web_contents()]);
  }
  ActorFormFillingService& service() { return service_; }
  tabs::TabInterface& tab() { return mock_tab; }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  tabs::MockTabInterface mock_tab;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<TestContentAutofillDriver>
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
