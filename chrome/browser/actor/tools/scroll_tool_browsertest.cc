// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

class ActorScrollToolBrowserTest : public ActorToolsTest {
 public:
  ActorScrollToolBrowserTest() = default;
  ~ActorScrollToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_FailOnInvalidNodeID) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  float scroll_offset_y = 50;
  std::unique_ptr<ToolRequest> action =
      MakeScrollRequest(*main_frame(), kNonExistentContentNodeId,
                        /*scroll_offset_x=*/0, scroll_offset_y);

  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

// Test scrolling the viewport vertically.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_ScrollPageVertical) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_y = 50;

  {
    // If no node id is passed, it will scroll the page's viewport.
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), /*content_node_id=*/std::nullopt,
                          /*scroll_offset_x=*/0, scroll_offset_y);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_y, EvalJs(web_contents(), "window.scrollY"));
  }

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), /*content_node_id=*/std::nullopt,
                          /*scroll_offset_x=*/0, scroll_offset_y);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(2 * scroll_offset_y, EvalJs(web_contents(), "window.scrollY"));
  }
}

// Test scrolling the viewport horizontally.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_ScrollPageHorizontal) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_x = 50;

  {
    // If no node id is passed, it will scroll the page's viewport.
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(),
                          /*content_node_id=*/std::nullopt, scroll_offset_x,
                          /*scroll_offset_y=*/0);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_x, EvalJs(web_contents(), "window.scrollX"));
  }

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(),
                          /*content_node_id=*/std::nullopt, scroll_offset_x,
                          /*scroll_offset_y=*/0);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(2 * scroll_offset_x, EvalJs(web_contents(), "window.scrollX"));
  }
}

// Test scrolling in a sub-scroller on the page.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest, ScrollTool_ScrollElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_x = 50;
  int scroll_offset_y = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller, scroll_offset_x,
                          /*scroll_offset_y=*/0);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_x,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollLeft"));
  }

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_y);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_y,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Test scrolling over a non-scrollable element returns failure.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest, ScrollTool_NonScrollable) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_y = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#nonscroll").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_y);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kScrollTargetNotUserScrollable);
    EXPECT_EQ(0, EvalJs(web_contents(),
                        "document.getElementById('nonscroll').scrollTop"));
    EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
  }
}

// Test scrolling a scroller that's currently offscreen. It will first be
// scrolled into view then scroll applied.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_OffscreenScrollable) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Page starts unscrolled
  ASSERT_EQ(0, EvalJs(web_contents(), "window.scrollY"));

  int scroll_offset_y = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#offscreenscroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_y);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(scroll_offset_y,
              EvalJs(web_contents(),
                     "document.getElementById('offscreenscroller').scrollTop"));
    EXPECT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  }
}

// Test that a scrolling over a scroller with overflow in one axis only works
// correctly.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest, ScrollTool_OneAxisScroller) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#horizontalscroller").value();

  // Try a vertical scroll - it should fail since the scroller has only
  // horizontal overflow.
  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kScrollTargetNotUserScrollable);
    EXPECT_EQ(
        0, EvalJs(web_contents(),
                  "document.getElementById('horizontalscroller').scrollTop"));
    EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
  }

  // Horizontal scroll should succeed.
  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller, scroll_offset,
                          /*scroll_offset_y=*/0);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(
        scroll_offset,
        EvalJs(web_contents(),
               "document.getElementById('horizontalscroller').scrollLeft"));
  }
}

// Ensure scroll distances are correctly scaled when browser zoom is applied.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest, ScrollTool_BrowserZoom) {
  // Set the default browser page zoom to 150%.
  double level = blink::ZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 DIPs translates to 40 CSS pixels when the zoom factor is 1.5
  // (3 DIPs : 2 CSS Pixels)
  int scroll_offset_dips = 60;
  int expected_offset_css = 40;
  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_dips);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Ensure scroll distances are correctly scaled when applied to a CSS zoomed
// scroller.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest, ScrollTool_CSSZoom) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 DIPs translates to 120 CSS pixels since the scroller is
  // inside a `zoom:0.5` subtree (1 DIPs : 2 CSS Pixels)
  int scroll_offset_dips = 60;
  int expected_offset_css = 120;
  int scroller = GetDOMNodeId(*main_frame(), "#zoomedscroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_dips);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('zoomedscroller').scrollTop"));
  }
}

