// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "base/check_op.h"
#include "chromeos/ui/base/window_properties.h"

namespace ash {

FloatController::FloatController() = default;

FloatController::~FloatController() = default;

bool FloatController::IsFloated(aura::Window* window) const {
  DCHECK(window);
  return float_window_ == window;
}

void FloatController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(float_window_, window);
  float_window_observation_.Reset();
  float_window_ = nullptr;
}

void FloatController::Float(aura::Window* window) {
  // Only one floating window is allowed, reset previously floated window.
  ResetFloatedWindow();
  DCHECK(!float_window_);
  DCHECK(window->GetProperty(chromeos::kWindowToggleFloatKey));
  float_window_ = window;
  float_window_observation_.Observe(float_window_);
  aura::Window* float_container =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  if (window->parent() != float_container)
    float_container->AddChild(window);
}

void FloatController::Unfloat(aura::Window* window) {
  DCHECK(!window->GetProperty(chromeos::kWindowToggleFloatKey));
  //  Re-parent window to active desk container.
  desks_util::GetActiveDeskContainerForRoot(float_window_->GetRootWindow())
      ->AddChild(float_window_);
  float_window_observation_.Reset();
  float_window_ = nullptr;
}

void FloatController::ResetFloatedWindow() {
  if (float_window_)
    float_window_->SetProperty(chromeos::kWindowToggleFloatKey, false);
}

}  // namespace ash