// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/autofill/actor/mock_actor_form_filling_service.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::autofill::ActorFormFillingSelection;
using ::autofill::ActorSuggestion;
using ::autofill::ActorSuggestionId;
using ::base::StringPrintf;
using ::base::test::RunOnceCallback;
using ::glic::test::GlicActorUiTest;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::Truly;

using ::optimization_guide::proto::FormFillingRequest_RequestedData;
using ::optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS;

void SetFormFillingAction(optimization_guide::proto::Action* action,
                          int tab_id,
                          const DomNode& node,
                          FormFillingRequest_RequestedData requested_data) {
  CHECK(action);
  auto* form_filling_action = action->mutable_attempt_form_filling();

  form_filling_action->set_tab_id(tab_id);
  auto* request = form_filling_action->add_form_filling_requests();

  auto* target = request->add_trigger_fields();
  target->set_content_node_id(node.node_id);
  target->mutable_document_identifier()->set_serialized_token(
      node.document_identifier);

  request->set_requested_data(requested_data);
}

// Internal recursive helper to collect text from a subtree.
void GetSubtreeText(const optimization_guide::proto::ContentNode& node,
                    std::string& out) {
  if (node.content_attributes().has_text_data()) {
    if (!out.empty() && out.back() != ' ') {
      out += " ";
    }
    out += node.content_attributes().text_data().text_content();
  }
  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    GetSubtreeText(child, out);
  }
}

// Recursively builds a map from label text to the corresponding input's
// DomNode. We ignore iframes and collect label text from the node's subtree.
//
// This function requires label text to be unique within the `node`.
void FindFormLabelsRecursively(
    const optimization_guide::proto::ContentNode& node,
    const std::string& document_identifier,
    base::flat_map<std::string, DomNode>& label_map) {
  const optimization_guide::proto::ContentAttributes& attrs =
      node.content_attributes();

  if (attrs.has_label_for_dom_node_id()) {
    std::string text;
    GetSubtreeText(node, text);
    text = base::TrimWhitespaceASCII(text, base::TRIM_ALL);
    if (!text.empty()) {
      CHECK(!label_map.contains(text)) << "Test pages must not repeat labels";
      label_map[text] = DomNode{.node_id = attrs.label_for_dom_node_id(),
                                .document_identifier = document_identifier};
    }
  }

  // Since no tests use iframes, we ignore iframes which have a different
  // document identifier.
  if (attrs.attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    return;
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    FindFormLabelsRecursively(child, document_identifier, label_map);
  }
}

// Builds a map from label text to the corresponding input's DomNode. This is
// needed to specify the node easily in tests that use step
// GetDomNodeForLabel().
base::flat_map<std::string, DomNode> BuildFormLabelsMap(
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  base::flat_map<std::string, DomNode> label_map;
  CHECK(apc.has_root_node());
  CHECK(apc.has_main_frame_data());
  FindFormLabelsRecursively(
      apc.root_node(),
      apc.main_frame_data().document_identifier().serialized_token(),
      label_map);
  return label_map;
}

std::string FormLabelsDebugString(
    const base::flat_map<std::string, DomNode>& map) {
  return base::StrCat(
      {"{",
       base::JoinString(base::ToVector(map,
                                       [](const auto& entry) {
                                         return StringPrintf(
                                             "'%s' -> %d @ %s", entry.first,
                                             entry.second.node_id,
                                             entry.second.document_identifier);
                                       }),
                        ", "),
       "}"});
}

content::EvalJsResult EvalJsAt(content::WebContents* contents,
                               std::string_view query_selector,
                               std::string function) {
  std::string query_selector_encoded = content::JsReplace("$1", query_selector);
  return content::EvalJs(
      contents, StringPrintf(R"js(
            (function(){
              const selector = %s;
              const where = document.querySelector(selector);
              if (where == null) {
                throw new Error('selector not found: ' + selector);
              }
              const func = (%s);
              return func(where);
            })()
          )js",
                             query_selector_encoded.c_str(), function.c_str()));
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(ActorTask& task) : ExecutionEngine(task) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(autofill::ActorFormFillingService&,
              GetActorFormFillingService,
              (),
              (override));
};

