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

  pip_window_ = window;
  pip_window_observation_.Reset();
  pip_window_observation_.Observe(window);
}

void PipController::UnsetPipWindow() {
  pip_window_observation_.Reset();
  pip_window_ = nullptr;
}

void PipController::UpdatePipBounds() {
  CHECK(pip_window_);
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
  CHECK(window == pip_window_);
  pip_window_observation_.Reset();
  pip_window_ = nullptr;
}

}  // namespace ash
