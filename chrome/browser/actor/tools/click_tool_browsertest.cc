// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;
using content::RenderFrameHost;

namespace actor {

namespace {

// Basic test to ensure sending a click to an element works.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to the document body.
  {
    std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
    ASSERT_TRUE(body_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), body_id.value());
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("mousedown[BODY#],mouseup[BODY#],click[BODY#]",
              EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to the button.
  {
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), "button#clickable");
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#clickable],mouseup[BUTTON#clickable],click[BUTTON#"
        "clickable]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to an element that doesn't exist fails.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_NonExistentElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), kNonExistentContentNodeId);
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to a disabled element should fail without dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_DisabledElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "button#disabled");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementDisabled);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to an element that's not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_OffscreenElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementOffscreen);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Ensure clicks can be sent to elements that are only partially onscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_ClippedElements) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_with_overflow_clip.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<std::string> test_cases = {
      "offscreenButton", "overflowHiddenButton", "overflowScrollButton"};

  for (auto button : test_cases) {
    SCOPED_TRACE(testing::Message() << "WHILE TESTING: " << button);
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), base::StrCat({"#", button}));
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(button, EvalJs(web_contents(), "clicked_button"));

    ASSERT_TRUE(ExecJs(web_contents(), "clicked_button = ''"));
  }
}

// Ensure clicks can be sent to a coordinate onscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a (0,0) coordinate inside the document.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), gfx::Point(0, 0));
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("mousedown[HTML#],mouseup[HTML#],click[HTML#]",
              EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to a coordinate on the button.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "clickable"));

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#clickable],mouseup[BUTTON#clickable],click[BUTTON#"
        "clickable]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToCoordinateOffScreen) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a negative coordinate offscreen.
  {
    gfx::Point negative_offscreen = {-1, 0};
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), negative_offscreen);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_fail;
    actor_task().Act(ToRequestList(action), result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);

    // The page should not have received any events.
    EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  // Send a click to a positive coordinate offscreen.
  {
    gfx::Point positive_offscreen = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), positive_offscreen);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_fail;
    actor_task().Act(ToRequestList(action), result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);
    // The page should not have received any events.
    EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }
}

// Ensure click is using viewport coordinate.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_ViewportCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Scroll the window by 100vh so #offscreen button is in viewport.
  ASSERT_TRUE(ExecJs(web_contents(), "window.scrollBy(0, window.innerHeight)"));

  // Send a click to button's viewport coordinate.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#offscreen],mouseup[BUTTON#offscreen],click[BUTTON#"
        "offscreen]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "offscreen_button_clicked"));
  }
}

// Ensure click works correctly when clicking on a cross process iframe using a
// DomNodeId
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_Subframe_DomNodeId) {
  // This test only applies if cross-origin frames are put into separate
  // processes.
  if (!content::AreAllSitesIsolatedForTesting()) {
    GTEST_SKIP();
  }

  const GURL url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "bar.com", "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());

  // Send a click to the button in the subframe.
  std::optional<int> button_id = GetDOMNodeId(*subframe, "button#clickable");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*subframe, button_id.value());

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

// Ensure that page tools (click is arbitrary here) correctly add the acted on
// tab to the task's tab set.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Send a click to the document body.
  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), body_id.value());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

}  // namespace
}  // namespace actor