class GlicActorAttemptFormFillingUiTest : public GlicActorUiTest {
 public:
  GlicActorAttemptFormFillingUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActorAutofill);
  }

  void SetUpOnMainThread() override {
    GlicActorUiTest::SetUpOnMainThread();
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  autofill::MockActorFormFillingService& mock_form_filling_service() {
    return mock_form_filling_service_;
  }

  // Generic helper for checking Js values in Glic.
  template <typename M>
  [[nodiscard]] auto CheckJsInGlicAt(std::string_view query_selector,
                                     const std::string& function,
                                     M&& matcher) {
    return CheckResult(
               [this, query_selector = std::string(query_selector),
                function]() mutable {
                 content::EvalJsResult result = EvalJsAt(
                     GetGlicContents(), query_selector, std::move(function));
                 if (!result.is_ok()) {
                   VLOG(0) << "CheckJsInGlicAt failed: "
                           << result.ExtractError();
                   return base::Value();
                 }
                 return std::move(result).TakeValue();
               },
               matcher)
        .SetDescription(StringPrintf("CheckJsInGlicAt with query selector %s",
                                     query_selector));
  }

  // Generic helper for JS execution in Glic.
  [[nodiscard]] auto ExecuteJsInGlicAt(std::string_view query_selector,
                                       const std::string& function) {
    return Do([this, query_selector = std::string(query_selector), function]() {
      content::EvalJsResult result =
          EvalJsAt(GetGlicContents(), query_selector, std::move(function));
      ASSERT_TRUE(result.is_ok())
          << "ExecuteJsInGlicAt failed: " << result.ExtractError();
    });
  }

  // Generic helper to click in Glic.
  [[nodiscard]] auto ClickInGlicAt(std::string_view query_selector) {
    return ExecuteJsInGlicAt(query_selector, "el => el.click()");
  }

  // Generic helper to set value in Glic.
  [[nodiscard]] auto SetValueInGlicAt(std::string_view query_selector,
                                      std::string_view value) {
    return ExecuteJsInGlicAt(query_selector,
                             content::JsReplace("el => el.value = $1", value));
  }

  // Generic helper to wait for element in Glic.
  [[nodiscard]] auto WaitForElementToExistInGlic(
      std::string_view query_selector) {
    return CheckResult(
        [this, query_selector = std::string(query_selector)]() {
          std::string function = content::JsReplace(R"(
            (function (){
              const selector = $1;
              const polling_interval_milliseconds = 500;
              return new Promise(
                (resolve, _reject) => {
                  const timer_id = setInterval(() => {
                    if (!!document.querySelector(selector)) {
                      resolve(true);
                      clearInterval(timer_id);
                    } else {
                      console.log("Waiting for query selector: " + selector);
                    }
                  }, polling_interval_milliseconds);
                });
            })()
        )",
                                                    query_selector);
          content::EvalJsResult result =
              content::EvalJs(GetGlicContents(), std::move(function));
          if (!result.is_ok()) {
            return result.ExtractError();
          }
          return std::string(result.ExtractBool() ? "present" : "not present");
        },
        "present");
  }

  [[nodiscard]] auto GetApcAndFormLabelsMap() {
    return Steps(GetPageContextForActorTab(), Do([this]() {
                   form_labels_map_ =
                       BuildFormLabelsMap(*annotated_page_content_);
                 }));
  }

  [[nodiscard]] auto CaptureTabHandle(
      ::ui::ElementIdentifier tab_element_identifier) {
    return WithElement(
               tab_element_identifier,
               [this](::ui::TrackedElement* el) {
                 content::WebContents* new_tab_contents =
                     AsInstrumentedWebContents(el)->web_contents();
                 tabs::TabInterface* tab =
                     tabs::TabInterface::GetFromContents(new_tab_contents);
                 CHECK(tab);
                 tab_handle_ = tab->GetHandle();
               })
        .SetDescription("Set the tab handle for GlicActorUiTest");
  }

  // Ensures the glic test page is ready for autofill dialog requests.
  [[nodiscard]] auto VerifyAutofillHandlerIsListening() {
    return CheckJsInGlicAt("#autofill-setup-status", "el => el.textContent",
                           Eq("handler-found"));
  }

  // Waits for the test page to show the dialog request.
  [[nodiscard]] auto WaitForAutofillDialog() {
    return WaitForElementToExistInGlic("#suggestion-input-0")
        .AddDescriptionPrefix("Waiting for Autofill Dialog");
  }

  // Generic helper to check the origin in Glic.
  [[nodiscard]] auto CheckFormattedRequestOriginInGlic(
      int form_index,
      const std::string& expected_origin) {
    return CheckJsInGlicAt(
        StringPrintf("#formatted-request-origin-%d", form_index),
        "el => el.textContent", Eq(expected_origin));
  }

  // Issues an onFormPresented to the AutofillSelectionDialogRequest on the glic
  // api via the glic test page.
  [[nodiscard]] auto PresentForm(int form_index) {
    return Steps(
        Log("Presenting form: ", form_index),
        ClickInGlicAt(StringPrintf("#notify-form-presented-%d", form_index)));
  }

  // Issues an onFormPreview to the AutofillSelectionDialogRequest on the glic
  // api via the glic test page.
  [[nodiscard]] auto ChangeFormPreview(
      int form_index,
      std::optional<ActorSuggestionId> suggestion_id) {
    std::string suggestion_input_value =
        suggestion_id ? StringPrintf("%d", suggestion_id->value()) : "";
    return Steps(
        Log("Changing form preview: form_index=", form_index,
            ", suggestion_id=", suggestion_input_value),
        SetValueInGlicAt(StringPrintf("#suggestion-input-%d", form_index),
                         suggestion_input_value),
        ClickInGlicAt(StringPrintf("#preview-input-btn-%d", form_index)));
  }

  // Issues an onFormConfirmed to the AutofillSelectionDialogRequest on the glic
  // api via the the glic test page.
  [[nodiscard]] auto ConfirmForm(int form_index,
                                 ActorSuggestionId suggestion_id) {
    return Steps(
        Log("Filling form: form_index=", form_index,
            ", suggestion_id=", suggestion_id),
        SetValueInGlicAt(StringPrintf("#suggestion-input-%d", form_index),
                         StringPrintf("%d", suggestion_id.value())),
        ClickInGlicAt(StringPrintf("#confirm-form-btn-%d", form_index)));
  }

  // Issues an onDialogRequest to the AutofillSelectionDialogRequest on the glic
  // api via the glic test page.
  [[nodiscard]] auto SubmitSelections(
      base::span<const ActorSuggestionId> selections) {
    std::string selections_string = base::JoinString(
        base::ToVector(
            selections,
            [](ActorSuggestionId id) { return base::ToString(id.value()); }),
        ",");
    return Steps(
        Log(StringPrintf("Submitting selections: %s",
                         selections_string.c_str())),
        SetValueInGlicAt("#selectedAutofillSuggestionId", selections_string),
        ClickInGlicAt("#sendAutofillSuggestionsResponse"));
  }

  [[nodiscard]] auto SetupTaskForAttemptFormFilling(
      ::ui::ElementIdentifier kNewActorTabId,
      TaskId& task_id,
      const GURL& url) {
    MultiStep steps = Steps(
        InstrumentTab(kNewActorTabId), CaptureTabHandle(kNewActorTabId),
        NavigateWebContents(kNewActorTabId, url),
        OpenGlic(GlicInstrumentMode::kHostAndContents),
        VerifyAutofillHandlerIsListening(),
        CreateTask(/*out_task=*/task_id, "Attempt to fill form (test action)"),
        GetApcAndFormLabelsMap(),
        Log("Done setting up Task for attempt form filling"));
    AddDescriptionPrefix(steps, "Setup Task for AttemptFormFilling");
    return steps;
  }

  // Retrieves the DomNode from the annotated page content given a label
  // associated to the node via `label-for` attributes.
  //
  // The `form_labels_map_` must be populated: See GetApcAndFormLabelsMap().
  [[nodiscard]] auto GetDomNodeForLabel(DomNode& node_out,
                                        std::string_view label) {
    return Do([this, label, &node_out]() {
      auto it = form_labels_map_.find(label);
      ASSERT_NE(it, form_labels_map_.end())
          << "Could not find label '" << label << "' in "
          << FormLabelsDebugString(form_labels_map_);
      node_out = it->second;
    });
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(ActorTask& task) {
    auto engine = std::make_unique<NiceMock<MockExecutionEngine>>(task);
    ON_CALL(*engine, GetActorFormFillingService())
        .WillByDefault(ReturnRef(mock_form_filling_service_));
    return engine;
  }

 private:
  // Maps the text of a node to the node specified by its `label-for`
  // attribute. Used by tests to find dom nodes when buildling the Actions
  // protobuf.
  base::flat_map<std::string, DomNode> form_labels_map_;
  base::test::ScopedFeatureList scoped_feature_list_;
  autofill::MockActorFormFillingService mock_form_filling_service_;
  ScopedExecutionEngineFactory mock_execution_engine_factory_{
      base::BindRepeating(
          &GlicActorAttemptFormFillingUiTest::CreateExecutionEngine,
          base::Unretained(this))};
};

