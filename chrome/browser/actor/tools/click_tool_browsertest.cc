// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;
using content::RenderFrameHost;

namespace actor {

namespace {

constexpr char kTargetCenterHitElementIdScript[] = R"JS(
  const target_rect =
      document.getElementById('target').getBoundingClientRect();
  const x = target_rect.left + target_rect.width / 2;
  const y = target_rect.top + target_rect.height / 2;
  document.elementFromPoint(x, y).id;
)JS";

// This observer detects when WebContents receives notification of a user
// gesture having occurred, following a user input event targeted to
// a RenderWidgetHost under that WebContents.
class UserInteractionObserver : public content::WebContentsObserver {
 public:
  explicit UserInteractionObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  UserInteractionObserver(const UserInteractionObserver&) = delete;
  UserInteractionObserver& operator=(const UserInteractionObserver&) = delete;

  ~UserInteractionObserver() override {}

  // Retrieve the flag. There is no need to wait on a loop since
  // DidGetUserInteraction() should be called synchronously with the input
  // event processing in the browser process.
  bool WasUserInteractionReceived() { return user_interaction_received_; }

  void Reset() { user_interaction_received_ = false; }

 private:
  // WebContentsObserver
  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    user_interaction_received_ = true;
  }

  bool user_interaction_received_ = false;
};

class ActorClickToolBrowserTest : public ActorToolsTest {
 public:
  ActorClickToolBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{features::kGlicActorClickDelay.name, "200ms"},
         {features::kGlicActorPolicyControlExemption.name, "true"}});
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
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_SentToElement) {
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
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BODY#],mousedown[BODY#],mouseup[BODY#],click[BODY#]"));
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
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
            "mouseup[BUTTON#clickable],click[BUTTON#clickable]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to an element that doesn't exist fails.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
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

  // The page should not have received any click events.
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::Not(testing::HasSubstr("mousedown")));
}

// Sending a click to a disabled element should fail without dispatching events.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_DisabledElement) {
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

  // The page should not have received any click events.
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::Not(testing::HasSubstr("mousedown")));
}

// Sending a click to an element that's not in the viewport should cause it to
// first be scrolled into view then clicked.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_OffscreenElement) {
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
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::EndsWith(
          "mousemove[BUTTON#offscreen],mousedown[BUTTON#offscreen],"
          "mouseup[BUTTON#offscreen],click[BUTTON#offscreen]"));
}

// Sending a click to an element that's not in the viewport should cause it to
// first be scrolled into view then clicked.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
                       ClickTool_OffscreenHiddenElement) {
  const GURL url = embedded_test_server()->GetURL("/actor/oov_elements.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  for (const char* selector :
       {"#detailButton", "#hiddenButton", "#autoButton"}) {
    SCOPED_TRACE(selector);
    // Starts unscrolled
    ASSERT_TRUE(EvalJs(web_contents(), "window.scroll(0,0)").is_ok());

    std::optional<int> button_id = GetDOMNodeId(*main_frame(), selector);
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    // Page is now scrolled.
    EXPECT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  }
}

// Ensure clicks can be sent to elements that are only partially onscreen.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_ClippedElements) {
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
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_SentToCoordinate) {
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
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[HTML#],mousedown[HTML#],mouseup[HTML#],click[HTML#]"));
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
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
            "mouseup[BUTTON#clickable],click[BUTTON#clickable]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
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

    // The page should not have received any click events.
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::Not(testing::HasSubstr("mousedown")));
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
    // The page should not have received any click events.
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::Not(testing::HasSubstr("mousedown")));
  }
}

// Ensure click is using viewport coordinate.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
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
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#offscreen],mousedown[BUTTON#offscreen],"
            "mouseup[BUTTON#offscreen],click[BUTTON#offscreen]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "offscreen_button_clicked"));
  }
}

// Ensure click works correctly when clicking on a cross process iframe using a
// DomNodeId
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
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
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
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

IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, ClickTool_Delay) {
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

  const double mousedown_timestamp = EvalJs(main_frame(), R"(
        let index = mouse_event_log.findIndex(
          (entry) => entry.startsWith('mousedown'));
        mouse_event_timestamps[index]
      )")
                                         .ExtractDouble();
  const double mouseup_timestamp = EvalJs(main_frame(), R"(
        let index = mouse_event_log.findIndex(
          (entry) => entry.startsWith('mouseup'));
        mouse_event_timestamps[index]
      )")
                                       .ExtractDouble();
  const base::TimeDelta delta =
      base::Milliseconds(mouseup_timestamp - mousedown_timestamp);

  EXPECT_GE(delta, features::kGlicActorClickDelay.Get());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, UserInteractionTriggered) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  UserInteractionObserver observer(web_contents());

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ASSERT_FALSE(observer.WasUserInteractionReceived());

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  ASSERT_TRUE(observer.WasUserInteractionReceived());
}

// Test that we can dispatch a click to a checkbox that's entirely overlaid by
// a pseudo element in its associated label.
IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest, CheckboxOverlayedByPseudo) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::optional<int> checkbox_id =
      GetDOMNodeId(*main_frame(), "#checkboxPseudo");
  ASSERT_TRUE(checkbox_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), checkbox_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

IN_PROC_BROWSER_TEST_F(ActorClickToolBrowserTest,
                       ClickTool_OccludedByFixedContainer) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // 1. Try to click target while it is occluded.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), target_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());

    // The tool itself might succeed (because validation is disabled by
    // default).
    ExpectOkResult(result);

    // But coordinate input should have hit the fixed container, not the target.
    EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
                testing::HasSubstr("click[fixed-container]"));
  }

  // Reset log.
  ASSERT_TRUE(ExecJs(web_contents(), "click_log = []"));

  // 2. Scroll the page so that the target is no longer behind the fixed
  // container. Scroll down by 150px. Target visual y becomes 220 - 150 = 70px.
  // Fixed container is still at 200-300px.
  // Target is now fully visible and not occluded.
  ASSERT_TRUE(ExecJs(web_contents(), "window.scroll(0, 150)"));

  // Verify the test condition directly instead of depending on the exact
  // scroll offset reported by the platform. Some environments round the scroll
  // amount by a few pixels, but the important contract is that the target's
  // full visual rect is above the fixed panel's visual rect.
  ASSERT_EQ(true, EvalJs(web_contents(), R"JS(
    const target_rect =
        document.getElementById('target').getBoundingClientRect();
    const fixed_rect =
        document.getElementById('fixed-container').getBoundingClientRect();
    target_rect.bottom < fixed_rect.top;
  )JS"));

  // Try to click target again.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), target_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());

    ExpectOkResult(result);

    // Now coordinate input should have hit the target.
    EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
                testing::HasSubstr("click[target]"));
  }
}

class ActorClickToolValidationBrowserTest : public ActorClickToolBrowserTest {
 public:
  ActorClickToolValidationBrowserTest() {
    validation_feature_list_.InitWithFeatures(
        {features::kGlicActorToctouValidation,
         features::kGlicActorOccludedDirectActivation},
        {});
  }

  static mojom::ObservedToolTargetPtr MakeBroadObservedTarget(int node_id) {
    auto observed_target = mojom::ObservedToolTarget::New();
    observed_target->node_attribute =
        blink::mojom::AIPageContentAttributes::New();
    observed_target->node_attribute->dom_node_id = node_id;
    observed_target->node_attribute->geometry =
        blink::mojom::AIPageContentGeometry::New();
    // These direct activation tests are about renderer-side validation, not
    // observed APC bounds. Use a broad synthetic APC box so validation reaches
    // the specific condition under test.
    observed_target->node_attribute->geometry->outer_bounding_box =
        gfx::Rect(0, 0, 10000, 10000);
    observed_target->node_attribute->geometry->visible_bounding_box =
        gfx::Rect(0, 0, 10000, 10000);
    return observed_target;
  }

