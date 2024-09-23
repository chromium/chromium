// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include <cmath>

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/mouse_warp_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {

MouseCursorEventFilter::MouseCursorEventFilter() : mouse_warp_enabled_(true) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
}

void MouseCursorEventFilter::ShowSharedEdgeIndicator(aura::Window* from) {
  mouse_warp_controller_ =
      CreateMouseWarpController(Shell::Get()->display_manager(), from);
}

void MouseCursorEventFilter::HideSharedEdgeIndicator() {
  OnDidApplyDisplayChanges();
}

void MouseCursorEventFilter::OnDisplaysInitialized() {
  OnDidApplyDisplayChanges();
}

void MouseCursorEventFilter::OnDidApplyDisplayChanges() {
  mouse_warp_controller_ =
      CreateMouseWarpController(Shell::Get()->display_manager(), nullptr);
}

void MouseCursorEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  // Don't warp due to synthesized event.
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  // Handle both MOVED and DRAGGED events here because when the mouse pointer
  // enters the other root window while dragging, the underlying window system
  // (at least X11) stops generating a ui::EventType::kMouseMoved event.
  if (event->type() != ui::EventType::kMouseMoved &&
      event->type() != ui::EventType::kMouseDragged) {
    return;
  }

  bool mouse_warp_enabled =
      mouse_warp_enabled_ &&
      (event->flags() & ui::EF_NOT_SUITABLE_FOR_MOUSE_WARPING) == 0;

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->UpdateLocation();
  mouse_warp_controller_->SetEnabled(mouse_warp_enabled);

  if (mouse_warp_controller_->WarpMouseCursor(event))
    event->StopPropagation();
}

}  // namespace ash
