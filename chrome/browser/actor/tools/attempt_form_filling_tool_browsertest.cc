// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool.h"

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_metrics.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/autofill/actor/mock_actor_form_filling_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
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
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Property;
using ::testing::Ref;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace actor {

namespace {

using RequestedData = AttemptFormFillingToolRequest::RequestedData;

// Helper function that returns a composed matcher for ActorSuggestion.
auto EqActorSuggestion(const autofill::ActorSuggestion& expected) {
  return AllOf(
      Field(&autofill::ActorSuggestion::id, Eq(expected.id)),
      Field(&autofill::ActorSuggestion::title, Eq(expected.title)),
      Field(&autofill::ActorSuggestion::details, Eq(expected.details)));
}

// Helper function that returns a composed matcher for FillRequest.
auto EqFormFillingRequest(
    RequestedData expected_data,
    const std::vector<autofill::FieldGlobalId>& expected_ids) {
  return FieldsAre(Eq(expected_data), Eq(expected_ids));
}

// Helper function that returns a composed matcher for ActorFormFillingRequest.
template <typename SuggestionsMatcher>
auto EqActorFormFillingRequest(RequestedData expected_data,
                               SuggestionsMatcher suggestions_matcher) {
  return AllOf(Field(&autofill::ActorFormFillingRequest::requested_data,
                     Eq(expected_data)),
               Field(&autofill::ActorFormFillingRequest::suggestions,
                     suggestions_matcher));
}

// Helper function that returns an ActorFormFillingSelection.
autofill::ActorFormFillingSelection MakeActorFormFillingSelection(
    autofill::ActorSuggestionId suggestion_id) {
  autofill::ActorFormFillingSelection selection;
  selection.selected_suggestion_id = suggestion_id;
  return selection;
}

std::unique_ptr<ToolRequest> MakeAttemptFormFillingRequest(
    const tabs::TabInterface& tab,
    std::vector<std::pair<std::vector<PageTarget>, RequestedData>>
        requests_in) {
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests_out;
  for (auto& [trigger_fields, request_data] : requests_in) {
    AttemptFormFillingToolRequest::FormFillingRequest request;
    request.trigger_fields = std::move(trigger_fields);
    request.requested_data = request_data;
    requests_out.emplace_back(request);
  }
  return std::make_unique<AttemptFormFillingToolRequest>(
      tab.GetHandle(), std::move(requests_out));
}

std::unique_ptr<ToolRequest> MakeAttemptFormFillingRequest(
    const tabs::TabInterface& tab,
    std::vector<PageTarget> trigger_fields) {
  return MakeAttemptFormFillingRequest(
      tab, {std::pair{std::move(trigger_fields), RequestedData::kUnknown}});
}

// Gets the dom node or returns nullopt when the node id or document token
// cannot be retrieved.
std::optional<DomNode> GetDomNodeOnPage(content::RenderFrameHost& rfh,
                                        std::string_view query_selector) {
  ASSIGN_OR_RETURN(int node_id, GetDOMNodeId(rfh, query_selector));
  ASSIGN_OR_RETURN(
      std::string document_identifier,
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  return DomNode{.node_id = node_id,
                 .document_identifier = std::move(document_identifier)};
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(ActorTask& task) : ExecutionEngine(task) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(void,
              RequestToShowAutofillSuggestions,
              (std::vector<autofill::ActorFormFillingRequest>,
               base::WeakPtr<AutofillSelectionDialogEventHandler>,
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
        base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
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

  static std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      ActorTask& task) {
    return std::make_unique<::testing::NiceMock<MockExecutionEngine>>(task);
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
        *active_tab(), actor_task().id(), std::nullopt,
        tab_observation_future.GetCallback());
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
  ScopedExecutionEngineFactory mock_execution_engine_factory_{
      base::BindRepeating(AttemptFormFillingToolTest::CreateExecutionEngine)};
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

  EXPECT_CALL(
      mock_form_filling_service(),
      FillSuggestions(
          _,
          ElementsAre(MakeActorFormFillingSelection(request.suggestions[0].id)),
          _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Test that dialog events are forwarded to the form filling service.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, DialogEventsForwarding) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);
  std::optional<DomNode> phone_number =
      GetDomNodeOnPage(*main_frame(), "#PHONE_HOME_WHOLE_NUMBER");
  ASSERT_TRUE(phone_number);

  autofill::ActorFormFillingRequest request1;
  request1.requested_data = RequestedData::kHomeAddress;
  autofill::ActorSuggestion suggestion1;
  suggestion1.id = autofill::ActorSuggestionId(123);
  request1.suggestions.push_back(suggestion1);

  autofill::ActorFormFillingRequest request2;
  request2.requested_data = RequestedData::kContactInformation;
  autofill::ActorSuggestion suggestion2;
  suggestion2.id = autofill::ActorSuggestionId(234);
  request2.suggestions.push_back(suggestion2);

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(std::vector{request1, request2}));

  TestFuture<base::WeakPtr<AutofillSelectionDialogEventHandler>> handler_future;
  ToolDelegate::AutofillSuggestionSelectedCallback captured_callback;
  ActResultFuture result;
  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .WillOnce([&](auto, auto handler, auto callback) {
        handler_future.SetValue(handler);
        captured_callback = std::move(callback);
      });

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {std::pair{std::vector{PageTarget(*address_home_line1)},
                                RequestedData::kHomeAddress},
                      std::pair{std::vector{PageTarget(*phone_number)},
                                RequestedData::kContactInformation}});
  actor_task().Act(ToRequestList(action), result.GetCallback());

  base::WeakPtr<AutofillSelectionDialogEventHandler> captured_handler =
      handler_future.Take();
  ASSERT_TRUE(captured_handler);

  // Expect that OnFormPresented calls ScrollToForm for the request.
  EXPECT_CALL(mock_form_filling_service(), ScrollToForm(Ref(*active_tab()), 0));
  EXPECT_TRUE(captured_handler->OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParams::New(
          /*form_filling_request_index=*/0)));
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      AttemptFormFillingToolRequest::RequestedData::kHomeAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kContactInformation, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogPresented,
      1);

  // Expect that OnFormPresented calls ScrollToForm for the request.
  EXPECT_CALL(mock_form_filling_service(), ScrollToForm(Ref(*active_tab()), 1));
  EXPECT_TRUE(captured_handler->OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParams::New(
          /*form_filling_request_index=*/1)));

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kHomeAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kContactInformation, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogPresented,
      1);

  // Expect that OnFormPresented doesn't call ScrollToForm and returns false for
  // non-existent request.
  EXPECT_FALSE(captured_handler->OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParams::New(
          /*form_filling_request_index=*/2)));

  // Expect that OnFormPreviewChanged (with response) calls PreviewForm
  EXPECT_CALL(mock_form_filling_service(),
              PreviewForm(Ref(*active_tab()), 20, suggestion1.id));
  captured_handler->OnFormPreviewChanged(
      webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParams::New(
          /*form_filling_request_index=*/20,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"123")));

  // Expect that OnFormPreviewChanged (null response) calls ClearFormPreview
  EXPECT_CALL(mock_form_filling_service(),
              ClearFormPreview(Ref(*active_tab()), 30));
  captured_handler->OnFormPreviewChanged(
      webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParams::New(
          /*form_filling_request_index=*/30, /*response=*/nullptr));

  // Expect that OnFormConfirmed calls FillForm for existing request and
  // suggestion.
  EXPECT_CALL(mock_form_filling_service(),
              FillForm(Ref(*active_tab()), 0,
                       MakeActorFormFillingSelection(suggestion1.id)));
  EXPECT_TRUE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/0,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"123"))));
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType",
      RequestedData::kHomeAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType",
      RequestedData::kContactInformation, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogAccepted,
      0);

  // Expect that OnFormConfirmed calls FillForm for existing request and
  // suggestion.
  EXPECT_CALL(mock_form_filling_service(),
              FillForm(Ref(*active_tab()), 1,
                       MakeActorFormFillingSelection(suggestion2.id)));
  EXPECT_TRUE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/1,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"234"))));
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType",
      RequestedData::kHomeAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType",
      RequestedData::kContactInformation, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogAccepted,
      1);

  // Expect that OnFormConfirmed doesn't call FillForm for non-existent request.
  EXPECT_FALSE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/2,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"123"))));

  // Expect that OnFormConfirmed doesn't call FillForm for non-integer
  // suggestion.
  EXPECT_FALSE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/0,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"abc"))));

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kInvoked, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kServiceResponded, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kSuggestionsRetrieved, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.AutofillSuggestionsPerDialog", 2, 1);

  std::move(captured_callback).Run(MakeAutofillSuggestionsErrorResponse());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingDialogError);
}

