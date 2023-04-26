// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_dimmer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/window_user_data.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/window_dimmer.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

// Opacity when it's dimming the entire screen.
constexpr float kDimmingLayerOpacityForRoot = 0.4f;

}  // namespace

ScreenDimmer::ScreenDimmer()
    : window_dimmers_(std::make_unique<WindowUserData<WindowDimmer>>()) {
  Shell::Get()->AddShellObserver(this);
}

ScreenDimmer::~ScreenDimmer() {
  // Usage in chrome results in ScreenDimmer outliving the shell.
  if (Shell::HasInstance())
    Shell::Get()->RemoveShellObserver(this);
}

void ScreenDimmer::SetDimming(bool should_dim) {
  if (should_dim == is_dimming_)
    return;
  is_dimming_ = should_dim;

  Update(should_dim);
}

void ScreenDimmer::OnRootWindowAdded(aura::Window* root_window) {
  Update(is_dimming_);
}

void ScreenDimmer::Update(bool should_dim) {
  for (aura::Window* container : Shell::GetAllRootWindows()) {
    WindowDimmer* window_dimmer = window_dimmers_->Get(container);
    if (should_dim) {
      if (!window_dimmer) {
        window_dimmers_->Set(container,
                             std::make_unique<WindowDimmer>(container));
        window_dimmer = window_dimmers_->Get(container);
        window_dimmer->SetDimOpacity(kDimmingLayerOpacityForRoot);
      }
      if (at_bottom_)
        container->StackChildAtBottom(window_dimmer->window());
      else
        container->StackChildAtTop(window_dimmer->window());
      window_dimmer->window()->Show();
    } else if (window_dimmer) {
      window_dimmers_->Set(container, nullptr);
    }
  }
}

}  // namespace ash