class ActorToolsTestDSF2 : public ActorScrollToolBrowserTest {
 public:
  ActorToolsTestDSF2() = default;
  explicit ActorToolsTestDSF2(const ActorToolsTestDSF2&) = delete;
  ActorToolsTestDSF2& operator=(const ActorToolsTestDSF2&) = delete;
  ~ActorToolsTestDSF2() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

// Ensure scroll distances are correctly scaled when using a non-1 device scale
// factor
IN_PROC_BROWSER_TEST_F(ActorToolsTestDSF2, ScrollTool_ScrollDSF) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 40 dips translates to 40 CSS pixels
  int scroll_offset_dips = 40;
  int expected_offset_css = 40;
  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_dips);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Ensure scroll distances are correctly scaled when browser zoom is applied.
IN_PROC_BROWSER_TEST_F(ActorToolsTestDSF2, ScrollTool_BrowserZoom) {
  // Set the default browser page zoom to 150%.
  double level = blink::ZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 DIPs translates to 40 CSS pixels when the zoom factor is 1.5
  // (3 DIPs : 2 CSS Pixels)
  int scroll_offset_dips = 60;
  int expected_offset_css = 40;
  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_dips);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Ensure scroll distances are correctly scaled when applied to a CSS zoomed
// scroller.
IN_PROC_BROWSER_TEST_F(ActorToolsTestDSF2, ScrollTool_CSSZoom) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 DIPs translates to 120 CSS pixels since the scroller is
  // inside a `zoom:0.5` subtree (1 DIPs : 2 CSS Pixels)
  int scroll_offset_dips = 60;
  int expected_offset_css = 120;
  int scroller = GetDOMNodeId(*main_frame(), "#zoomedscroller").value();

  {
    std::unique_ptr<ToolRequest> action =
        MakeScrollRequest(*main_frame(), scroller,
                          /*scroll_offset_x=*/0, scroll_offset_dips);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('zoomedscroller').scrollTop"));
  }
}

IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_ZeroIdTargetsViewport) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // DOMNodeIDs start at 1 so 0 should be interpreted as viewport.
  constexpr int kViewportId = 0;
  float scroll_offset_y = 50;
  std::unique_ptr<ToolRequest> action =
      MakeScrollRequest(*main_frame(), kViewportId,
                        /*scroll_offset_x=*/0, scroll_offset_y);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Not sure why, since all zooms should be exactly 1.0, but some numerical
  // instability seems to creep in. Using ExtractDouble and EXPECT_FLOAT_EQ for
  // that reason.
  EXPECT_FLOAT_EQ(scroll_offset_y,
                  EvalJs(web_contents(), "window.scrollY").ExtractDouble());
}

// Test that a scroll on a page with scroll-behavior:smooth returns success if
// an animation was started, even though it may not have instantly scrolled.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_SmoothScrollSucceeds) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  float scroll_offset_y = 300;
  int scroller = GetDOMNodeId(*main_frame(), "#smoothscroller").value();
  std::unique_ptr<ToolRequest> action =
      MakeScrollRequest(*main_frame(), scroller,
                        /*scroll_offset_x=*/0, scroll_offset_y);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Test that a scroll on a page with scroll-behavior:smooth returns failure if
// trying to scroll in a direction with no scrollable extent.
IN_PROC_BROWSER_TEST_F(ActorScrollToolBrowserTest,
                       ScrollTool_SmoothScrollAtExtent) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Scroll to the scroller's full extent.
  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.querySelector('#smoothscroller').scrollTo({top:"
                     "10000, behavior:'instant'})"));

  float scroll_offset_y = 300;
  int scroller = GetDOMNodeId(*main_frame(), "#smoothscroller").value();
  std::unique_ptr<ToolRequest> action =
      MakeScrollRequest(*main_frame(), scroller,
                        /*scroll_offset_x=*/0, scroll_offset_y);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScrollOffsetDidNotChange);
}

}  // namespace
}  // namespace actor
