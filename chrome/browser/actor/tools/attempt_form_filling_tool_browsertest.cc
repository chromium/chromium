// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_switches.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/autofill/glic/mock_actor_form_filling_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Property;
using ::testing::ReturnRef;

namespace actor {

namespace {

// Helper function that returns a composed matcher for ActorSuggestion.
auto EqActorSuggestion(const autofill::ActorSuggestion& expected) {
  return AllOf(
      Field(&autofill::ActorSuggestion::id, Eq(expected.id)),
      Field(&autofill::ActorSuggestion::title, Eq(expected.title)),
      Field(&autofill::ActorSuggestion::details, Eq(expected.details)));
}

// Helper function that returns a composed matcher for FillRequest.
auto EqFormFillingRequest(
    optimization_guide::proto::FormFillingRequest_RequestedData expected_data,
    const std::vector<autofill::FieldGlobalId>& expected_ids) {
  return FieldsAre(Eq(expected_data), Eq(expected_ids));
}

// Helper function that returns a composed matcher for ActorFormFillingRequest.
template <typename SuggestionsMatcher>
auto EqActorFormFillingRequest(
    optimization_guide::proto::FormFillingRequest_RequestedData expected_data,
    SuggestionsMatcher suggestions_matcher) {
  return AllOf(Field(&autofill::ActorFormFillingRequest::requested_data,
                     Eq(expected_data)),
               Field(&autofill::ActorFormFillingRequest::suggestions,
                     suggestions_matcher));
}

std::unique_ptr<ToolRequest> MakeAttemptFormFillingRequest(
    const tabs::TabInterface& tab,
    std::vector<PageTarget> trigger_fields) {
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests;
  requests.emplace_back().trigger_fields = std::move(trigger_fields);
  return std::make_unique<AttemptFormFillingToolRequest>(tab.GetHandle(),
                                                         std::move(requests));
}

// Gets the dom node or returns nullopt when the node id or document token
// cannot be retrieved.
std::optional<DomNode> GetDomNodeOnPage(content::RenderFrameHost& rfh,
                                        std::string_view query_selector) {
  std::optional<int> node_id = GetDOMNodeId(rfh, query_selector);
  if (!node_id) {
    return std::nullopt;
  }
  std::optional<std::string> document_identifier =
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken());
  if (!document_identifier) {
    return std::nullopt;
  }
  return DomNode{.node_id = *node_id,
                 .document_identifier = std::move(*document_identifier)};
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(Profile* profile) : ExecutionEngine(profile) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(void,
              RequestToShowAutofillSuggestions,
              (std::vector<autofill::ActorFormFillingRequest>,
               ToolDelegate::AutofillSuggestionSelectedCallback),
              (override));
  MOCK_METHOD(autofill::ActorFormFillingService&,
              GetActorFormFillingService,
              (),
              (override));
};

class AttemptFormFillingToolTest : public ActorToolsTest {
 public:
  ~AttemptFormFillingToolTest() override = default;

  // Gmock Action for the RequestToShowAutofillSuggestions() call.
  struct SuggestionSelectionAction {
    // A reference to the test fixture in order to retrieve the task id.
    base::WeakPtr<AttemptFormFillingToolTest> test_fixture;
    // Simulated user selections as an array of indexes. Each index corresponds
    // to one request. When empty, then zero (the first suggestion) is assumed.
    std::vector<size_t> selections;

    void operator()(
        std::vector<autofill::ActorFormFillingRequest> requests,
        ExecutionEngine::AutofillSuggestionSelectedCallback callback) {
      auto response =
          webui::mojom::SelectAutofillSuggestionsDialogResponse::New();
      if (!test_fixture) {
        std::move(callback).Run(std::move(response));
        return;
      }
      response->task_id = test_fixture->actor_task().id().value();
      std::vector<webui::mojom::FormFillingResponsePtr> selected_suggestions;
      for (size_t i = 0; i < requests.size(); i++) {
        const auto& request = requests[i];
        if (!request.suggestions.empty()) {
          size_t suggestion_index = SuggestionIndexForRequest(i);
          auto form_response = webui::mojom::FormFillingResponse::New();
          form_response->selected_suggestion_id = base::NumberToString(
              request.suggestions[suggestion_index].id.value());
          selected_suggestions.push_back(std::move(form_response));
        }
      }
      response->result = webui::mojom::SelectAutofillSuggestionsDialogResult::
          NewSelectedSuggestions(std::move(selected_suggestions));
      std::move(callback).Run(std::move(response));
    }

    size_t SuggestionIndexForRequest(size_t request_index) {
      if (request_index >= selections.size()) {
        return 0;
      }
      return selections[request_index];
    }
  };

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());

