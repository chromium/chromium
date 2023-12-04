// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_controller.h"

#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

PipController::PipController() = default;
PipController::~PipController() = default;

void PipController::SetPipWindow(aura::Window* window) {
  if (!window || pip_window_ == window) {
    return;
  }

  if (pip_window_) {
    // As removing ARC/Lacros PiP is async, a new PiP could be created before
    // the currently one is fully removed.
    UnsetPipWindow(pip_window_);
  }

  pip_window_ = window;
  pip_window_observation_.Reset();
  pip_window_observation_.Observe(window);
}

void PipController::UnsetPipWindow(aura::Window* window) {
  if (!pip_window_ || pip_window_ != window) {
    // This function can be called with one of window state, visibility, or
    // existence changes, all of which are valid.
    return;
  }
  pip_window_observation_.Reset();
  pip_window_ = nullptr;
}

void PipController::UpdatePipBounds() {
  if (!pip_window_) {
    // It's a bit hard for the caller of this function to tell when PiP is
    // really active (v.s. A PiP window just exists), so allow calling this
    // when not appropriate.
    return;
  }
  WindowState* window_state = WindowState::Get(pip_window_);
  gfx::Rect new_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(window_state);
  wm::ConvertRectFromScreen(pip_window_->GetRootWindow(), &new_bounds);
  if (pip_window_->bounds() != new_bounds) {
    SetBoundsWMEvent event(new_bounds, /*animate=*/true);
    window_state->OnWMEvent(&event);
  }
}

void PipController::OnWindowDestroying(aura::Window* window) {
  // Ensure to clean up when PiP is gone especially in unit tests.
  UnsetPipWindow(window);
}

}  // namespace ash
