// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_SESSION_WINDOWS_HIDER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_SESSION_WINDOWS_HIDER_H_

#include <map>

#include "ash/ash_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Hides all visible windows except the source window and the dragged
// window (if applicable), and restores the windows' visibility upon its
// destruction. It also blurs and darkens the background, hides the home
// launcher if home launcher is enabled. Only need to do so if we need
// to scale up and down the source window when dragging a tab window out
// of it.
class ASH_EXPORT TabletModeBrowserWindowDragSessionWindowsHider
    : public aura::WindowObserver {
 public:
  TabletModeBrowserWindowDragSessionWindowsHider(aura::Window* source_window,
                                                 aura::Window* dragged_window);

  TabletModeBrowserWindowDragSessionWindowsHider(
      const TabletModeBrowserWindowDragSessionWindowsHider&) = delete;
  TabletModeBrowserWindowDragSessionWindowsHider& operator=(
      const TabletModeBrowserWindowDragSessionWindowsHider&) = delete;

  ~TabletModeBrowserWindowDragSessionWindowsHider() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  int GetWindowVisibilityMapSizeForTesting() const;

 private:
  // The window from which the drag originated.
  aura::Window* source_window_;

  // The currently dragged window, if applicable.
  aura::Window* dragged_window_;

  // The root window the drag is taking place within. Guaranteed to be
  // non-null during the lifetime of |this|.
  aura::Window* root_window_;

  // Maintains the map between windows and their visibilities. All windows
  // except the dragged window and the source window should stay hidden during
  // dragging.
  std::map<aura::Window*, bool> window_visibility_map_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_SESSION_WINDOWS_HIDER_H_
