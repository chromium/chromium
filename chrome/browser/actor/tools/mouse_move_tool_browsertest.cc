// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

class ActorMouseMoveToolBrowserTest : public ActorToolsTest {
 public:
  ActorMouseMoveToolBrowserTest() = default;
  ~ActorMouseMoveToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// Test the MouseMove tool fails on a non-existent content node.
IN_PROC_BROWSER_TEST_F(ActorMouseMoveToolBrowserTest,
                       MouseMoveTool_NonExistentNode) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Use a random node id that doesn't exist.
  std::unique_ptr<ToolRequest> action =
      MakeMouseMoveRequest(*main_frame(), kNonExistentContentNodeId);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
}

// Test basic movements using MouseMove tool generates the expected events.
IN_PROC_BROWSER_TEST_F(ActorMouseMoveToolBrowserTest, MouseMoveTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #first DIV
  {
    std::optional<int> first_id = GetDOMNodeId(*main_frame(), "#first");
    std::unique_ptr<ToolRequest> action =
        MakeMouseMoveRequest(*main_frame(), first_id.value());

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ("mouseenter[DIV#first],mousemove[DIV#first]",
            EvalJs(web_contents(), "event_log.join(',')"));
  ASSERT_TRUE(ExecJs(web_contents(), "event_log = []"));

  // Move mouse over #second DIV
  {
    std::optional<int> second_id = GetDOMNodeId(*main_frame(), "#second");
    std::unique_ptr<ToolRequest> action =
        MakeMouseMoveRequest(*main_frame(), second_id.value());

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      "mouseleave[DIV#first],mouseenter[DIV#second],mousemove[DIV#second]",
      EvalJs(web_contents(), "event_log.join(',')"));
}

// Test mouse move causes scrolling if the target is offscreen.
IN_PROC_BROWSER_TEST_F(ActorMouseMoveToolBrowserTest,
                       MouseMoveTool_TargetOutsideViewport) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));
  // Page starts unscrolled
  ASSERT_EQ(0, EvalJs(web_contents(), "window.scrollY"));

  // Move mouse over #offscreen DIV. It should succeed since it is first
  // scrolled into the viewport.
  {
    std::optional<int> offscreen_id = GetDOMNodeId(*main_frame(), "#offscreen");
    std::unique_ptr<ToolRequest> action =
        MakeMouseMoveRequest(*main_frame(), offscreen_id.value());

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  EXPECT_EQ("mouseenter[DIV#offscreen],mousemove[DIV#offscreen]",
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Ensure mouse can be moved to a coordinate onscreen.
IN_PROC_BROWSER_TEST_F(ActorMouseMoveToolBrowserTest,
                       MouseMoveTool_MoveToCoordinate) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #first DIV
  gfx::Point move_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(web_contents(), "first"));
  std::unique_ptr<ToolRequest> action =
      MakeMouseMoveRequest(*active_tab(), move_point);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("mouseenter[DIV#first],mousemove[DIV#first]",
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Moving mouse to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorMouseMoveToolBrowserTest,
                       MouseMoveTool_MoveToCoordinateOffScreen) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #offscreen DIV. This should fail since #offscreen is
  // outside the viewport.
  {
    gfx::Point move_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    std::unique_ptr<ToolRequest> action =
        MakeMouseMoveRequest(*active_tab(), move_point);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
  }

  // The action should fail without generating any events.
  EXPECT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));
}

}  // namespace
}  // namespace actor
