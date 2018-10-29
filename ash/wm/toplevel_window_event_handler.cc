// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ash {

ToplevelWindowEventHandler::ToplevelWindowEventHandler()
    : weak_factory_(this) {}

ToplevelWindowEventHandler::~ToplevelWindowEventHandler() = default;

void ToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  wm_toplevel_window_event_handler_.OnKeyEvent(event);
}

void ToplevelWindowEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  wm_toplevel_window_event_handler_.OnMouseEvent(event, target);
}

void ToplevelWindowEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  wm_toplevel_window_event_handler_.OnGestureEvent(event, target);
}

bool ToplevelWindowEventHandler::AttemptToStartDrag(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    wm::WmToplevelWindowEventHandler::EndClosure end_closure) {
  aura::Window* gesture_target =
      wm_toplevel_window_event_handler_.gesture_target();
  ::wm::WindowMoveSource source = gesture_target
                                      ? ::wm::WINDOW_MOVE_SOURCE_TOUCH
                                      : ::wm::WINDOW_MOVE_SOURCE_MOUSE;
  if (gesture_target) {
    window->env()->gesture_recognizer()->TransferEventsTo(
        gesture_target, window, ui::TransferTouchesBehavior::kDontCancel);
  }
  return wm_toplevel_window_event_handler_.AttemptToStartDrag(
      window, point_in_parent, window_component, source,
      std::move(end_closure));
}

::wm::WindowMoveResult ToplevelWindowEventHandler::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    ::wm::WindowMoveSource move_source) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  aura::Window* root_window = source->GetRootWindow();
  DCHECK(root_window);
  gfx::Point drag_location;
  if (move_source == ::wm::WINDOW_MOVE_SOURCE_TOUCH &&
      Shell::Get()->aura_env()->is_touch_down()) {
    gfx::PointF drag_location_f;
    bool has_point =
        source->env()->gesture_recognizer()->GetLastTouchPointForTarget(
            source, &drag_location_f);
    drag_location = gfx::ToFlooredPoint(drag_location_f);
    DCHECK(has_point);
  } else {
    drag_location =
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(root_window, source->parent(),
                                       &drag_location);
  }
  // Set the cursor before calling AttemptToStartDrag(), as that will
  // eventually call LockCursor() and prevent the cursor from changing.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(ui::CursorType::kPointer);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  wm::WmToplevelWindowEventHandler::DragResult result =
      wm::WmToplevelWindowEventHandler::DragResult::SUCCESS;
  if (!wm_toplevel_window_event_handler_.AttemptToStartDrag(
          source, drag_location, HTCAPTION, move_source,
          base::Bind(&ToplevelWindowEventHandler::OnDragCompleted,
                     weak_factory_.GetWeakPtr(), &result, &run_loop))) {
    return ::wm::MOVE_CANCELED;
  }

  in_move_loop_ = true;
  base::WeakPtr<ToplevelWindowEventHandler> weak_ptr(
      weak_factory_.GetWeakPtr());

  // Disable window position auto management while dragging and restore it
  // aftrewards.
  wm::WindowState* window_state = wm::GetWindowState(source);
  const bool window_position_managed = window_state->GetWindowPositionManaged();
  window_state->SetWindowPositionManaged(false);
  aura::WindowTracker tracker({source});

  run_loop.Run();

  if (!weak_ptr)
    return ::wm::MOVE_CANCELED;

  // Make sure the window hasn't been deleted.
  if (tracker.Contains(source))
    window_state->SetWindowPositionManaged(window_position_managed);

  in_move_loop_ = false;
  return result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS
             ? ::wm::MOVE_SUCCESSFUL
             : ::wm::MOVE_CANCELED;
}

void ToplevelWindowEventHandler::EndMoveLoop() {
  if (in_move_loop_)
    wm_toplevel_window_event_handler_.RevertDrag();
}

void ToplevelWindowEventHandler::OnDragCompleted(
    wm::WmToplevelWindowEventHandler::DragResult* result_return_value,
    base::RunLoop* run_loop,
    wm::WmToplevelWindowEventHandler::DragResult result) {
  *result_return_value = result;
  run_loop->Quit();
}

}  // namespace ash