  void ExpectDirectActivationClicksTarget(RenderFrameHost* frame = nullptr) {
    if (!frame) {
      frame = main_frame();
    }

    std::optional<int> target_id = GetDOMNodeId(*frame, "#target");
    ASSERT_TRUE(target_id);

    // Document the live geometry before the APC observation. The direct path is
    // only valid if the target's interaction point is really covered by the
    // test panel, so the test should fail early if fixture geometry changes.
    ASSERT_THAT(EvalJs(frame, kTargetCenterHitElementIdScript).ExtractString(),
                testing::AnyOf(testing::Eq("fixed-container"),
                               testing::Eq("panel-child")));

    // Direct activation still uses the normal TOCTOU contract for node-id
    // targets. The model would have learned the target id from a prior APC
    // observation, so the test records that observation before asking actor to
    // bypass the panel.
    GetPageApc();

    // The APC snapshot must not move this test out of the intended state.
    // Actor validates against the live DOM just before dispatch, so direct
    // activation should only be accepted while the panel is still on top.
    ASSERT_THAT(EvalJs(frame, kTargetCenterHitElementIdScript).ExtractString(),
                testing::AnyOf(testing::Eq("fixed-container"),
                               testing::Eq("panel-child")));

    std::unique_ptr<ToolRequest> action =
        MakeDirectElementActivationClickRequest(*frame, target_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());

    // Direct activation is an explicit opt-in path for element-id targets that
    // are blocked by fixed or absolute panels. It should still pass validation
    // for the observed node, but it must not fail just because a coordinate hit
    // test would land on the overlay.
    ExpectOkResult(result);

    // Direct activation should dispatch the same basic pointer and mouse event
    // sequence to the target that Blink's accessibility activation path uses.
    EXPECT_THAT(
        EvalJs(frame, "click_log.join(',')").ExtractString(),
        testing::Eq("pointerdown[target],mousedown[target],pointerup[target],"
                    "mouseup[target],click[target]"));
  }

  void ExpectDirectActivationRejectedDuringRendererValidation(
      const std::string& path,
      mojom::ActionResultCode expected_code =
          mojom::ActionResultCode::kElementDisabled,
      bool expect_panel_hit = true) {
    const GURL url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
    ASSERT_TRUE(target_id);

    // Most cases keep an eligible panel as the live hit-test winner. Native
    // modal dialogs may instead make the outside page inert before hit testing
    // reaches the panel, which is also an author boundary we must respect.
    const std::string hit_element_id =
        EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString();
    if (expect_panel_hit) {
      ASSERT_THAT(hit_element_id, testing::AnyOf(testing::Eq("fixed-container"),
                                                 testing::Eq("panel-child")));
    } else {
      ASSERT_NE("target", hit_element_id);
    }

    auto observed_target = MakeBroadObservedTarget(target_id.value());

    auto click = mojom::ClickAction::New();
    click->type = mojom::ClickType::kLeftOnOccludedTarget;
    click->count = mojom::ClickCount::kSingle;

    auto invocation = mojom::ToolInvocation::New();
    invocation->task_id = actor_task().id();
    invocation->action = mojom::ToolAction::NewClick(std::move(click));
    invocation->target = mojom::ToolTarget::NewDomNodeId(target_id.value());
    invocation->observed_target = std::move(observed_target);

    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);

    TestFuture<mojom::InitializeToolResultPtr> initialize_future;
    chrome_render_frame->InitializeTool(std::move(invocation),
                                        initialize_future.GetCallback());
    mojom::InitializeToolResultPtr initialize_result = initialize_future.Take();

