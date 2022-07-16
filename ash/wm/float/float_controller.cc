// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "base/check_op.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

FloatController::FloatController() = default;

FloatController::~FloatController() = default;

void FloatController::ToggleFloatCurrentWindow(aura::Window* window) {
  DCHECK(features::IsWindowControlMenuEnabled());
  // If try to float the same window again, will toggle unfloat.
  // Since only one floating window is allowed, reset other floating window.
  if (window == ResetFloatedWindow())
    return;
  // Float current window.
  float_window_ = window;
  float_window_observation_.Observe(float_window_);
  aura::Window* float_container =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  if (window->parent() != float_container)
    float_container->AddChild(window);
}

bool FloatController::IsFloated(aura::Window* window) const {
  DCHECK(window);
  return float_window_ == window;
}

void FloatController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(float_window_, window);
  float_window_observation_.Reset();
  float_window_ = nullptr;
}

aura::Window* FloatController::ResetFloatedWindow() {
  aura::Window* floated_window = float_window_;
  if (float_window_) {
    // Reparent window to active desk container.
    desks_util::GetActiveDeskContainerForRoot(float_window_->GetRootWindow())
        ->AddChild(float_window_);
    float_window_observation_.Reset();
    float_window_ = nullptr;
  }
  return floated_window;
}

}  // namespace ash