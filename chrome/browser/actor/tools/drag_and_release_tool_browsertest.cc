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
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;

namespace actor {

namespace {

gfx::RectF GetBoundingClientRect(content::RenderFrameHost& rfh,
                                 std::string_view query) {
  double width =
      content::EvalJs(
          &rfh, content::JsReplace(
                    "document.querySelector($1).getBoundingClientRect().width",
                    query))
          .ExtractDouble();
  double height =
      content::EvalJs(
          &rfh, content::JsReplace(
                    "document.querySelector($1).getBoundingClientRect().height",
                    query))
          .ExtractDouble();
  double x =
      content::EvalJs(
          &rfh,
          content::JsReplace(
              "document.querySelector($1).getBoundingClientRect().x", query))
          .ExtractDouble();
  double y =
      content::EvalJs(
          &rfh,
          content::JsReplace(
              "document.querySelector($1).getBoundingClientRect().y", query))
          .ExtractDouble();

  return gfx::RectF(x, y, width, height);
}

int GetRangeValue(content::RenderFrameHost& rfh, std::string_view query) {
  return content::EvalJs(
             &rfh, content::JsReplace(
                       "parseInt(document.querySelector($1).value)", query))
      .ExtractInt();
}

// Test the drag and release tool by moving the thumb on a range slider control.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Range) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#range"));
}

// Ensure the drag tool sends the expected mouse down, move and up events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Events) {
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

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(base::StrCat({"mousemove[", start.ToString(), "],", "mousedown[",
                          start.ToString(), "],", "mousemove[", end.ToString(),
                          "],", "mouseup[", end.ToString(), "]"}),
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Ensure coordinates outside of the viewport are rejected.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Offscreen) {
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
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kDragAndReleaseFromOffscreen);
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
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);
  }

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#offscreenRange"));
}

}  // namespace
}  // namespace actor
