// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_windows_hider.h"
#include "base/memory/raw_ptr.h"

#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

TabDragDropWindowsHider::TabDragDropWindowsHider(aura::Window* source_window)
    : source_window_(source_window) {
  DCHECK(source_window_);

  root_window_ = source_window_->GetRootWindow();

  // Disable the backdrop for |source_window_| during dragging.
  WindowBackdrop::Get(source_window_)->DisableBackdrop();

  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (aura::Window* window : windows) {
    if (window == source_window_ || window->GetRootWindow() != root_window_) {
      continue;
    }

    window_visibility_map_.emplace(window, window->IsVisible());
    if (window->IsVisible()) {
      wm::ScopedAnimationDisabler disabler(window);
      window->Hide();
    }
    window->AddObserver(this);
  }

  // Hide the home launcher if it's enabled during dragging.
  Shell::Get()->app_list_controller()->OnWindowDragStarted();

  // Blurs the wallpaper background.
  RootWindowController::ForWindow(root_window_)
      ->wallpaper_widget_controller()
      ->SetWallpaperBlur(ColorProvider::kBackgroundBlurSigma);

  // `root_window_` might became nullptr during drag&drop. See b/276736023 for
  // details.
  root_window_->AddObserver(this);
}

TabDragDropWindowsHider::~TabDragDropWindowsHider() {
  // It might be possible that |source_window_| is destroyed during dragging.
  if (source_window_) {
    WindowBackdrop::Get(source_window_)->RestoreBackdrop();
  }

  for (auto iter = window_visibility_map_.begin();
       iter != window_visibility_map_.end(); ++iter) {
    iter->first->RemoveObserver(this);
    if (iter->second) {
      wm::ScopedAnimationDisabler disabler(iter->first);
      iter->first->Show();
    }
  }

  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

  // May reshow the home launcher after dragging.
  Shell::Get()->app_list_controller()->OnWindowDragEnded(
      /*animate=*/false);

  // Clears the background wallpaper blur.
  if (root_window_) {
    RootWindowController::ForWindow(root_window_)
        ->wallpaper_widget_controller()
        ->SetWallpaperBlur(wallpaper_constants::kClear);
    root_window_->RemoveObserver(this);
  }
}

void TabDragDropWindowsHider::OnWindowDestroying(aura::Window* window) {
  if (window == source_window_) {
    source_window_ = nullptr;
    return;
  }

  if (window == root_window_) {
    root_window_ = nullptr;
    return;
  }

  window->RemoveObserver(this);
  window_visibility_map_.erase(window);
}

void TabDragDropWindowsHider::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  // The window object is not necessarily the one that is being observed.
  // So we only take action if the window is currently being observed.
  if (window_visibility_map_.count(window) == 0) {
    return;
  }

  if (visible) {
    // Do not let |window| change to visible during the lifetime of |this|.
    // Also update |window_visibility_map_| so that we can restore the window
    // visibility correctly.
    window->Hide();
    window_visibility_map_[window] = visible;
  }
  // else do nothing. It must come from Hide() function above thus should be
  // ignored.
}

int TabDragDropWindowsHider::GetWindowVisibilityMapSizeForTesting() const {
  return window_visibility_map_.size();
}

}  // namespace ash
