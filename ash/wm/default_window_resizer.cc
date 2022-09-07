// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

DefaultWindowResizer::~DefaultWindowResizer() {
  Shell::Get()->cursor_manager()->UnlockCursor();
}

// static
std::unique_ptr<DefaultWindowResizer> DefaultWindowResizer::Create(
    WindowState* window_state) {
  return base::WrapUnique(new DefaultWindowResizer(window_state));
}

void DefaultWindowResizer::Drag(const gfx::PointF& location, int event_flags) {
  gfx::Rect bounds(CalculateBoundsForDrag(location));
  if (bounds != GetTarget()->bounds()) {
    if (!did_move_or_resize_ && !details().restore_bounds_in_parent.IsEmpty())
      window_state_->ClearRestoreBounds();
    did_move_or_resize_ = true;
    SetBoundsDuringResize(bounds);
  }
}

void DefaultWindowResizer::CompleteDrag() {}

void DefaultWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  GetTarget()->SetBounds(details().initial_bounds_in_parent);

  if (!details().restore_bounds_in_parent.IsEmpty())
    window_state_->SetRestoreBoundsInParent(details().restore_bounds_in_parent);
}

void DefaultWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {}

DefaultWindowResizer::DefaultWindowResizer(WindowState* window_state)
    : WindowResizer(window_state) {
  DCHECK(details().is_resizable);
  Shell::Get()->cursor_manager()->LockCursor();
}

}  // namespace ash
