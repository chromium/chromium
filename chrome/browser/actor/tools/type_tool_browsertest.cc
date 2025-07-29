// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "ui/gfx/geometry/point_conversions.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

// Basic test of the TypeTool - ensure typed string is entered into an input
// box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_TextInput) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure that if the page creates and focus on to a new input upon focusing on
// the original target (even if the original target is readonly), type tool will
// continue on to the new input.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_TextInputAtNewlyCreatedNode) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_NonExistentNode) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), kNonExistentContentNodeId, typed_string,
                      /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// TypeTool fails when target is disabled.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_DisabledInput) {
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
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("",
              EvalJs(web_contents(), "document.getElementById('input').value"));
  }
}

// Ensure type tool sends the expected events to an input box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_Events) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // b
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,change,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool can be used without text to send an enter key in an
// input.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_EmptyText) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string typed_string = "";

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool correctly sends the enter key after input if specified.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_FollowByEnter) {
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

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,change,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));

  ASSERT_TRUE(ExecJs(web_contents(), "input_event_log = []"));

  // Send 'b' without an enter. Ensure the click event is _not_ seen.
  {
    std::string typed_string = "b";
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                        /*follow_by_enter=*/false);

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // b
      "keydown,input,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool doesn't fail if the keydown event is handled (page
// called preventDefault).
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_PageHandlesKeyEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> input_id =
      GetDOMNodeId(*main_frame(), "#keyHandlingInput");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), typed_string,
                      /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Ensure that the default mode is for the type tool to replace any existing
// text in the targeted element.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_ReplacesText) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure that if the page moves focus immediately to a different input box, the
// type tool correctly operates on the new input box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_FocusMovesFocus) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_TextInputAtCoordinate) {
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

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(typed_string,
              EvalJs(web_contents(),
                     "document.getElementById('editableDiv').textContent"));
  }
}

// Ensure the type tool correctly sends the events to element at the
// coordinates.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_EventsSentToCoordinates) {
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

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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

    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
IN_PROC_BROWSER_TEST_F(ActorToolsTest,
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_SentToOffScreenCoordinates) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);

  EXPECT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool can send a type action to a DOMNodeId that isn't
// an editable.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_DomNodeIdTargetsNonEditable) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
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
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_IncrementalTyping) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Check that the events are what we expect.
  ASSERT_EQ(
      "keydown,input,keyup,"  // T
      "keydown,input,keyup,"  // e
      "keydown,input,keyup,"  // s
      "keydown,input,keyup",  // t
      EvalJs(web_contents(), "input_event_log.join(',')"));

  base::Value::List timestamps =
      EvalJs(web_contents(), "input_event_log_times").TakeValue().TakeList();

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

}  // namespace
}  // namespace actor
