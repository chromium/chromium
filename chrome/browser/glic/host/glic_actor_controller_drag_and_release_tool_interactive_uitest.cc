// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_controller_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DragAndReleaseTool_Range) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  gfx::Rect range_rect;
  auto drag_provider = base::BindLambdaForTesting([this, &range_rect]() {
    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;

    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);

    gfx::Point end = range_rect.CenterPoint();

    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      GetClientRect(kNewActorTabId, "range", range_rect),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#range').value", "0"),
      ExecuteAction(std::move(drag_provider)),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#range').value", "50"));
}

// Ensure the drag tool sends the expected mouse down, move and up events.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DragAndReleaseTool_Events) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  gfx::Rect target_rect;

  auto drag_provider = base::BindLambdaForTesting([this, &target_rect]() {
    // Arbitrary pad to hit a few pixels inside the logger element.
    const int kPadding = 10;
    gfx::Vector2d delta(100, 150);
    gfx::Point start(target_rect.x() + kPadding, target_rect.y() + kPadding);
    gfx::Point end = start + delta;

    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ExecuteJs(kNewActorTabId, "() => { window.scrollTo(450, 250); }"),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')", ""),
      GetClientRect(kNewActorTabId, "dragLogger", target_rect),
      ExecuteAction(std::move(drag_provider)),
      WaitForJsResult(kNewActorTabId, "() => event_log.join(',')",
                      "mousemove[60,60],mousedown[60,60],mousemove[160,210],"
                      "mouseup[160,210]"));
}

// Ensure coordinates outside of the viewport are rejected.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       DragAndReleaseTool_Offscreen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  gfx::Rect range_rect;
  auto drag_provider = base::BindLambdaForTesting([this, &range_rect]() {
    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;

    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);

    gfx::Point end = range_rect.CenterPoint();

    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')", ""),
      GetClientRect(kNewActorTabId, "offscreenRange", range_rect),
      ExecuteAction(drag_provider,
                    actor::mojom::ActionResultCode::kCoordinatesOutOfBounds),

      // Scroll the range into the viewport.
      ExecuteJs(kNewActorTabId,
                "() => { "
                "document.getElementById('offscreenRange').scrollIntoView(); "
                "}"),

      // Try to drag the range again - it should succeed now.
      GetClientRect(kNewActorTabId, "offscreenRange", range_rect),
      ExecuteAction(drag_provider),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#offscreenRange').value",
                    "50"));
}

}  // namespace

}  // namespace glic::test
