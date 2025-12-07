// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_conversions.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

// NOTE: A separate suite of UI interaction tests validates the
// end-to-end integration from the Glic client, through the action proto
// conversion, to the execution of this tool.
//
// For Glic integration coverage of these scenarios, see the interactive
// UI tests in the chrome/browser/glic/host/ directory.
class ActorTypeToolBrowserTest : public ActorToolsTest,
                                 public ::testing::WithParamInterface<
                                     ::features::ActorPaintStabilityMode> {
 public:
  ActorTypeToolBrowserTest() {
    auto paint_stability_mode = GetParam();
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kActorPaintStabilityMode.name,
          ::features::kActorPaintStabilityMode.GetName(paint_stability_mode)}});
  }

  ~ActorTypeToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Basic test of the TypeTool - ensure typed string containing composition
// characters is entered into an input box.
// Flaky timeouts on sanitizer builds and in certain debug builds:
// https://crbug.com/453258855
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_TypeTool_TextInputCompositionCharacters \
  DISABLED_TypeTool_TextInputCompositionCharacters
#else
#define MAYBE_TypeTool_TextInputCompositionCharacters \
  TypeTool_TextInputCompositionCharacters
#endif
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       MAYBE_TypeTool_TextInputCompositionCharacters) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string typed_string =
      "Acute: ÁÉÍÓÚÝáéíóúý. Grave: ÀÈÌÒÙàèìòù. Umlaut: ÄËÏÖÜŸäëïöüÿ. "
      "Tilde: ÃÑÕãñõ. Circumflex: ÂÊÎÔÛâêîôû. Cedilla: Çç.";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Basic test of the TypeTool - ensure typed string containing composition
// characters is entered into an input box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_TextInputAltGrCharacter) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string typed_string =
      "Symbols: ¡§©®¶. Currency: ¢£¥. Ordinals: ¹²³. "
      "Letters: ßæåøðþÆÅØÐÞ. Micro: µ.";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Basic test of the TypeTool - ensure typed string is entered into an input
// box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_TextInput) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Basic test of the TypeTool - ensure typed string is entered into an input
// box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_TextInputAnyCharacter) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "你好こんにちはпривет";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Tests that it is possible to type an empty string when the existing field
// is empty (effectively a no-op).
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_TextInputEmptyString) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string empty_string;
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  ASSERT_EQ(empty_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), empty_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(empty_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure that if the page creates and focus on to a new input upon focusing on
// the original target (even if the original target is readonly), type tool will
// continue on to the new input.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_TextInputAtNewlyCreatedNode) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_dynamic_input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // #input3 is set up to be readonly with a click handler that will spawn a
  // clone of itself (#input3-clone) in its place without the readonly tag
  // that's focused and ready to accept input.
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // The input should go to the cloned input while original input remains
  // readonly.
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
  EXPECT_EQ(
      typed_string,
      EvalJs(web_contents(), "document.getElementById('inputclone').value"));
}

// TypeTool fails when target is non-existent.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_NonExistentNode) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), kNonExistentContentNodeId, typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// TypeTool fails when target is disabled.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_DisabledInput) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('input').disabled = true"));

  std::string typed_string = "test";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  {
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                        /*follow_by_enter=*/true);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
    EXPECT_EQ("",
              EvalJs(web_contents(), "document.getElementById('input').value"));
  }

  // Reenable the input and set it to readOnly, the action should now pass but
  // the input value won't change.

  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('input').disabled = false"));
  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('input').readOnly = true"));

  {
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                        /*follow_by_enter=*/true);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("",
              EvalJs(web_contents(), "document.getElementById('input').value"));
  }
}

// Ensure type tool sends the expected events to an input box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string typed_string = "ab";

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // b
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "getStableEventLog()"));
}

// Tests that it is possible to type an empty string (which has the effect of
// deleting any existing value) and the correct events are sent.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_Events_EmptyString) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string empty_string;

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "document.getElementById('input').value = \'pumpkin pie\'"));

  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), empty_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(empty_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
  EXPECT_EQ(
      // backspace
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "getStableEventLog()"));
}

// Ensure type tool sends the expected events to an input box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_EventsForDeadKey) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string typed_string = "Áñ";

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // Á
      "keydown,keyup,keydown,input,keyup,"
      // ñ
      "keydown,keyup,keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "getStableEventLog()"));
}

// Ensure the type tool correctly sends the enter key after input if specified.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_FollowByEnter) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  // Send 'a' followed by enter. Ensure the click event is seen.
  {
    std::string typed_string = "a";
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                        /*follow_by_enter=*/true);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "getStableEventLog()"));

  ASSERT_TRUE(ExecJs(web_contents(), "input_event_log = []"));

  // Send 'b' without an enter. Ensure the click event is _not_ seen.
  {
    std::string typed_string = "b";
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                        /*follow_by_enter=*/false);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // b
      "keydown,input,keyup", EvalJs(web_contents(), "getStableEventLog()"));
}