    ON_CALL(mock_execution_engine(), GetActorFormFillingService())
        .WillByDefault(ReturnRef(mock_form_filling_service_));

    ON_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
        .WillByDefault(MakeSelections({}));
  }

 protected:
  SuggestionSelectionAction MakeSelections(std::vector<size_t> selections) {
    return SuggestionSelectionAction{
        .test_fixture = weak_ptr_factory_.GetWeakPtr(),
        .selections = std::move(selections)};
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile) override {
    return std::make_unique<::testing::NiceMock<MockExecutionEngine>>(profile);
  }

  autofill::MockActorFormFillingService& mock_form_filling_service() {
    return mock_form_filling_service_;
  }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  webui::mojom::SelectAutofillSuggestionsDialogResponsePtr
  MakeAutofillSuggestionsErrorResponse() {
    auto response =
        webui::mojom::SelectAutofillSuggestionsDialogResponse::New();
    response->task_id = actor_task().id().value();
    response->result =
        webui::mojom::SelectAutofillSuggestionsDialogResult::NewErrorReason(
            webui::mojom::SelectAutofillSuggestionsDialogErrorReason::
                kDialogPromiseNoSubscriber);
    return response;
  }

  void WaitForTabObservation() {
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
    WaitForCopyableViewInWebContents(web_contents());
    ActorKeyedService* actor_keyed_service =
        ActorKeyedService::Get(browser()->profile());
    TestFuture<ActorKeyedService::TabObservationResult> tab_observation_future;
    actor_keyed_service->RequestTabObservation(
        *active_tab(), actor_task().id(), tab_observation_future.GetCallback());
    const ActorKeyedService::TabObservationResult& result =
        tab_observation_future.Get();
    std::optional<std::string> error_message =
        ActorKeyedService::ExtractErrorMessageIfFailed(result);
    ASSERT_FALSE(error_message)
        << "Waiting for tab observation failed: " << *error_message;
    ASSERT_TRUE(result.value());
  }

 private:
  autofill::MockActorFormFillingService mock_form_filling_service_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kGlicActorAutofill};
  base::WeakPtrFactory<AttemptFormFillingToolTest> weak_ptr_factory_{this};
};

// Test that the form filling service is called to retrieve and fill
// suggestions.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, GetSuggestionsAndFill) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";
  request.suggestions.push_back(suggestion);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  autofill::ActorFormFillingSelection response;
  response.selected_suggestion_id = request.suggestions[0].id;

  EXPECT_CALL(mock_form_filling_service(),
              FillSuggestions(_, ElementsAre(response), _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Test that when form filling service returns no suggestions and error is
// returned from the tool.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, NoSuggestions) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(autofill::ActorFormFillingError::kNoSuggestions)));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(
      result, mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable);
}

// Test that if the dialog is not shown an error is returned from the tool.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, DialogNotShown) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";
  request.suggestions.push_back(suggestion);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  auto dialog_error_response =
      webui::mojom::SelectAutofillSuggestionsDialogResponse::New(
          actor_task().id().value(),
          webui::mojom::SelectAutofillSuggestionsDialogResult::NewErrorReason(
              webui::mojom::SelectAutofillSuggestionsDialogErrorReason::
                  kDialogPromiseNoSubscriber));
  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .WillOnce(RunOnceCallback<1>(std::move(dialog_error_response)));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingDialogError);
}

// Test that if the dialog returns no selected suggestions then an error is
// returned from the tool.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest,
                       DialogResponseWithEmptySelectedSuggestions) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";
  request.suggestions.push_back(suggestion);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  auto dialog_with_empty_selected_suggestions =
      webui::mojom::SelectAutofillSuggestionsDialogResponse::New(
          actor_task().id().value(),
          webui::mojom::SelectAutofillSuggestionsDialogResult::
              NewSelectedSuggestions({}));
  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .WillOnce(RunOnceCallback<1>(
          std::move(dialog_with_empty_selected_suggestions)));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingDialogError);
}

// Test that when the form filling service fails to fill, an error is returned
// from the tool.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, FillFails) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";
  request.suggestions.push_back(suggestion);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  EXPECT_CALL(mock_form_filling_service(), FillSuggestions)
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(autofill::ActorFormFillingError::kOther)));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
}

