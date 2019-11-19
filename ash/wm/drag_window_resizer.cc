// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

void RecursiveSchedulePainter(ui::Layer* layer) {
  if (!layer)
    return;
  layer->SchedulePaint(gfx::Rect(layer->size()));
  for (auto* child : layer->children())
    RecursiveSchedulePainter(child);
}

}  // namespace

// static
DragWindowResizer* DragWindowResizer::instance_ = NULL;

DragWindowResizer::~DragWindowResizer() {
  if (window_state_)
    window_state_->DeleteDragDetails();
  Shell* shell = Shell::Get();
  shell->mouse_cursor_filter()->set_mouse_warp_enabled(true);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();
  if (instance_ == this)
    instance_ = NULL;
}

void DragWindowResizer::Drag(const gfx::Point& location, int event_flags) {
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
  next_window_resizer_->CompleteDrag();
  EndDragImpl();
}

void DragWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  drag_window_controller_.reset();
}

void DragWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  next_window_resizer_->FlingOrSwipe(event);
  EndDragImpl();
}

DragWindowResizer::DragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(std::move(next_window_resizer)) {
  // The pointer should be confined in one display during resizing a window
  // because the window cannot span two displays at the same time anyway. The
  // exception is window/tab dragging operation. During that operation,
  // |mouse_warp_mode_| should be set to WARP_DRAG so that the user could move a
  // window/tab to another display.
  MouseCursorEventFilter* mouse_cursor_filter =
      Shell::Get()->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_enabled(ShouldAllowMouseWarp());
  if (ShouldAllowMouseWarp())
    mouse_cursor_filter->ShowSharedEdgeIndicator(GetTarget()->GetRootWindow());
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

  // TODO(malaykeshav) - This is temporary fix/workaround that keeps performance
  // but may not give the best UI while dragging. See https://crbug/834114
  RecursiveSchedulePainter(GetTarget()->layer());

  // Check if the destination is another display.
  if (details().source == wm::WINDOW_MOVE_SOURCE_TOUCH)
    return;
  aura::Window* root_window = GetTarget()->GetRootWindow();
  const display::Display dst_display =
      Shell::Get()->cursor_manager()->GetDisplay();
  if (dst_display.id() ==
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window).id()) {
    return;
  }

  // Adjust the size and position so that it doesn't exceed the size of work
  // area.
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
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_mouse_location_in_screen);
  if (!dst_bounds.Contains(last_mouse_location_in_screen)) {
    if (last_mouse_location_in_screen.x() < dst_bounds.x())
      dst_bounds.set_x(last_mouse_location_in_screen.x());
    else if (last_mouse_location_in_screen.x() > dst_bounds.right())
      dst_bounds.set_x(last_mouse_location_in_screen.x() - dst_bounds.width());
  }
  AdjustBoundsToEnsureMinimumWindowVisibility(dst_display.bounds(),
                                              &dst_bounds);

  GetTarget()->SetBoundsInScreen(dst_bounds, dst_display);
}

}  // namespace ash
