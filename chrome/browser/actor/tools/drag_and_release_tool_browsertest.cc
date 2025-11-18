// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;

namespace actor {

namespace {

int GetRangeValue(content::RenderFrameHost& rfh, std::string_view query) {
  return content::EvalJs(
             &rfh, content::JsReplace(
                       "parseInt(document.querySelector($1).value)", query))
      .ExtractInt();
}

class ActorDragAndReleaseToolBrowserTest : public ActorToolsTest {
 public:
  ActorDragAndReleaseToolBrowserTest() = default;
  ~ActorDragAndReleaseToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// Test the drag and release tool by moving the thumb on a range slider control.
IN_PROC_BROWSER_TEST_F(ActorDragAndReleaseToolBrowserTest,
                       DragAndReleaseTool_Range) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  gfx::RectF range_rect = GetBoundingClientRect(*main_frame(), "#range");

  ASSERT_EQ(0, GetRangeValue(*main_frame(), "#range"));

  // Padding to roughly hit the center of the range drag thumb.
  const int thumb_padding = range_rect.height() / 2;

  gfx::Point start(range_rect.x() + thumb_padding,
                   range_rect.y() + thumb_padding);
  gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

  std::unique_ptr<ToolRequest> action =
      MakeDragAndReleaseRequest(*active_tab(), start, end);

  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#range"));
}

// Ensure the drag tool sends the expected mouse down, move and up events.
IN_PROC_BROWSER_TEST_F(ActorDragAndReleaseToolBrowserTest,
                       DragAndReleaseTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The dragLogger starts in the bottom right of the viewport. Scroll it to the
  // top left to ensure client coordinates are being used (i.e. drag coordinates
  // should not be affected by scroll and should match the mousemove client
  // coordinates reported by the page).
  ASSERT_TRUE(ExecJs(web_contents(), "window.scrollTo(450, 250)"));

  // Log starts off empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  gfx::RectF target_rect = GetBoundingClientRect(*main_frame(), "#dragLogger");

  // Arbitrary pad to hit a few pixels inside the logger element.
  const int kPadding = 10;
  gfx::Vector2d delta(100, 150);
  gfx::Point start(target_rect.x() + kPadding, target_rect.y() + kPadding);
  gfx::Point end = start + delta;

  std::unique_ptr<ToolRequest> action =
      MakeDragAndReleaseRequest(*active_tab(), start, end);

  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_THAT(
      EvalJs(web_contents(), "event_log.join(',')").ExtractString(),
      testing::AllOf(
          testing::StartsWith(
              base::StrCat({"mousemove[", start.ToString(), "],", "mousedown[",
                            start.ToString(), "],"})),
          testing::EndsWith(base::StrCat({"mousemove[", end.ToString(), "],",
                                          "mouseup[", end.ToString(), "]"}))));
}

// Ensure the drag tool sends the expected pointer down, move and up events and
// responds appropriately to setPointerCapture
IN_PROC_BROWSER_TEST_F(ActorDragAndReleaseToolBrowserTest,
                       DragAndReleaseTool_PointerEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts off empty.
  ASSERT_EQ("", EvalJs(web_contents(), "pointer_log.join(',')"));

  gfx::RectF target_rect =
      GetBoundingClientRect(*main_frame(), "#pointerLogger");

  // Arbitrary pad to hit a few pixels inside the logger element.
  const int kPadding = 10;
  gfx::Vector2d delta(100, 150);
  gfx::Point start(target_rect.x() + kPadding, target_rect.y() + kPadding);
  gfx::Point end = start + delta;

  std::unique_ptr<ToolRequest> action =
      MakeDragAndReleaseRequest(*active_tab(), start, end);

  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_THAT(EvalJs(web_contents(), "pointer_log.join(',')").ExtractString(),
              testing::AllOf(testing::StartsWith(base::StrCat({
                                 "pointermove[",
                                 start.ToString(),
                                 "]: 0,",
                                 "pointerdown[",
                                 start.ToString(),
                                 "]: 1,",
                                 "gotpointercapture[",
                             })),
                             testing::EndsWith(base::StrCat(
                                 {"pointermove[", end.ToString(), "]: 1,",
                                  "pointerup[", end.ToString(), "]: 0"}))));
}

// Ensure coordinates outside of the viewport are rejected.
IN_PROC_BROWSER_TEST_F(ActorDragAndReleaseToolBrowserTest,
                       DragAndReleaseTool_Offscreen) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts off empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Try to drag the range - it should fail since the range is offscreen (and so
  // the range_rect has bounds outside the viewport).
  {
    gfx::RectF range_rect =
        GetBoundingClientRect(*main_frame(), "#offscreenRange");

    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;
    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);
    gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

    std::unique_ptr<ToolRequest> action =
        MakeDragAndReleaseRequest(*active_tab(), start, end);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
  }

  // Scroll the range into the viewport.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "document.getElementById('offscreenRange').scrollIntoView()"));

  // Try to drag the range - now that it's been scrolled into the viewport this
  // should succeed.
  {
    // Recompute the client rect since it depends on scroll offset.
    gfx::RectF range_rect =
        GetBoundingClientRect(*main_frame(), "#offscreenRange");
    const int thumb_padding = range_rect.height() / 2;
    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);
    gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

    std::unique_ptr<ToolRequest> action =
        MakeDragAndReleaseRequest(*active_tab(), start, end);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
  }

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#offscreenRange"));
}

// TODO(crbug.com/460801630): Enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragAndReleaseTool_CrossOriginSubframe \
  DISABLED_DragAndReleaseTool_CrossOriginSubframe
#else
#define MAYBE_DragAndReleaseTool_CrossOriginSubframe \
  DragAndReleaseTool_CrossOriginSubframe
#endif
IN_PROC_BROWSER_TEST_F(ActorDragAndReleaseToolBrowserTest,
                       MAYBE_DragAndReleaseTool_CrossOriginSubframe) {
  const GURL url = embedded_https_test_server().GetURL(
      "/actor/positioned_iframe_no_scroll.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL cross_origin_iframe_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/drag.html");
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), "iframe", cross_origin_iframe_url));

  content::RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  // Addressing flaky test due to layout shift on the iframe
  ASSERT_TRUE(content::ExecJs(web_contents(), "wait()"));
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());

  ASSERT_EQ(0, GetRangeValue(*subframe, "#range"));

  gfx::RectF range_rect = GetBoundingClientRect(*subframe, "#range");
  const int thumb_padding = range_rect.height() / 2;
  gfx::Point start_in_subframe(range_rect.x() + thumb_padding,
                               range_rect.y() + thumb_padding);
  gfx::Point end_in_subframe = gfx::ToFlooredPoint(range_rect.CenterPoint());

  gfx::RectF subframe_rect = GetBoundingClientRect(*main_frame(), "#iframe");
  gfx::Point start_in_viewport(subframe_rect.x() + start_in_subframe.x(),
                               subframe_rect.y() + start_in_subframe.y());
  gfx::Point end_in_viewport(subframe_rect.x() + end_in_subframe.x(),
                             subframe_rect.y() + end_in_subframe.y());

  std::unique_ptr<ToolRequest> action = MakeDragAndReleaseRequest(
      *active_tab(), start_in_viewport, end_in_viewport);

  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/447000769): Allow 1 pixel of slop - probably due to
  // different display densities and the ToFlooredPoint above.
  EXPECT_NEAR(50, GetRangeValue(*subframe, "#range"), 1);
#else
  EXPECT_EQ(50, GetRangeValue(*subframe, "#range"));
#endif
}

}  // namespace
}  // namespace actor
