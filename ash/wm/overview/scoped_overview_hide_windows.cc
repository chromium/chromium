// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_hide_windows.h"

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace ash {

ScopedOverviewHideWindows::ScopedOverviewHideWindows(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
    bool force_hidden)
    : force_hidden_(force_hidden) {
  for (aura::Window* window : windows) {
    AddWindow(window);
  }
}

ScopedOverviewHideWindows::~ScopedOverviewHideWindows() {
  for (const auto& element : window_visibility_) {
    element.first->RemoveObserver(this);

    // While in the middle of the destructor of `aura::Window`,
    // `OverviewItem::OnWindowDestroying()` might be called before
    // `ScopedOverviewHideWindows::OnWindowDestroying()`. It eventually invokes
    // `OverviewGrid::RemoveItem()`, and finally reaches here. Therefore, we
    // need to check if the hidden window is going to be destroyed.
    if (!element.first->is_destroying() && element.second)
      element.first->Show();
  }
}

bool ScopedOverviewHideWindows::HasWindow(aura::Window* window) const {
  return base::Contains(window_visibility_, window);
}

void ScopedOverviewHideWindows::AddWindow(aura::Window* window) {
  window->AddObserver(this);

  // Stores `TargetVisibility()` in `window_visibility_`, which directly
  // assesses the window's target visibility, regardless of the visibility of
  // its parent's layer.
  window_visibility_.emplace(window, window->TargetVisibility());
  window->Hide();
}

void ScopedOverviewHideWindows::RemoveWindow(aura::Window* window,
                                             bool show_window) {
  DCHECK(HasWindow(window));
  window->RemoveObserver(this);
  if (!window->is_destroying() && window_visibility_[window] && show_window)
    window->Show();
  window_visibility_.erase(window);
}

void ScopedOverviewHideWindows::RemoveAllWindows() {
  std::vector<aura::Window*> windows_to_remove;
  windows_to_remove.reserve(window_visibility_.size());
  for (const auto& element : window_visibility_)
    windows_to_remove.push_back(element.first);
  for (auto* window : base::Reversed(windows_to_remove))
    RemoveWindow(window, /*show_window=*/true);
}

void ScopedOverviewHideWindows::OnWindowDestroying(aura::Window* window) {
  window_visibility_.erase(window);
  window->RemoveObserver(this);
}

void ScopedOverviewHideWindows::OnWindowVisibilityChanged(aura::Window* window,
                                                          bool visible) {
  if (!visible)
    return;

  // If it's not one of the registered windows, then it must be a child of the
  // registered windows. Early return in this case.
  if (!HasWindow(window))
    return;

  // It's expected that windows hidden in overview, unless they are forcefully
  // hidden should not be shown while in overview.
  CHECK(force_hidden_);

  // Do not let |window| change to visible during the lifetime of |this|. Also
  // update |window_visibility_| so that we can restore the window visibility
  // correctly.
  window->Hide();
  window_visibility_[window] = true;
}

}  // namespace ash