// Simulates an attempt form filling action for a single form filling request.
// Then acts as if the user hovered/focused, unhovered/unfocused and accepted
// the dialog card. These events are expected to be delivered to the (mocked)
// actor form filling service before finally returning a response to the
// initiating PerformActions call that started the action.
IN_PROC_BROWSER_TEST_F(GlicActorAttemptFormFillingUiTest,
                       FillsSingleFormAndSendsEvents) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");

  std::vector<autofill::ActorFormFillingRequest> requests;
  ActorSuggestion& suggestion =
      requests.emplace_back().suggestions.emplace_back();
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
        .WillOnce(RunOnceCallback<2>(requests));

    EXPECT_CALL(mock_form_filling_service(), ScrollToForm(_, /*form_index=*/0));

    EXPECT_CALL(mock_form_filling_service(),
                PreviewForm(_, /*form_index=*/0, ActorSuggestionId(123)));

    EXPECT_CALL(mock_form_filling_service(),
                ClearFormPreview(_, /*form_index=*/0));

    EXPECT_CALL(mock_form_filling_service(),
                PreviewForm(_, /*form_index=*/0, ActorSuggestionId(123)));

    EXPECT_CALL(mock_form_filling_service(),
                FillForm(_, /*form_index=*/0,
                         ActorFormFillingSelection(suggestion.id)));

    EXPECT_CALL(
        mock_form_filling_service(),
        FillSuggestions(
            _, ElementsAre(ActorFormFillingSelection(suggestion.id)), _))
        .WillOnce(RunOnceCallback<2>(base::ok()));
  }
  TaskId task_id;
  DomNode address_field_node;
  std::optional<PerformActionsResultHandle> perform_actions_result_handle;
  RunTestSequence(
      SetupTaskForAttemptFormFilling(kNewActorTabId, task_id, url),
      GetDomNodeForLabel(address_field_node, "Address:"),
      SendExecuteActions(
          perform_actions_result_handle,
          base::BindLambdaForTesting([this, &task_id, &address_field_node]() {
            optimization_guide::proto::Actions actions;
            actions.set_task_id(task_id.value());
            SetFormFillingAction(actions.add_actions(), tab_handle_.raw_value(),
                                 address_field_node,
                                 FormFillingRequest_RequestedData_ADDRESS);
            return EncodeActionProto(actions);
          })),
      WaitForAutofillDialog(),
      // The UI immediately presents the form.
      PresentForm(/*form_index=*/0),
      // The user hovers/focuses the card.
      ChangeFormPreview(/*form_index=*/0, suggestion.id),
      // The user unhovers/unfocuses the card.
      ChangeFormPreview(/*form_index=*/0, /*suggestion_id=*/std::nullopt),
      // The user hovers/focuses the card.
      ChangeFormPreview(/*form_index=*/0, suggestion.id),
      // The user clicks the confirm button.
      ConfirmForm(/*form_index=*/0, suggestion.id),
      // Since this is the last (only) form the dialog response is sent.
      SubmitSelections({suggestion.id}),
      CheckExecuteActionsResultHandle(perform_actions_result_handle));
}

