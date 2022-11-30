// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_window_hider.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/containers/adapters.h"
#include "base/containers/cxx20_erase_vector.h"

namespace ash {

GlanceablesWindowHider::GlanceablesWindowHider() {
  std::vector<aura::Window*> windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);

  // Process the windows from back to front, so that minimizing the windows
  // doesn't change the stacking order.
  for (aura::Window* window : base::Reversed(windows)) {
    WindowState* window_state = WindowState::Get(window);

    // Ignore windows that are already minimized.
    if (window_state->IsMinimized())
      continue;

    window->AddObserver(this);
    windows_.push_back(window);

    window_state->Minimize();
  }
}

GlanceablesWindowHider::~GlanceablesWindowHider() {
  // `windows_` is stored back-to-front, so unminimizing in order will restore
  // the stacking order.
  for (aura::Window* window : windows_) {
    window->RemoveObserver(this);
    WindowState* window_state = WindowState::Get(window);
    // Window might not be minimized if the user manually restored it.
    if (window_state->IsMinimized()) {
      window_state->Unminimize();
    }
  }
}

void GlanceablesWindowHider::OnWindowDestroying(aura::Window* window) {
  // aura::Window removes observers on window destruction, so no need to remove
  // the observer here.
  base::Erase(windows_, window);
}

}  // namespace ash
