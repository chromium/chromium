// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorScrollToolUiTest : public GlicActorUiTest {
 public:
  // Scrolls by the given offsets with optional `label`. If `label` is not
  // provided, the viewport is scrolled.
  MultiStep ScrollAction(std::optional<std::string_view> label,
                         float offset_x,
                         float offset_y,
                         actor::TaskId& task_id,
                         tabs::TabHandle& tab_handle,
                         ExpectedErrorResult expected_result = {}) {
    auto scroll_provider = base::BindLambdaForTesting(
        [this, &task_id, &tab_handle, label, offset_x, offset_y]() {
          std::optional<int32_t> node_id;
          if (label) {
            node_id = SearchAnnotatedPageContent(*label);
          }
          content::RenderFrameHost* frame =
              tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
          Actions action =
              actor::MakeScroll(*frame, node_id, offset_x, offset_y);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(scroll_provider),
                         std::move(expected_result));
  }

  MultiStep ScrollAction(std::optional<std::string_view> label,
                         float offset_x,
                         float offset_y,
                         ExpectedErrorResult expected_result = {}) {
    return ScrollAction(label, offset_x, offset_y, task_id_, tab_handle_,
                        std::move(expected_result));
  }

  // Scrolls by the given offsets at the given `click_point`.
  MultiStep ScrollActionAtPoint(const gfx::Point& click_point,
                                float offset_x,
                                float offset_y,
                                actor::TaskId& task_id,
                                tabs::TabHandle& tab_handle,
                                ExpectedErrorResult expected_result = {}) {
    auto scroll_provider = base::BindLambdaForTesting(
        [&task_id, &tab_handle, click_point, offset_x, offset_y]() {
          content::RenderFrameHost* frame =
              tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
          Actions action =
              actor::MakeScroll(*frame, click_point, offset_x, offset_y);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(scroll_provider),
                         std::move(expected_result));
  }

  MultiStep ScrollActionAtPoint(const gfx::Point& click_point,
                                float offset_x,
                                float offset_y,
                                ExpectedErrorResult expected_result = {}) {
    return ScrollActionAtPoint(click_point, offset_x, offset_y, task_id_,
                               tab_handle_, std::move(expected_result));
  }
};

// Test scrolling the viewport vertically.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollPageVertical) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(/*label=*/std::nullopt, /*offset_x=*/0, kScrollOffsetY),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", kScrollOffsetY));
}

// Test scrolling the viewport horizontally.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollPageHorizontal) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetX = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(/*label=*/std::nullopt, kScrollOffsetX, /*offset_y=*/0),
      CheckJsResult(kNewActorTabId, "() => window.scrollX", kScrollOffsetX));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, FailOnInvalidNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ExecuteAction(
          base::BindLambdaForTesting([this]() {
            content::RenderFrameHost* frame =
                tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
            Actions action =
                actor::MakeScroll(*frame, kNonExistentContentNodeId,
                                  /*scroll_offset_x=*/0, kScrollOffsetY);
            action.set_task_id(task_id_.value());
            return EncodeActionProto(action);
          }),
          actor::mojom::ActionResultCode::kInvalidDomNodeId),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

// Test scrolling in a sub-scroller on the page.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollElementWithNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "scroller";
  const int kScrollOffsetY = 50;
  const int kScrollOffsetX = 20;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('scroller').scrollTop",
                    kScrollOffsetY),
      ScrollAction(kElementLabel, kScrollOffsetX, /*offset_y=*/0),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('scroller').scrollLeft",
                    kScrollOffsetX));
}

// Test scrolling over a non-scrollable element returns failure.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollNonScrollable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "nonscroll";
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(
          kElementLabel, /*offset_x=*/0, kScrollOffsetY,
          actor::mojom::ActionResultCode::kScrollTargetNotUserScrollable),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('nonscroll').scrollTop",
                    /*value=*/0),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", /*value=*/0));
}

// Test scrolling a scroller that's currently offscreen. It will first be
// scrolled into view then scroll applied.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, OffscreenScrollable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "offscreenscroller";
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      CheckJsResult(kNewActorTabId, "()=>{ return window.scrollY == 0 }"),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      CheckJsResult(
          kNewActorTabId,
          "() => document.getElementById('offscreenscroller').scrollTop",
          kScrollOffsetY),
      CheckJsResult(kNewActorTabId, "()=>{ return window.scrollY > 0 }"));
}

