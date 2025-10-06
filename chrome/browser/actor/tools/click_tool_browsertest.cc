// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;
using content::RenderFrameHost;

namespace actor {

namespace {

class ActorClickToolBrowserTest
    : public ActorToolsTest,
      public ::testing::WithParamInterface<
          std::tuple<::features::ActorPaintStabilityMode,
                     ::features::ActorGeneralPageStabilityMode>> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [paint_stability_mode, general_page_stability_mode] = info.param;
    std::stringstream params_description;
    params_description << DescribePaintStabilityMode(paint_stability_mode)
                       << "_"
                       << DescribeGeneralPageStabilityMode(
                              general_page_stability_mode);
    return params_description.str();
  }

  ActorClickToolBrowserTest() {
    auto [paint_stability_mode, general_page_stability_mode] = GetParam();
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kActorPaintStabilityMode.name,
          ::features::kActorPaintStabilityMode.GetName(paint_stability_mode)},
         {::features::kActorGeneralPageStabilityMode.name,
          ::features::kActorGeneralPageStabilityMode.GetName(
              general_page_stability_mode)},
         {features::kGlicActorClickDelay.name, "200ms"}});
  }

  ~ActorClickToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Basic test to ensure sending a click to an element works.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_SentToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to the document body.
  {
    std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
    ASSERT_TRUE(body_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), body_id.value());
    ActResultFuture result;
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
    ActResultFuture result;
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
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_NonExistentElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), kNonExistentContentNodeId);
  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to a disabled element should fail without dispatching events.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_DisabledElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "button#disabled");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementDisabled);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to an element that's not in the viewport should cause it to
// first be scrolled into view then clicked.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_OffscreenElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Page starts unscrolled
  ASSERT_EQ(0, EvalJs(web_contents(), "window.scrollY"));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Page is now scrolled.
  ASSERT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  // The page should not have received any events.
  EXPECT_EQ(
      "mousedown[BUTTON#offscreen],"
      "mouseup[BUTTON#offscreen],"
      "click[BUTTON#offscreen]",
      EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Ensure clicks can be sent to elements that are only partially onscreen.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_ClippedElements) {
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
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(button, EvalJs(web_contents(), "clicked_button"));

    ASSERT_TRUE(ExecJs(web_contents(), "clicked_button = ''"));
  }
}

// Ensure clicks can be sent to a coordinate onscreen.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_SentToCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a (0,0) coordinate inside the document.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), gfx::Point(0, 0));
    ActResultFuture result;
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
    ActResultFuture result;
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
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_SentToCoordinateOffScreen) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a negative coordinate offscreen.
  {
    gfx::Point negative_offscreen = {-1, 0};
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), negative_offscreen);
    ActResultFuture result_fail;
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
    ActResultFuture result_fail;
    actor_task().Act(ToRequestList(action), result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);
    // The page should not have received any events.
    EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }
}

// Ensure click is using viewport coordinate.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_ViewportCoordinate) {
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
    ActResultFuture result;
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
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_Subframe_DomNodeId) {
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

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

// Ensure that page tools (click is arbitrary here) correctly add the acted on
// tab to the task's tab set.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Send a click to the document body.
  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), body_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_Delay) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), body_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const double mousedown_timestamp =
      EvalJs(main_frame(), "mouse_event_timestamps[0]").ExtractDouble();
  const double mouseup_timestamp =
      EvalJs(main_frame(), "mouse_event_timestamps[1]").ExtractDouble();
  const base::TimeDelta delta =
      base::Milliseconds(mouseup_timestamp - mousedown_timestamp);

  EXPECT_GE(delta, features::kGlicActorClickDelay.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorClickToolBrowserTest,
    testing::Combine(
        testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                        ::features::ActorPaintStabilityMode::kLogOnly,
                        ::features::ActorPaintStabilityMode::kEnabled),
        testing::ValuesIn(kActorGeneralPageStabilityModeValues)),
    ActorClickToolBrowserTest::DescribeParams);

}  // namespace
}  // namespace actor
