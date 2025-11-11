// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using apc::TypeAction;

class GlicActorTypeToolUiTest : public GlicActorUiTest {
 public:
  GlicActorUiTest::MultiStep TypeAction(
      std::string_view label,
      std::string_view text,
      optimization_guide::proto::TypeAction::TypeMode mode,
      bool follow_by_enter,
      actor::TaskId& task_id,
      tabs::TabHandle& tab_handle,
      ExpectedErrorResult expected_result = {});

  GlicActorUiTest::MultiStep TypeAction(
      std::string_view label,
      std::string_view text,
      optimization_guide::proto::TypeAction::TypeMode mode,
      ExpectedErrorResult expected_result = {});

  GlicActorUiTest::MultiStep TypeAction(
      const gfx::Point& coordinate,
      std::string_view text,
      optimization_guide::proto::TypeAction::TypeMode mode,
      ExpectedErrorResult expected_result = {});
};

GlicActorUiTest::MultiStep GlicActorTypeToolUiTest::TypeAction(
    std::string_view label,
    std::string_view text,
    optimization_guide::proto::TypeAction::TypeMode mode,
    bool follow_by_enter,
    actor::TaskId& task_id,
    tabs::TabHandle& tab_handle,
    ExpectedErrorResult expected_result) {
  auto type_provider = base::BindLambdaForTesting(
      [this, &task_id, &tab_handle, label, text, mode, follow_by_enter]() {
        int32_t node_id = SearchAnnotatedPageContent(label);
        content::RenderFrameHost* frame =
            tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
        Actions action =
            actor::MakeType(*frame, node_id, text, follow_by_enter, mode);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(type_provider), std::move(expected_result));
}

GlicActorUiTest::MultiStep GlicActorTypeToolUiTest::TypeAction(
    std::string_view label,
    std::string_view text,
    optimization_guide::proto::TypeAction::TypeMode mode,
    ExpectedErrorResult expected_result) {
  return TypeAction(label, text, mode, /*follow_by_enter=*/false, task_id_,
                    tab_handle_, std::move(expected_result));
}

GlicActorUiTest::MultiStep GlicActorTypeToolUiTest::TypeAction(
    const gfx::Point& coordinate,
    std::string_view text,
    optimization_guide::proto::TypeAction::TypeMode mode,
    ExpectedErrorResult expected_result) {
  auto type_provider =
      base::BindLambdaForTesting([this, &coordinate, text, mode]() {
        Actions action = actor::MakeType(tab_handle_, coordinate, text,
                                         /*follow_by_enter=*/false, mode);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(type_provider), std::move(expected_result));
}

// TODO(crbug.com/409570203): Add Tests for other cases once they are
// implemented. Currently uses DELETE_EXISTING behavior in all cases.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest, BasicTypeActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kExpectedText = "Hello Standard Input";
  const std::string kElementLabel = "test-input";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),
      TypeAction(kElementLabel, kExpectedText,
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING),
      // Verify the text was typed into the correct field.
      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('input2').value",
                      kExpectedText));
}

// Ensures that existing text in the input is deleted before typing.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionDeleteExistingSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kExpectedText = "This Should Be The Only Text";
  const std::string kElementLabel = "test-input";
  const std::string kInitialText = "This Should Not Appear";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      // Inject some initial text that should be deleted.
      ExecuteJs(kTypingTestTabId,
                content::JsReplace(
                    "() => { document.getElementById('input2').value = $1; }",
                    kInitialText)),
      GetPageContextForActorTab(),
      TypeAction(kElementLabel, kExpectedText,
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING),

      // Verify the text was typed into the correct field.
      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('input2').value",
                      kExpectedText));
}

IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionOnDisabledInputFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kElementLabel = "disabled-input";

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kTypingTestTabId),
                  GetPageContextForActorTab(),
                  // Attempt to type, expecting the specific error code.
                  TypeAction(kElementLabel, "should fail",
                             optimization_guide::proto::TypeAction::TypeMode::
                                 TypeAction_TypeMode_DELETE_EXISTING,
                             actor::mojom::ActionResultCode::kElementDisabled));
}

// Tests that attempting to type using a label that doesn't exist fails.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionOnNonExistentNodeFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");

  auto type_provider = base::BindLambdaForTesting([this]() {
    content::RenderFrameHost* frame =
        tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
    const std::string kText = "this should fail";

    Actions action = actor::MakeType(
        *frame, kNonExistentContentNodeId, kText, /*follow_by_enter=*/false,
        optimization_guide::proto::TypeAction::TypeMode::
            TypeAction_TypeMode_DELETE_EXISTING);

    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      ExecuteAction(std::move(type_provider),
                    actor::mojom::ActionResultCode::kInvalidDomNodeId));
}