// Test that when form filling service returns no suggestions and error is
// returned from the tool.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, NoSuggestions) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kInvoked, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kServiceResponded, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kSuggestionsRetrieved, 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Actor.AutofillSuggestionsPerDialog", 0);
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
      .WillOnce(RunOnceCallback<2>(std::move(dialog_error_response)));

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
      .WillOnce(RunOnceCallback<2>(
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
  base::HistogramTester histogram_tester;
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

  EXPECT_CALL(
      mock_form_filling_service(),
      FillSuggestions(
          _,
          ElementsAre(MakeActorFormFillingSelection(request.suggestions[1].id)),
          _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectOkResult(result);

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kInvoked, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kServiceResponded, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kSuggestionsRetrieved, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.AutofillSuggestionsPerDialog", 1, 1);
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
                  RequestedData::kShippingAddress,
                  std::vector<autofill::FieldGlobalId>{expected_field_id}),
              EqFormFillingRequest(
                  RequestedData::kCreditCard,
                  std::vector<autofill::FieldGlobalId>{expected_field_id}),
              EqFormFillingRequest(
                  RequestedData::kContactInformation,
                  std::vector<autofill::FieldGlobalId>{expected_field_id})),
          _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(autofill::ActorFormFillingError::kNoSuggestions)));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {std::pair{std::vector{PageTarget(*address_home_line1)},
                                RequestedData::kShippingAddress},
                      std::pair{std::vector{PageTarget(*address_home_line1)},
                                RequestedData::kCreditCard},
                      std::pair{std::vector{PageTarget(*address_home_line1)},
                                RequestedData::kContactInformation}});

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
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
  request.requested_data = RequestedData::kAddress;
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
                  _, _))
      .WillOnce(RunOnceCallback<2>(MakeAutofillSuggestionsErrorResponse()));

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

  EXPECT_CALL(
      mock_form_filling_service(),
      FillSuggestions(
          _,
          ElementsAre(MakeActorFormFillingSelection(request.suggestions[0].id)),
          _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Test that when the service chooses to split one tool request into multiple
// (e.g. an address form request into contact information and an address), it
// correctly records metrics and interacts with the dialog.
IN_PROC_BROWSER_TEST_F(AttemptFormFillingToolTest, ServiceSplitsRequests) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  WaitForTabObservation();
  std::optional<DomNode> address_home_line1 =
      GetDomNodeOnPage(*main_frame(), "#ADDRESS_HOME_LINE1");
  ASSERT_TRUE(address_home_line1);

  autofill::ActorFormFillingRequest request1;
  request1.requested_data = RequestedData::kAddress;
  autofill::ActorSuggestion suggestion1;
  suggestion1.id = autofill::ActorSuggestionId(123);
  request1.suggestions.push_back(suggestion1);

  autofill::ActorFormFillingRequest request2;
  request2.requested_data = RequestedData::kContactInformation;
  autofill::ActorSuggestion suggestion2;
  suggestion2.id = autofill::ActorSuggestionId(234);
  request2.suggestions.push_back(suggestion2);

  std::vector<autofill::ActorFormFillingRequest> requests = {request1,
                                                             request2};

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));

  TestFuture<base::WeakPtr<AutofillSelectionDialogEventHandler>> handler_future;
  ToolDelegate::AutofillSuggestionSelectedCallback captured_callback;
  ActResultFuture result;
  EXPECT_CALL(mock_execution_engine(), RequestToShowAutofillSuggestions)
      .WillOnce([&](auto, auto handler, auto callback) {
        handler_future.SetValue(handler);
        captured_callback = std::move(callback);
      });

  std::unique_ptr<ToolRequest> action = MakeAttemptFormFillingRequest(
      *active_tab(), {PageTarget(*address_home_line1)});
  actor_task().Act(ToRequestList(action), result.GetCallback());

  base::WeakPtr<AutofillSelectionDialogEventHandler> captured_handler =
      handler_future.Take();
  ASSERT_TRUE(captured_handler);

  EXPECT_CALL(mock_form_filling_service(), ScrollToForm(Ref(*active_tab()), 0));
  EXPECT_TRUE(captured_handler->OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParams::New(
          /*form_filling_request_index=*/0)));

  EXPECT_CALL(mock_form_filling_service(), ScrollToForm(Ref(*active_tab()), 1));
  EXPECT_TRUE(captured_handler->OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParams::New(
          /*form_filling_request_index=*/1)));

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kContactInformation, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogPresented,
      1);

  EXPECT_CALL(mock_form_filling_service(),
              FillForm(Ref(*active_tab()), 0,
                       MakeActorFormFillingSelection(suggestion1.id)));
  EXPECT_TRUE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/0,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"123"))));
  EXPECT_CALL(mock_form_filling_service(),
              FillForm(Ref(*active_tab()), 1,
                       MakeActorFormFillingSelection(suggestion2.id)));
  EXPECT_TRUE(captured_handler->OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParams::New(
          /*form_filling_request_index=*/1,
          webui::mojom::FormFillingResponse::New(
              /*suggestion_id=*/"234"))));

  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType",
      RequestedData::kAddress, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType",
      RequestedData::kContactInformation, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Actor.AttemptFormFillingToolEvent",
      form_fill_metrics::AttemptFormFillingToolEvent::kAttentionDialogAccepted,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.AutofillSuggestionsPerDialog", 2, 1);

  std::move(captured_callback).Run(MakeAutofillSuggestionsErrorResponse());
  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingDialogError);
}

}  // namespace

}  // namespace actor