// Test that a scrolling over a scroller with overflow in one axis only works
// correctly.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, OneAxisScroller) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "horizontalscroller";
  const int kScrollOffset = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(
          kElementLabel, /*offset_x=*/0, kScrollOffset,
          actor::mojom::ActionResultCode::kScrollTargetNotUserScrollable),
      CheckJsResult(
          kNewActorTabId,
          "() => document.getElementById('horizontalscroller').scrollTop",
          /*value=*/0),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", /*value=*/0),
      ScrollAction(kElementLabel, kScrollOffset, /*offset_y=*/0),
      CheckJsResult(
          kNewActorTabId,
          "() => document.getElementById('horizontalscroller').scrollLeft",
          kScrollOffset));
}

// Ensure scroll distances are correctly scaled when browser zoom is applied.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, BrowserZoomWithNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "scroller";

  double level = blink::ZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  // 60 physical pixels translates to 40 CSS pixels when the zoom factor is 1.5
  // (3 physical pixels : 2 CSS Pixels)
  const int kScrollOffsetPhysical = 60;
  const int kExpectedOffsetCss = 40;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetPhysical),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('scroller').scrollTop",
                    kExpectedOffsetCss));
}

// Ensure scroll distances are correctly scaled when applied to a CSS zoomed
// scroller.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, CssZoomWithNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "zoomedscroller";

  // 60 physical pixels translates to 120 CSS pixels since the scroller is
  // inside a `zoom:0.5` subtree (1 physical pixels : 2 CSS Pixels)
  const int kScrollOffsetPhysical = 60;
  const int kExpectedOffsetCss = 120;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetPhysical),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('zoomedscroller').scrollTop",
                    kExpectedOffsetCss));
}

// Test that a scroll on a page with scroll-behavior:smooth returns success if
// an animation was started, even though it may not have instantly scrolled.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, SmoothScrollSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "smoothscroller";
  const int kScrollOffsetY = 100;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('smoothscroller').scrollTop",
                    kScrollOffsetY));
}

// Test that a scroll on a page with scroll-behavior:smooth returns failure if
// trying to scroll in a direction with no scrollable extent.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, SmoothScrollAtExtent) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "smoothscroller";
  const int kScrollOffsetY = 100;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ExecuteJs(kNewActorTabId,
                "() => { "
                "document.querySelector('#smoothscroller').scrollTo({top:"
                "10000, behavior:'instant'})"
                "}"),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY,
                   actor::mojom::ActionResultCode::kScrollOffsetDidNotChange));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ZeroIdTargetsViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  // DOMNodeIDs start at 1 so 0 should be interpreted as viewport.
  const int kTargetViewport = actor::kRootElementDomNodeId;
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ExecuteAction(base::BindLambdaForTesting([this, kTargetViewport]() {
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        Actions action =
            actor::MakeScroll(*frame, kTargetViewport,
                              /*scroll_offset_x=*/0, kScrollOffsetY);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      })),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", kScrollOffsetY));
}

// Test targeting a subscroller for scrolling using a coordinate.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollElementWithCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kScrollerId = "scroller";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  gfx::Rect scroller_bound;
  const int kScrollOffsetY = 50;
  const int kScrollOffsetX = 20;

  auto scroller_x_provider =
      base::BindLambdaForTesting([&scroller_bound, this]() {
        gfx::Point coordinate = scroller_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, kScrollOffsetX, /*scroll_offset_y=*/0);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  auto scroller_y_provider =
      base::BindLambdaForTesting([&scroller_bound, this]() {
        gfx::Point coordinate = scroller_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, /*scroll_offset_x=*/0, kScrollOffsetY);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kScrollerId, scroller_bound),
      ExecuteAction(std::move(scroller_y_provider)),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('scroller').scrollTop",
                    kScrollOffsetY),
      ExecuteAction(std::move(scroller_x_provider)),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('scroller').scrollLeft",
                    kScrollOffsetX));
}