    ASSERT_TRUE(initialize_result->is_error_result());
    const mojom::ActionResultPtr& error_result =
        initialize_result->get_error_result();
    EXPECT_EQ(expected_code, error_result->code);
    EXPECT_FALSE(error_result->requires_page_stabilization);
    EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
                testing::IsEmpty());
  }

  void InitializeDirectActivationToolForTesting(
      int target_id,
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>*
          chrome_render_frame) {
    auto observed_target = MakeBroadObservedTarget(target_id);

    auto click = mojom::ClickAction::New();
    click->type = mojom::ClickType::kLeftOnOccludedTarget;
    click->count = mojom::ClickCount::kSingle;

    auto invocation = mojom::ToolInvocation::New();
    invocation->task_id = actor_task().id();
    invocation->action = mojom::ToolAction::NewClick(std::move(click));
    invocation->target = mojom::ToolTarget::NewDomNodeId(target_id);
    invocation->observed_target = std::move(observed_target);

    main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        chrome_render_frame);

    TestFuture<mojom::InitializeToolResultPtr> initialize_future;
    (*chrome_render_frame)
        ->InitializeTool(std::move(invocation),
                         initialize_future.GetCallback());

    // These tests mutate the page after initialization. A setup failure here
    // means the test never reached the live execution-time revalidation path.
    ASSERT_TRUE(initialize_future.Get()->is_success_point());
  }

 private:
  base::test::ScopedFeatureList validation_feature_list_;
};

class ActorClickToolOccludedDirectActivationDisabledBrowserTest
    : public ActorClickToolBrowserTest {
 public:
  ActorClickToolOccludedDirectActivationDisabledBrowserTest() {
    // Enable normal TOCTOU validation but leave the click-behind rollout gate
    // disabled. The option alone should not unlock the direct activation path.
    validation_feature_list_.InitWithFeatures(
        {features::kGlicActorToctouValidation},
        {features::kGlicActorOccludedDirectActivation});
  }

 private:
  base::test::ScopedFeatureList validation_feature_list_;
};