// Ensure the type tool doesn't fail if the keydown event is handled (page
// called preventDefault).
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_PageHandlesKeyEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> input_id =
      GetDOMNodeId(*main_frame(), "#keyHandlingInput");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Ensure that the default mode is for the type tool to replace any existing
// text in the targeted element.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_ReplacesText) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('input').value = 'foo bar'"));
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure that the type tool still correctly replaces any existing text in the
// targeted element when in a subframe.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_ReplacesTextInSubframe) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/actor/simple_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));

  const GURL subframe_url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  content::RenderFrameHost* subframe =
      content::ChildFrameAt(main_frame(), /*index=*/0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(main_frame()->GetRenderWidgetHost(),
            subframe->GetRenderWidgetHost());

  ASSERT_TRUE(
      ExecJs(subframe, "document.getElementById('input').value = 'foo bar'"));
  std::optional<int> input_id =
      GetDOMNodeIdFromSubframe(*main_frame(), "#iframe", "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*subframe, input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(typed_string,
            EvalJs(subframe, "document.getElementById('input').value"));
}

// Ensure that if the page moves focus immediately to a different input box, the
// type tool correctly operates on the new input box.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_FocusMovesFocus) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Setup the first input box to immediately move focus to the second input
  // box. Ensure the existing text in the second box is replaced.
  ASSERT_TRUE(ExecJs(web_contents(),
                     R"JS(
                        let input = document.getElementById('input');
                        let input2 = document.getElementById('input2');
                        input2.value = 'foo bar';
                        input.addEventListener('focus', () => {
                          input2.focus();
                        });
                      )JS"));
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Since focusing the first input causes the second input to become focused,
  // the tool should operate on the second input.
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input2').value"));
}

// Basic test of the TypeTool coordinate target - ensure typed string is entered
// into a node at the coordinate.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_TextInputAtCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  // Type into coordinate of input box.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "input"));
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*active_tab(), type_point, typed_string,
                        /*follow_by_enter=*/true);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(typed_string,
              EvalJs(web_contents(), "document.getElementById('input').value"));
  }
  // Type into coordinate of editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "editableDiv"));
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*active_tab(), type_point, typed_string,
                        /*follow_by_enter=*/true);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(typed_string,
              EvalJs(web_contents(),
                     "document.getElementById('editableDiv').textContent"));
  }
}

// Ensure the type tool correctly sends the events to element at the
// coordinates.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_EventsSentToCoordinates) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Send event to an editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "editableDiv"));

    // Send 'a'. Ensure a click event is observed first on element at the
    // coordinate.
    std::string typed_string = "a";
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*active_tab(), type_point, typed_string,
                        /*follow_by_enter=*/false);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(
        // click
        "mousedown(" + type_point.ToString() + "),mouseup(" +
            type_point.ToString() + "),click(" + type_point.ToString() +
            "),"
            // a
            "keydown,input,keyup",
        EvalJs(web_contents(), "input_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "input_event_log = []"));

  // Send event to a focusable but not editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "focusableDiv"));

    // Send 'a'. Ensure a click event is observed first on element at the
    // coordinate.
    std::string typed_string = "a";
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*active_tab(), type_point, typed_string,
                        /*follow_by_enter=*/false);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(
        // click
        "mousedown(" + type_point.ToString() + "),mouseup(" +
            type_point.ToString() + "),click(" + type_point.ToString() +
            "),"
            // a
            "keydown,keyup",
        EvalJs(web_contents(), "input_event_log.join(',')"));
  }
}

// Ensure the type tool correctly sends the events to an unfocusable element at
// the coordinates.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_EventsSentToUnfocusableCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Set coordinate to an unfocusable div.

  gfx::Point type_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(web_contents(), "unfocusableDiv"));

  // Send 'a'. Ensure a click event is observed first on element at the
  // coordinate.
  std::string typed_string = "a";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*active_tab(), type_point, typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Only the click is handled by the node at coordinate.
  EXPECT_EQ(
      // click
      "mousedown(" + type_point.ToString() + "),mouseup(" +
          type_point.ToString() + "),click(" + type_point.ToString() + ")",
      EvalJs(web_contents(), "input_event_log.join(',')"));
  // The keydown and keyup event will go to the body now that div is
  // unfocusable.
  EXPECT_EQ(
      // a
      "keydown,keyup",
      EvalJs(web_contents(), "body_input_event_log.join(',')"));
}

// Ensure the type tool will fail if target coordinate is offscreen.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_SentToOffScreenCoordinates) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Send 'a' to an offscreen coordinate and observe failure.
  std::string typed_string = "a";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*active_tab(), gfx::Point(-1, 0), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);

  EXPECT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool can send a type action to a DOMNodeId that isn't