// Test scrolling in a non-scrollable element on the page with coordinate.
// This will result scrolling the root page scroller.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest,
                       ScrollNonScrollableElementWithCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kNonScrollerId = "nonscroll";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  gfx::Rect non_scroller_bound;
  const int kScrollOffsetY = 50;

  auto non_scroller_provider =
      base::BindLambdaForTesting([&non_scroller_bound, this]() {
        gfx::Point coordinate = non_scroller_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, /*scroll_offset_x=*/0, kScrollOffsetY);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kNonScrollerId, non_scroller_bound),
      ExecuteAction(std::move(non_scroller_provider)),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", kScrollOffsetY));
}

// Test scrolling on the page with invalid coordinate.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest,
                       ScrollInvalidCoordinateFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const gfx::Point kPoint(-1, -1);
  const int kScrollOffsetY = 50;

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ScrollActionAtPoint(
                      kPoint, /*offset_x=*/0, kScrollOffsetY,
                      actor::mojom::ActionResultCode::kCoordinatesOutOfBounds));
}

// Test scrolling a scroller that's currently offscreen with coordinate.
// This is expected to fail because offscreen actuation supports only DOMNodeId
// targeting.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest,
                       OffscreenScrollableWithCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kOffScreenScrollerId = "offscreenscroller";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  gfx::Rect off_screen_scrolle_bound;
  const int kScrollOffsetY = 50;

  auto off_screen_scroller_provider =
      base::BindLambdaForTesting([&off_screen_scrolle_bound, this]() {
        gfx::Point coordinate = off_screen_scrolle_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, /*scroll_offset_x=*/0, kScrollOffsetY);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kOffScreenScrollerId,
                    off_screen_scrolle_bound),
      ExecuteAction(std::move(off_screen_scroller_provider),
                    actor::mojom::ActionResultCode::kCoordinatesOutOfBounds));
}

// Test scrolling in a non-scrollable element on the page with coordinate.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest,
                       ScrollNonScrollableElementAndPageWithCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kNonScrollerId = "nonscroll";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/non_scrollable_page.html");
  gfx::Rect non_scroller_bound;
  const int kScrollOffsetY = 50;

  auto non_scroller_provider =
      base::BindLambdaForTesting([&non_scroller_bound, this]() {
        gfx::Point coordinate = non_scroller_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, /*scroll_offset_x=*/0, kScrollOffsetY);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kNonScrollerId, non_scroller_bound),
      ExecuteAction(
          std::move(non_scroller_provider),
          actor::mojom::ActionResultCode::kScrollTargetNotUserScrollable),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('nonscroll').scrollTop",
                    /*value=*/0),
      CheckJsResult(kNewActorTabId, "() => window.scrollY", /*value=*/0));
}

// Test for scrolling using a coordinate at a subscroller which has a button
// overlapping its center area. Because scroll tool with coordinate does not use
// toctou check, this test case will still succeed and will not throw a
// kObservedTargetElementDestroyed error.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest,
                       ScrollBubblesFromNonScrollingElement) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kButtonId = "button";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  gfx::Rect button_bound;
  const int kScrollOffsetY = 50;
  const int kScrollOffsetX = 20;

  auto scroller_x_provider =
      base::BindLambdaForTesting([&button_bound, this]() {
        gfx::Point coordinate = button_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, kScrollOffsetX, /*scroll_offset_y=*/0);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  auto scroller_y_provider =
      base::BindLambdaForTesting([&button_bound, this]() {
        gfx::Point coordinate = button_bound.CenterPoint();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeScroll(
            *frame, coordinate, /*scroll_offset_x=*/0, kScrollOffsetY);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kButtonId, button_bound),
      ExecuteAction(std::move(scroller_y_provider)),
      CheckJsResult(kNewActorTabId,
                    "() => document.getElementById('buttonscroller').scrollTop",
                    kScrollOffsetY),
      ExecuteAction(std::move(scroller_x_provider)),
      CheckJsResult(
          kNewActorTabId,
          "() => document.getElementById('buttonscroller').scrollLeft",
          kScrollOffsetX));
}

}  // namespace

}  // namespace glic::test