class GlicActorTypeToolUiTestWithoutMultiInstance
    : public GlicActorTypeToolUiTest {
 public:
  GlicActorTypeToolUiTestWithoutMultiInstance() {
    feature_list_.InitAndDisableFeature(features::kGlicMultiInstance);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that if focusing the target element causes focus to move to a
// different element, the type action correctly types into that new element.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTestWithoutMultiInstance,
                       TypeActionOnFocusRedirectSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kExpectedText = "Should be typed in input2";
  const std::string kElementLabel = "test-input";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      WaitForWebContentsReady(kTypingTestTabId),
      ExecuteJs(kTypingTestTabId,
                R"JS(
                 () => {
                   let input1 = document.getElementById('input');
                   let input2 = document.getElementById('input2');
                   input2.addEventListener('focus', () => {
                     input1.focus();
                   });
                 }
               )JS"),
      GetPageContextForActorTab(),
      TypeAction(kElementLabel, kExpectedText,
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING),

      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('input').value",
                      kExpectedText),

      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('input2').value", ""));
}

// Tests that typing at coordinates succeed
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest, TypeActionCoordinatesSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");

  const std::string_view kTypedString = "test";

  gfx::Rect current_bounds;
  auto type_in_current_bounds = base::BindLambdaForTesting(
      // Capture the rect by reference to get its value at execution time.
      [this, &current_bounds, kTypedString]() {
        gfx::Point coordinate = current_bounds.CenterPoint();
        Actions action =
            actor::MakeType(tab_handle_, coordinate, kTypedString,
                            /*follow_by_enter=*/false,
                            optimization_guide::proto::TypeAction::TypeMode::
                                TypeAction_TypeMode_DELETE_EXISTING);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),
      // First test case - input in a form
      GetClientRect(kTypingTestTabId, "input", current_bounds),
      ExecuteAction(type_in_current_bounds),
      WaitForJsResult(kTypingTestTabId,
                      "()=>document.getElementById('input').value",
                      kTypedString),
      // Second test case - editable div
      GetClientRect(kTypingTestTabId, "editableDiv", current_bounds),
      ExecuteAction(type_in_current_bounds),
      WaitForJsResult(kTypingTestTabId,
                      "()=>document.getElementById('editableDiv').textContent",
                      kTypedString),
      // Third test case - input obscured by pseudo-element. Just try to click
      // and type on the container.
      GetPageContextForActorTab(),
      GetClientRect(kTypingTestTabId, "pseudoContainer", current_bounds),
      ExecuteAction(type_in_current_bounds),
      WaitForJsResult(kTypingTestTabId,
                      "()=>document.getElementById('pseudoInput').value",
                      kTypedString));
}

// Tests that attempting to type at an out-of-bounds coordinate fails.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionOffScreenCoordinateFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),

      // Attempt to type at an impossible coordinate.
      TypeAction(gfx::Point(-1, -1), "should fail",
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING,
                 actor::mojom::ActionResultCode::kCoordinatesOutOfBounds));
}

// Tests typing on an element that replaces itself with a new input on
// focus/click.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionOnDynamicNodeSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/type_dynamic_input.html");
  const std::string kExpectedText = "abc";
  const std::string kElementLabel = "dynamic-input";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),

      // Action targets the original readonly input. The tool will click it,
      // the page script will replace it with '#inputclone' and focus that.
      // The tool should detect this and type into '#inputclone'.
      TypeAction(kElementLabel, kExpectedText,
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING),

      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('inputclone').value",
                      kExpectedText));
}

// Tests that typing into an input that intercepts key events via
// 'preventDefault()' still succeeds. This confirms that the TypeAction is
// firing real key events that the page's script can listen for, rather
// than just setting the element's .value property..
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionWithPageKeyHandlerSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kExpectedText = "Hello Key Handler";
  const std::string kElementLabel = "key-handling-input";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),
      // The element 'keyHandlingInput' has JS listeners that manually build the
      // value from key events. This test proves our action triggers those
      // events.
      TypeAction(kElementLabel, kExpectedText,
                 optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING),

      // Verify the JS keydown handler correctly built the value.
      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('keyHandlingInput').value",
                      kExpectedText));
}

// Verifies that a coordinate-based TypeAction also correctly fires key events
// that a 'preventDefault' listener can capture.
IN_PROC_BROWSER_TEST_F(GlicActorTypeToolUiTest,
                       TypeActionByCoordinateWithPageKeyHandlerSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTypingTestTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/input.html");
  const std::string kExpectedText = "Hello Coordinate";

  // Declare a variable to hold the element's coordinates.
  gfx::Rect handler_input_bounds;

  // Create the action provider. This lambda is executed by Step B below.
  auto type_provider = base::BindLambdaForTesting(
      // Capture the rect by reference to get its value at execution time.
      [this, &handler_input_bounds, kExpectedText]() {
        gfx::Point coordinate = handler_input_bounds.CenterPoint();
        Actions action =
            actor::MakeType(tab_handle_, coordinate, kExpectedText,
                            /*follow_by_enter=*/false,
                            optimization_guide::proto::TypeAction::TypeMode::
                                TypeAction_TypeMode_DELETE_EXISTING);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTypingTestTabId),
      GetPageContextForActorTab(),
      GetClientRect(kTypingTestTabId, "keyHandlingInput", handler_input_bounds),
      ExecuteAction(std::move(type_provider)),
      WaitForJsResult(kTypingTestTabId,
                      "() => document.getElementById('keyHandlingInput').value",
                      kExpectedText));
}

}  // namespace

}  // namespace glic::test
