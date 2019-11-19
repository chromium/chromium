// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_hide_windows.h"

#include "base/logging.h"
#include "ui/aura/window.h"

namespace ash {

ScopedOverviewHideWindows::ScopedOverviewHideWindows(
    const std::vector<aura::Window*>& windows,
    bool force_hidden)
    : force_hidden_(force_hidden) {
  for (auto* window : windows) {
    window->AddObserver(this);
    window_visibility_.emplace(window, window->IsVisible());
    window->Hide();
  }
}

ScopedOverviewHideWindows::~ScopedOverviewHideWindows() {
  for (auto iter = window_visibility_.begin(); iter != window_visibility_.end();
       iter++) {
    iter->first->RemoveObserver(this);
    if (iter->second)
      iter->first->Show();
  }
}

void ScopedOverviewHideWindows::OnWindowDestroying(aura::Window* window) {
  window_visibility_.erase(window);
  window->RemoveObserver(this);
}

void ScopedOverviewHideWindows::OnWindowVisibilityChanged(aura::Window* window,
                                                          bool visible) {
  if (!visible)
    return;

  // It's expected that windows hidden in overview, unless they are forcefully
  // hidden should not be shown while in overview.
  if (!force_hidden_)
    NOTREACHED();

  // Do not let |window| change to visible during the lifetime of |this|. Also
  // update |window_visibility_| so that we can restore the window visibility
  // correctly.
  window->Hide();
  window_visibility_[window] = true;
}

}  // namespace ash
