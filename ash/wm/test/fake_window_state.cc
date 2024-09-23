// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test/fake_window_state.h"

#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"

namespace ash {

FakeWindowState::FakeWindowState(chromeos::WindowStateType initial_state_type)
    : state_type_(initial_state_type) {}

FakeWindowState::~FakeWindowState() = default;

void FakeWindowState::OnWMEvent(WindowState* window_state,
                                const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_MINIMIZE:
      was_visible_on_minimize_ = window_state->window()->IsVisible();
      break;
    case WM_EVENT_SET_BOUNDS:
      last_requested_bounds_ =
          event->AsSetBoundsWMEvent()->requested_bounds_in_parent();
      break;
    default:
      break;
  }
}

chromeos::WindowStateType FakeWindowState::GetType() const {
  return state_type_;
}

FakeWindowStateDelegate::FakeWindowStateDelegate() = default;

FakeWindowStateDelegate::~FakeWindowStateDelegate() = default;

bool FakeWindowStateDelegate::ToggleFullscreen(WindowState* window_state) {
  return false;
}

void FakeWindowStateDelegate::ToggleLockedFullscreen(
    WindowState* window_state) {
  ++toggle_locked_fullscreen_count_;
}

std::unique_ptr<PresentationTimeRecorder>
FakeWindowStateDelegate::OnDragStarted(int component) {
  drag_in_progress_ = true;
  drag_start_component_ = component;
  return nullptr;
}

void FakeWindowStateDelegate::OnDragFinished(bool cancel,
                                             const gfx::PointF& location) {
  drag_in_progress_ = false;
  drag_end_location_ = location;
}

}  // namespace ash
