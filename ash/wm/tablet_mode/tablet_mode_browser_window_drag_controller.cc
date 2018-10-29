// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

TabletModeBrowserWindowDragController::TabletModeBrowserWindowDragController(
    wm::WindowState* window_state)
    : WindowResizer(window_state),
      drag_delegate_(std::make_unique<TabletModeBrowserWindowDragDelegate>()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  DCHECK(!window_state->allow_set_bounds_direct());

  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    Shell::Get()->cursor_manager()->LockCursor();
    did_lock_cursor_ = true;
  }

  previous_location_in_screen_ = details().initial_location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &previous_location_in_screen_);

  drag_delegate_->StartWindowDrag(GetTarget(), previous_location_in_screen_);
}

TabletModeBrowserWindowDragController::
    ~TabletModeBrowserWindowDragController() {
  if (did_lock_cursor_)
    Shell::Get()->cursor_manager()->UnlockCursor();
}

void TabletModeBrowserWindowDragController::Drag(
    const gfx::Point& location_in_parent,
    int event_flags) {
  gfx::Point location_in_screen = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);
  previous_location_in_screen_ = location_in_screen;

  // Update the dragged window, the drag indicators, the preview window,
  // source window position, blurred background, etc, if necessary.
  if (wm::IsDraggingTabs(GetTarget())) {
    // Update the dragged window's bounds if it's tab-dragging.
    base::WeakPtr<TabletModeBrowserWindowDragController> resizer(
        weak_ptr_factory_.GetWeakPtr());
    drag_delegate_->ContinueWindowDrag(
        location_in_screen,
        TabletModeWindowDragDelegate::UpdateDraggedWindowType::UPDATE_BOUNDS,
        CalculateBoundsForDrag(location_in_parent));
    // Note, this might have been deleted when reaching here.
    if (!resizer)
      return;
  } else {
    // Otherwise, update the dragged window's transform.
    drag_delegate_->ContinueWindowDrag(
        location_in_screen, TabletModeWindowDragDelegate::
                                UpdateDraggedWindowType::UPDATE_TRANSFORM);
  }
}

void TabletModeBrowserWindowDragController::CompleteDrag() {
  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::SUCCESS,
      previous_location_in_screen_);
}

void TabletModeBrowserWindowDragController::RevertDrag() {
  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::REVERT,
      previous_location_in_screen_);
}

void TabletModeBrowserWindowDragController::FlingOrSwipe(
    ui::GestureEvent* event) {
  drag_delegate_->FlingOrSwipe(event);
}

}  // namespace ash