// Test that when multiple suggestions are provided by the form filling service,
// the selected suggestion is used to fill suggestions.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, MultipleSuggestions) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion1;
  suggestion1.id = autofill::ActorSuggestionId(123);
  suggestion1.title = "My Address";
  request.suggestions.push_back(suggestion1);
  autofill::ActorSuggestion suggestion2;
  suggestion2.id = autofill::ActorSuggestionId(456);
  suggestion2.title = "Work Address";
  request.suggestions.push_back(suggestion2);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .WillOnce(MakeSelections({1}));

  autofill::ActorFormFillingSelection response;
  response.selected_suggestion_id = request.suggestions[1].id;

  EXPECT_CALL(mock_form_filling_service(),
              FillSuggestions(_, ElementsAre(response), _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Test that if the trigger field is no longer available, an error is returned.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, TimeOfUseValidationFails) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();

  // Use a PageTarget with a DOMNode that won't be found.
  PageTarget target(DomNode{.node_id = 12345, .document_identifier = "abc"});
  std::unique_ptr<ToolRequest> action =
      MakeAttemptFormFillingRequest(*active_tab(), {target});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingFieldNotFound);
}

// Test that the request passed to the GetSuggestions() call contains the form
// filling requests.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest,
                       ProvidesDataToGetSuggestions) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::FieldGlobalId expected_field_id(
      autofill::LocalFrameToken(main_frame()->GetFrameToken().value()),
      autofill::FieldRendererId(address_home_line1->node_id));

  EXPECT_CALL(
      mock_form_filling_service(),
      GetSuggestions(
          _,
          ElementsAre(
              EqFormFillingRequest(
                  optimization_guide::proto::
                      FormFillingRequest_RequestedData_SHIPPING_ADDRESS,
                  std::vector<autofill::FieldGlobalId>{expected_field_id}),
              EqFormFillingRequest(
                  optimization_guide::proto::
                      FormFillingRequest_RequestedData_CREDIT_CARD,
                  std::vector<autofill::FieldGlobalId>{expected_field_id}),
              EqFormFillingRequest(
                  optimization_guide::proto::
                      FormFillingRequest_RequestedData_CONTACT_INFORMATION,
                  std::vector<autofill::FieldGlobalId>{expected_field_id})),
          _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(autofill::ActorFormFillingError::kNoSuggestions)));

  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests;
  AttemptFormFillingToolRequest::FormFillingRequest request1;
  request1.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kShippingAddress;
  request1.trigger_fields = {PageTarget(*address_home_line1)};
  requests.push_back(std::move(request1));

  AttemptFormFillingToolRequest::FormFillingRequest request2;
  request2.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kCreditCard;
  request2.trigger_fields = {PageTarget(*address_home_line1)};
  requests.push_back(std::move(request2));

  AttemptFormFillingToolRequest::FormFillingRequest request3;
  request3.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kContactInformation;
  request3.trigger_fields = {PageTarget(*address_home_line1)};
  requests.push_back(std::move(request3));

  auto action = std::make_unique<AttemptFormFillingToolRequest>(
      active_tab()->GetHandle(), std::move(requests));

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(std::unique_ptr<ToolRequest>(std::move(action))),
      result.GetCallback());
  ExpectErrorResult(
      result, mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable);
}

// Test that the request passed to the RequestToShowAutofillSuggestions() call
// contains the suggestions from the form filling service.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest,
                       ProvidesDataToRequestToShowAutofillSuggestions) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  request.requested_data =
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS;
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Test Address";
  suggestion.details = "123 Main St, Anytown, USA";
  request.suggestions.push_back(suggestion);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  EXPECT_CALL(mock_execution_engine(),
              RequestToShowAutofillSuggestions(
                  ElementsAre(EqActorFormFillingRequest(
                      request.requested_data,
                      ElementsAre(EqActorSuggestion(request.suggestions[0])))),
                  _))
      .WillOnce(RunOnceCallback<1>(MakeAutofillSuggestionsErrorResponse()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(
      ToRequestList(std::unique_ptr<ToolRequest>(std::move(action))),
      result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingDialogError);
}

// Test that when switches::kAttemptFormFillingToolSkipsUI is enabled, the
// user is not asked to select a suggestion.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, TestSkippingSelection) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAttemptFormFillingToolSkipsUI);

  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request;
  autofill::ActorSuggestion suggestion1;
  suggestion1.id = autofill::ActorSuggestionId(123);
  suggestion1.title = "My Address";
  request.suggestions.push_back(suggestion1);
  autofill::ActorSuggestion suggestion2;
  suggestion2.id = autofill::ActorSuggestionId(456);
  suggestion2.title = "Work Address";
  request.suggestions.push_back(suggestion2);
  std::vector<autofill::ActorFormFillingRequest> requests = {request};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  // RequestToShowAutofillSuggestions should not be shown but instead the
  // first address is automatically selected.
  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .Times(0);

  autofill::ActorFormFillingSelection response;
  response.selected_suggestion_id = request.suggestions[0].id;

  EXPECT_CALL(mock_form_filling_service(),
              FillSuggestions(_, ElementsAre(response), _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

}  // namespace

}  // namespace actor
