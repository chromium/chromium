// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/high_contrast/high_contrast_controller.h"

#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"

namespace ash {

HighContrastController::HighContrastController() : enabled_(false) {
  Shell::Get()->AddShellObserver(this);
}

HighContrastController::~HighContrastController() {
  Shell::Get()->RemoveShellObserver(this);
}

void HighContrastController::SetEnabled(bool enabled) {
  enabled_ = enabled;

  // Update all active displays.
  aura::Window::Windows root_window_list = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator it = root_window_list.begin();
       it != root_window_list.end(); it++) {
    UpdateDisplay(*it);
  }
}

void HighContrastController::UpdateDisplay(aura::Window* root_window) {
  root_window->layer()->SetLayerInverted(enabled_);
}

void HighContrastController::OnRootWindowAdded(aura::Window* root_window) {
  UpdateDisplay(root_window);
}

}  // namespace ash