IN_PROC_BROWSER_TEST_F(GlicActorAttemptFormFillingUiTest,
                       OriginIsFormattedViaUrlFormatter) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/autofill/autofill_test_form.html");

  ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "My Address";

  std::vector<autofill::ActorFormFillingRequest> requests;
  // 0. Standard non-IDN domain.
  requests.emplace_back().request_origin =
      url::Origin::Create(GURL("https://www.example.test"));
  requests.back().suggestions.push_back(suggestion);

  // 1. IDN Chinese.
  requests.emplace_back().request_origin =
      url::Origin::Create(GURL("http://\xe4\xb8\xad\xe5\x9b\xbd.icom.museum"));
  requests.back().suggestions.push_back(suggestion);

  // 2. IDN Hebrew RTL.
  requests.emplace_back().request_origin = url::Origin::Create(
      GURL("http://\xd7\x90\xd7\x99\xd7\xa7\xd7\x95\xd7\xb4\xd7\x9d."
           "\xd7\x99\xd7\xa9\xd7\xa8\xd7\x90\xd7\x9c.museum/"));
  requests.back().suggestions.push_back(suggestion);

  EXPECT_CALL(mock_form_filling_service(), GetSuggestions)
      .WillOnce(RunOnceCallback<2>(requests));
  EXPECT_CALL(mock_form_filling_service(), FillSuggestions)
      .WillOnce(RunOnceCallback<2>(base::ok()));

  TaskId task_id;
  DomNode address_field_node;
  std::optional<PerformActionsResultHandle> perform_actions_result_handle;
  RunTestSequence(
      SetupTaskForAttemptFormFilling(kNewActorTabId, task_id, url),
      GetDomNodeForLabel(address_field_node, "Address:"),
      SendExecuteActions(
          perform_actions_result_handle,
          base::BindLambdaForTesting([this, &task_id, &address_field_node]() {
            optimization_guide::proto::Actions actions;
            actions.set_task_id(task_id.value());
            SetFormFillingAction(actions.add_actions(), tab_handle_.raw_value(),
                                 address_field_node,
                                 FormFillingRequest_RequestedData_ADDRESS);
            return EncodeActionProto(actions);
          })),
      WaitForAutofillDialog(),
      // Verify the formatted request origin does not contain the scheme.
      CheckFormattedRequestOriginInGlic(/*form_index=*/0, "www.example.test"),
      // Verify the Chinese IDN request origin is formatted to unicode.
      CheckFormattedRequestOriginInGlic(/*form_index=*/1, "中国.icom.museum"),
      // Verify the Hebrew RTL IDN request origin is formatted as punycode.
      CheckFormattedRequestOriginInGlic(/*form_index=*/2,
                                        "xn--4dbklr2c8d.xn--4dbrk0ce.museum"),
      // Close the dialog by submitting selections.
      SubmitSelections({ActorSuggestionId(123), ActorSuggestionId(123),
                        ActorSuggestionId(123)}),
      // Wait for completion, so that mock expectations are checked after the
      // action has completed.
      CheckExecuteActionsResultHandle(perform_actions_result_handle));
}

}  // namespace
}  // namespace actor
