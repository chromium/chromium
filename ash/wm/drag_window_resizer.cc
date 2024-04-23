// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include <utility>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/shell.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// static
DragWindowResizer* DragWindowResizer::instance_ = nullptr;

DragWindowResizer::~DragWindowResizer() {
  Shell* shell = Shell::Get();
  if (window_state_) {
    window_state_->DeleteDragDetails();
    shell->shadow_controller()->UpdateShadowForWindow(window_state_->window());
  }
  shell->mouse_cursor_filter()->set_mouse_warp_enabled(true);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();
  if (instance_ == this)
    instance_ = nullptr;
}

void DragWindowResizer::Drag(const gfx::PointF& location, int event_flags) {
  base::WeakPtr<DragWindowResizer> resizer(weak_ptr_factory_.GetWeakPtr());
  next_window_resizer_->Drag(location, event_flags);

  if (!resizer)
    return;

  last_mouse_location_ = location;
  // Show a phantom window for dragging in another root window.
  if (display::Screen::GetScreen()->GetNumDisplays() > 1)
    UpdateDragWindow();
  else
    drag_window_controller_.reset();
}

void DragWindowResizer::CompleteDrag() {
  EndDragImpl();
  next_window_resizer_->CompleteDrag();
}

void DragWindowResizer::RevertDrag() {
  drag_window_controller_.reset();
  next_window_resizer_->RevertDrag();
}

void DragWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  EndDragImpl();
  next_window_resizer_->FlingOrSwipe(event);
}

DragWindowResizer::DragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(std::move(next_window_resizer)) {
  // The pointer should be confined in one display during resizing a window
  // because the window cannot span two displays at the same time anyway. The
  // exception is window/tab dragging operation. During that operation, mouse
  // warp is set so that the user can move a window/tab to another display.
  MouseCursorEventFilter* mouse_cursor_filter =
      Shell::Get()->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_enabled(ShouldAllowMouseWarp());
  if (ShouldAllowMouseWarp())
    mouse_cursor_filter->ShowSharedEdgeIndicator(GetTarget()->GetRootWindow());
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(GetTarget());
  instance_ = this;
}

void DragWindowResizer::UpdateDragWindow() {
  if (details().window_component != HTCAPTION || !ShouldAllowMouseWarp())
    return;

  if (!drag_window_controller_) {
    drag_window_controller_ = std::make_unique<DragWindowController>(
        GetTarget(), details().source == wm::WINDOW_MOVE_SOURCE_TOUCH);
  }
  drag_window_controller_->Update();
}

bool DragWindowResizer::ShouldAllowMouseWarp() {
  return details().window_component == HTCAPTION &&
         !::wm::GetTransientParent(GetTarget()) &&
         window_util::IsWindowUserPositionable(GetTarget());
}

void DragWindowResizer::EndDragImpl() {
  drag_window_controller_.reset();

  // Check if the destination is another display.
  if (details().source == wm::WINDOW_MOVE_SOURCE_TOUCH)
    return;
  aura::Window* root_window = GetTarget()->GetRootWindow();
  // The |Display| object returned by |CursorManager::GetDisplay| may be stale,
  // but will have the correct id.
  // TODO(oshima): Change the API so |GetDisplay| just returns a display id.
  const int64_t dst_display_id =
      Shell::Get()->cursor_manager()->GetDisplay().id();
  display::Screen* screen = display::Screen::GetScreen();
  if (dst_display_id == screen->GetDisplayNearestWindow(root_window).id())
    return;

  // Adjust the size and position so that it doesn't exceed the size of work
  // area.
  display::Display dst_display;
  // TODO(crbug.com/40721205): It's possible that |dst_display_id| returned from
  // CursorManager::GetDisplay().id() is an invalid display id thus
  // |dst_display| may be invalid as well. This may cause crash later. To avoid
  // crash, we early return here. However, |dst_display_id| should never be
  // invalid.
  if (!screen->GetDisplayWithDisplayId(dst_display_id, &dst_display))
    return;
  const gfx::Size& size = dst_display.work_area().size();
  gfx::Rect bounds = GetTarget()->bounds();
  if (bounds.width() > size.width()) {
    int diff = bounds.width() - size.width();
    bounds.set_x(bounds.x() + diff / 2);
    bounds.set_width(size.width());
  }
  if (bounds.height() > size.height())
    bounds.set_height(size.height());

  gfx::Rect dst_bounds = bounds;
  ::wm::ConvertRectToScreen(GetTarget()->parent(), &dst_bounds);

  // Adjust the position so that the cursor is on the window.
  gfx::Point last_mouse_location_in_screen =
      gfx::ToRoundedPoint(last_mouse_location_);
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_mouse_location_in_screen);
  if (!dst_bounds.Contains(last_mouse_location_in_screen)) {
    if (last_mouse_location_in_screen.x() < dst_bounds.x())
      dst_bounds.set_x(last_mouse_location_in_screen.x());
    else if (last_mouse_location_in_screen.x() > dst_bounds.right())
      dst_bounds.set_x(last_mouse_location_in_screen.x() - dst_bounds.width());
  }
  AdjustBoundsToEnsureMinimumWindowVisibility(
      dst_display.bounds(), /*client_controlled=*/false, &dst_bounds);

  GetTarget()->SetBoundsInScreen(dst_bounds, dst_display);
}

}  // namespace ash