class ActorClickToolDirectActivationNoToctouBrowserTest
    : public ActorClickToolBrowserTest {
 public:
  ActorClickToolDirectActivationNoToctouBrowserTest() {
    // Keep the direct-activation feature enabled while normal TOCTOU validation
    // is disabled. Direct activation still requires APC observation because it
    // bypasses renderer hit testing.
    validation_feature_list_.InitWithFeatures(
        {features::kGlicActorOccludedDirectActivation},
        {features::kGlicActorToctouValidation});
  }

 private:
  base::test::ScopedFeatureList validation_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_OccludedByFixedContainer_FailsValidation) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // Try to click target while it is occluded.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // Validation should fail because the target is occluded by the fixed
  // container.
  ExpectErrorResult(
      result, mojom::ActionResultCode::kTargetNodeInteractionPointObscured);

  // The page should not have received any click events on the target.
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_OccludedByFixedContainer) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ExpectDirectActivationClicksTarget();
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_OccludedWithModelessDialogAncestor) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/click_occluded_by_fixed.html?modeless_dialog_parent=1");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // A non-modal dialog opened with show() is ordinary page structure. The
  // native modal barrier case is covered separately by showModal().
  ExpectDirectActivationClicksTarget();
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolOccludedDirectActivationDisabledBrowserTest,
    ClickTool_DirectActivation_RequiresFeatureEnabled) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // Observe the page before executing so the rejection is specifically about
  // the Chrome rollout gate, not missing APC target data.
  GetPageApc();

  std::unique_ptr<ToolRequest> action =
      MakeDirectElementActivationClickRequest(*main_frame(), target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolDirectActivationNoToctouBrowserTest,
    ClickTool_DirectActivation_RequiresApcObservationWithoutToctou) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // The live page is in the only geometry state that direct activation may
  // bypass: the target center is covered by an eligible fixed panel.
  ASSERT_THAT(
      EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString(),
      testing::AnyOf(testing::Eq("fixed-container"),
                     testing::Eq("panel-child")));

  // Do not call GetPageApc(). A content node id is meaningful only when it
  // came from a prior APC snapshot, so the direct path must fail closed when
  // PageTool cannot attach observed target data to the renderer invocation.
  std::unique_ptr<ToolRequest> action =
      MakeDirectElementActivationClickRequest(*main_frame(), target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(
      result, mojom::ActionResultCode::kFrameLocationChangedSinceObservation);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_RequiresApcGeometry) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // Keep the live page in the valid click-behind shape. This test should fail
  // only because the APC payload lacks geometry for the observed node.
  ASSERT_THAT(
      EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString(),
      testing::AnyOf(testing::Eq("fixed-container"),
                     testing::Eq("panel-child")));

  auto observed_target = mojom::ObservedToolTarget::New();
  observed_target->node_attribute =
      blink::mojom::AIPageContentAttributes::New();
  observed_target->node_attribute->dom_node_id = target_id.value();
  // Leave `geometry` unset. Direct activation uses APC geometry to prove the
  // request is tied to an observed page target before it bypasses hit testing.

  auto click = mojom::ClickAction::New();
  click->type = mojom::ClickType::kLeftOnOccludedTarget;
  click->count = mojom::ClickCount::kSingle;

  auto invocation = mojom::ToolInvocation::New();
  invocation->task_id = actor_task().id();
  invocation->action = mojom::ToolAction::NewClick(std::move(click));
  invocation->target = mojom::ToolTarget::NewDomNodeId(target_id.value());
  invocation->observed_target = std::move(observed_target);

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  TestFuture<mojom::InitializeToolResultPtr> initialize_future;
  chrome_render_frame->InitializeTool(std::move(invocation),
                                      initialize_future.GetCallback());
  mojom::InitializeToolResultPtr initialize_result = initialize_future.Take();

  ASSERT_TRUE(initialize_result->is_error_result());
  const mojom::ActionResultPtr& error_result =
      initialize_result->get_error_result();
  EXPECT_EQ(mojom::ActionResultCode::kArgumentsInvalid, error_result->code);
  EXPECT_FALSE(error_result->requires_page_stabilization);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsWhenLivePointLeavesObservedApcBounds) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // The live target is still in the valid click-behind shape. This test is
  // only about rejecting stale or mismatched APC geometry.
  ASSERT_THAT(
      EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString(),
      testing::AnyOf(testing::Eq("fixed-container"),
                     testing::Eq("panel-child")));

  auto observed_target = mojom::ObservedToolTarget::New();
  observed_target->node_attribute =
      blink::mojom::AIPageContentAttributes::New();
  observed_target->node_attribute->dom_node_id = target_id.value();
  observed_target->node_attribute->geometry =
      blink::mojom::AIPageContentGeometry::New();
  // Put the observed APC box far away from the live interaction point. Direct
  // activation must prove the request still matches the last APC snapshot.
  observed_target->node_attribute->geometry->outer_bounding_box =
      gfx::Rect(0, 0, 1, 1);
  observed_target->node_attribute->geometry->visible_bounding_box =
      gfx::Rect(0, 0, 1, 1);

  auto click = mojom::ClickAction::New();
  click->type = mojom::ClickType::kLeftOnOccludedTarget;
  click->count = mojom::ClickCount::kSingle;

  auto invocation = mojom::ToolInvocation::New();
  invocation->task_id = actor_task().id();
  invocation->action = mojom::ToolAction::NewClick(std::move(click));
  invocation->target = mojom::ToolTarget::NewDomNodeId(target_id.value());
  invocation->observed_target = std::move(observed_target);

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  TestFuture<mojom::InitializeToolResultPtr> initialize_future;
  chrome_render_frame->InitializeTool(std::move(invocation),
                                      initialize_future.GetCallback());
  mojom::InitializeToolResultPtr initialize_result = initialize_future.Take();

  ASSERT_TRUE(initialize_result->is_error_result());
  const mojom::ActionResultPtr& error_result =
      initialize_result->get_error_result();
  EXPECT_EQ(mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            error_result->code);
  EXPECT_FALSE(error_result->requires_page_stabilization);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_RejectsSameProcessSubframe) {
  // Click-behind direct activation is currently scoped to the main frame. The
  // same-process case is subtle because actor initializes the renderer tool on
  // the local root, so validation must inspect the resolved target document.
  const GURL url =
      embedded_https_test_server().GetURL("/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_FALSE(subframe->IsCrossProcessSubframe());

  std::optional<int> target_id =
      GetDOMNodeIdFromSubframe(*main_frame(), "#iframe", "#target");
  ASSERT_TRUE(target_id);

  // The main-frame-only guard runs before APC matching for subframe targets,
  // so this test does not need a page observation.

  std::unique_ptr<ToolRequest> action =
      MakeDirectElementActivationClickRequest(*subframe, target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  const auto& action_results = result.Get();
  ASSERT_EQ(1u, action_results.size());
  ASSERT_TRUE(action_results[0].result);
  EXPECT_EQ(mojom::ActionResultCode::kArgumentsInvalid,
            action_results[0].result->code)
      << ToDebugString(*action_results[0].result);
  EXPECT_THAT(EvalJs(subframe, "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_RejectsCrossProcessSubframe) {
  // Click-behind direct activation is currently scoped to the main frame. A
  // subframe target may become supported later, but that needs separate frame
  // ownership, routing, and cross-frame occlusion validation.
  if (!content::AreAllSitesIsolatedForTesting()) {
    GTEST_SKIP();
  }

  const GURL url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "bar.com", "/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());

  std::optional<int> target_id = GetDOMNodeId(*subframe, "#target");
  ASSERT_TRUE(target_id);

  // The main-frame-only guard runs before APC matching for subframe targets,
  // so this test does not need a page observation. Avoiding APC extraction also
  // keeps the test independent of cross-frame extraction timing.

  std::unique_ptr<ToolRequest> action =
      MakeDirectElementActivationClickRequest(*subframe, target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_THAT(EvalJs(subframe, "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_OccludedByAbsolutePanels) {
  const char* urls[] = {
      "/actor/click_occluded_by_fixed.html?panel=absolute",
      "/actor/click_occluded_by_fixed.html?panel=absolute&child=1"};

  for (const char* path : urls) {
    SCOPED_TRACE(path);
    const GURL url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    // The `child=1` case makes the live hit-test winner a child of the
    // occluder. Direct activation should still dispatch to the requested DOM
    // target once the server has chosen this action.
    ExpectDirectActivationClicksTarget();
  }
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_OccludedByShadowPanel) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/click_occluded_by_fixed.html?panel=shadow_fixed_slot&child=1");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  // The fixed panel is inside shadow DOM, while the hit-test winner is a
  // slotted light-DOM child. Direct activation should still dispatch to the
  // requested DOM target once the server has chosen this action.
  ASSERT_NE(
      "target",
      EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString());

  GetPageApc();

  std::unique_ptr<ToolRequest> action =
      MakeDirectElementActivationClickRequest(*main_frame(), target_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectOkResult(result);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::Eq("pointerdown[target],mousedown[target],"
                          "pointerup[target],mouseup[target],click[target]"));
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_FailsWhenModalDialogOpen) {
  // A modal dialog makes content outside the dialog intentionally inert. Direct
  // activation should not click behind that author-provided boundary.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?modal=1",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
      /*expect_panel_hit=*/false);
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_FailsWhenTargetIsInert) {
  // The direct path must respect explicit inert subtrees. Otherwise it could
  // activate content the author intentionally removed from user interaction.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?inert=target-parent",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsWhenTargetOptsOutOfHitTesting) {
  // The bypass is allowed to ignore an eligible panel above the target. It must
  // not ignore pointer-events:none on the target itself.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?target_pointer_events=none",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
}

IN_PROC_BROWSER_TEST_F(ActorClickToolValidationBrowserTest,
                       ClickTool_DirectActivation_OccludedByPopover) {
  const char* urls[] = {
      "/actor/click_occluded_by_fixed.html?panel=popover&child=1",
      "/actor/click_occluded_by_fixed.html?panel=popover&child=1&"
      "child_position=fixed"};

  for (const char* path : urls) {
    SCOPED_TRACE(path);
    const GURL url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    // Popover is not a special actor concept. If it is the fixed-position
    // visual occluder at the target point, it follows the same eligibility
    // rule as a cookie banner or fixed footer. The model still owns whether
    // clicking behind the popover is the right task strategy.
    ExpectDirectActivationClicksTarget();
  }
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsWhenBackgroundIsScrollLocked) {
  // Scroll lock commonly means a foreground overlay is controlling
  // interaction. A target in the locked background should not be clicked
  // through the covering panel.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?scroll_locked=1",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsWhenTargetIsNoLongerOccludedAtExecution) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  InitializeDirectActivationToolForTesting(target_id.value(),
                                           &chrome_render_frame);

  // Mutate after initialization but before execution. Once the target is not
  // covered, direct activation should decline because a normal click can now
  // use the coordinate path.
  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    window.scroll(0, 150);
    click_log = [];
  )JS"));
  ASSERT_EQ(
      "target",
      EvalJs(web_contents(), kTargetCenterHitElementIdScript).ExtractString());

  TestFuture<mojom::ActionResultPtr> execute_future;
  chrome_render_frame->ExecuteTool(actor_task().id(),
                                   execute_future.GetCallback());

  const mojom::ActionResultPtr& execute_result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            execute_result->code);
  EXPECT_FALSE(execute_result->requires_page_stabilization);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsForRelativePositionedOccluder) {
  // Relative positioning can be part of ordinary page layout. The direct path
  // is only for out-of-flow panels that deliberately cover the page.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?panel=relative&child=1",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_OccludedByAbsolutePanelInAbsoluteAncestor) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/click_occluded_by_fixed.html?panel=shared_absolute_ancestor");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The server chose direct activation from the APC tree. The client only
  // revalidates that the live occluder is still a fixed/absolute panel.
  ExpectDirectActivationClicksTarget();
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_OccludedByAbsolutePanelInRelativeAncestor) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/click_occluded_by_fixed.html?panel=shared_relative_ancestor");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // A relative ancestor only establishes local layout. The eligible occluder is
  // still the absolute panel at the target point.
  ExpectDirectActivationClicksTarget();
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_FailsForEmbeddedContentOccluder) {
  // Embedded content occluders are rejected server-side today. If one reaches
  // the client anyway, fail closed rather than normalizing child-frame hits.
  ExpectDirectActivationRejectedDuringRendererValidation(
      "/actor/click_occluded_by_fixed.html?panel=iframe",
      mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
}

IN_PROC_BROWSER_TEST_F(
    ActorClickToolValidationBrowserTest,
    ClickTool_DirectActivation_RevalidatesAvailabilityBeforeDispatch) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_occluded_by_fixed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> target_id = GetDOMNodeId(*main_frame(), "#target");
  ASSERT_TRUE(target_id);

  auto observed_target = MakeBroadObservedTarget(target_id.value());

  auto click = mojom::ClickAction::New();
  click->type = mojom::ClickType::kLeftOnOccludedTarget;
  click->count = mojom::ClickCount::kSingle;

  auto invocation = mojom::ToolInvocation::New();
  invocation->task_id = actor_task().id();
  invocation->action = mojom::ToolAction::NewClick(std::move(click));
  invocation->target = mojom::ToolTarget::NewDomNodeId(target_id.value());
  invocation->observed_target = std::move(observed_target);

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  TestFuture<mojom::InitializeToolResultPtr> initialize_future;
  chrome_render_frame->InitializeTool(std::move(invocation),
                                      initialize_future.GetCallback());
  ASSERT_TRUE(initialize_future.Get()->is_success_point());

  // Mutate after validation but before execution. Direct activation must
  // re-check live target availability immediately before dispatching trusted
  // events to the target.
  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    document.getElementById('target').parentElement.inert = true;
    click_log = [];
  )JS"));

  TestFuture<mojom::ActionResultPtr> execute_future;
  chrome_render_frame->ExecuteTool(actor_task().id(),
                                   execute_future.GetCallback());

  const mojom::ActionResultPtr& execute_result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            execute_result->code);
  EXPECT_FALSE(execute_result->requires_page_stabilization);
  EXPECT_THAT(EvalJs(web_contents(), "click_log.join(',')").ExtractString(),
              testing::IsEmpty());
}

class ActorClickToolScaledBrowserTest : public ActorToolsTest {
 public:
  ActorClickToolScaledBrowserTest() = default;
  ~ActorClickToolScaledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

// Ensure clicks can be sent to elements that are only partially onscreen with
// scaling.
IN_PROC_BROWSER_TEST_F(ActorClickToolScaledBrowserTest,
                       ClickTool_ScaledClippedElements) {
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

}  // namespace
}  // namespace actor