// an editable.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_DomNodeIdTargetsNonEditable) {
  const GURL url = embedded_test_server()->GetURL("/actor/type_non_input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // The focusable div is not an editable context
  std::string typed_string = "abc";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#focusableDiv");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // a
      "keydown[a],keypress[a],keyup[a],"
      // b
      "keydown[b],keypress[b],keyup[b],"
      // c
      "keydown[c],keypress[c],keyup[c]",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool emits events at the expected intervals when typing
// incrementally.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_IncrementalTyping) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorIncrementalTyping)) {
    GTEST_SKIP() << "GlicActorIncrementalTyping feature is disabled";
  }

  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  const std::string_view typed_string = "Test";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Check that the events are what we expect.
  ASSERT_EQ(
      "keydown,input,keyup,"  // T
      "keydown,input,keyup,"  // e
      "keydown,input,keyup,"  // s
      "keydown,input,keyup",  // t
      EvalJs(web_contents(), "getStableEventLog()"));

  base::Value::List timestamps =
      EvalJs(web_contents(), "getStableEventLogTimes()").TakeValue().TakeList();

  // There are 3 events per character (keydown, input, keyup).
  ASSERT_EQ(timestamps.size(), typed_string.length() * 3);

  // Check that the time between events is what we expect.
  for (size_t i = 0; i < typed_string.length(); ++i) {
    const double key_down_ts = timestamps[i * 3].GetDouble();
    const double key_up_ts = timestamps[i * 3 + 2].GetDouble();
    const base::TimeDelta key_down_to_up_delta =
        base::Milliseconds(key_up_ts - key_down_ts);

    // Check the delay between keydown and keyup.
    EXPECT_GE(key_down_to_up_delta, features::kGlicActorKeyDownDuration.Get());

    // Check the delay between this character's keyup and the next character's
    // keydown.
    if (i < typed_string.length() - 1) {
      const double next_key_down_ts = timestamps[(i + 1) * 3].GetDouble();
      const base::TimeDelta key_up_to_down_delta =
          base::Milliseconds(next_key_down_ts - key_up_ts);
      EXPECT_GE(key_up_to_down_delta, features::kGlicActorKeyUpDuration.Get());
    }
  }
}

// Ensure the type tool functions when typing long string. It should boost the
// typing speed but testing the speed is going to be flaky. Ensure we at least
// have coverage that it works.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest,
                       TypeTool_IncrementalTypingLong) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorIncrementalTyping)) {
    GTEST_SKIP() << "GlicActorIncrementalTyping feature is disabled";
  }

  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string typed_string(
      features::kGlicActorIncrementalTypingLongTextThreshold.Get() + 1ul, 'a');
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

class ActorTypeToolBrowserTestWithLongDelay : public ActorTypeToolBrowserTest {
 public:
  ActorTypeToolBrowserTestWithLongDelay() {
    auto paint_stability_mode = GetParam();
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kActorPaintStabilityMode.name,
          ::features::kActorPaintStabilityMode.GetName(paint_stability_mode)},
         {::features::kGlicActorKeyDownDuration.name, "10s"}});
  }
};

// Ensure the type tool functions when typing very long string past the paste
// threshold. The typing delay was set very long so the fact that action
// finishes before time out indicates the usage of direct paste for long text.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTestWithLongDelay,
                       TypeTool_IncrementalTypingLongTextPaste) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorIncrementalTyping)) {
    GTEST_SKIP() << "GlicActorIncrementalTyping feature is disabled";
  }

  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string typed_string(
      features::kGlicActorIncrementalTypingLongTextPasteThreshold.Get() + 1ul,
      'a');
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure the type tool delays the final enter key by the expected amount.
IN_PROC_BROWSER_TEST_P(ActorTypeToolBrowserTest, TypeTool_FollowByEnterDelay) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  const std::string_view typed_string = "x";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Check that the events are what we expect.
  ASSERT_EQ(
      "keydown,input,keyup,"  // x
      "keydown,click,keyup",  // <enter>
      EvalJs(web_contents(), "getStableEventLog()"));

  base::Value::List timestamps =
      EvalJs(web_contents(), "getStableEventLogTimes()").TakeValue().TakeList();

  // 3 events for the 'x' char and 3 events for the <enter>.
  ASSERT_EQ(timestamps.size(), 6ul);

  const double x_key_up_ts = timestamps[2].GetDouble();
  const double enter_key_down_ts = timestamps[3].GetDouble();

  const base::TimeDelta x_up_to_enter_down_delta =
      base::Milliseconds(enter_key_down_ts - x_key_up_ts);

  // Check the delay between keydown and keyup.
  EXPECT_GE(x_up_to_enter_down_delta,
            features::kGlicActorTypeToolEnterDelay.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorTypeToolBrowserTest,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled),
    [](const testing::TestParamInfo<::features::ActorPaintStabilityMode>&
           info) { return DescribePaintStabilityMode(info.param); });

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorTypeToolBrowserTestWithLongDelay,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled),
    [](const testing::TestParamInfo<::features::ActorPaintStabilityMode>&
           info) { return DescribePaintStabilityMode(info.param); });

}  // namespace
}  // namespace actor
