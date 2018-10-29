// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// PhantomWindowController is responsible for showing a phantom representation
// of a window. It's used to show a preview of how snapping or docking a window
// will affect the window's bounds.
class ASH_EXPORT PhantomWindowController {
 public:
  explicit PhantomWindowController(aura::Window* window);

  // Hides the phantom window without any animation.
  ~PhantomWindowController();

  // Shows the phantom window and animates shrinking it to |bounds_in_screen|.
  void Show(const gfx::Rect& bounds_in_screen);

  aura::Window* window() { return window_; }

 private:
  // Creates, shows and returns a phantom widget at |bounds|
  // with kShellWindowId_ShelfContainer in |root_window| as a parent.
  std::unique_ptr<views::Widget> CreatePhantomWidget(
      aura::Window* root_window,
      const gfx::Rect& bounds_in_screen);

  // Window that the phantom window is stacked above.
  aura::Window* window_;

  // Target bounds (including the shadows if any) of the animation in screen
  // coordinates.
  gfx::Rect target_bounds_in_screen_;

  // Phantom representation of the window.
  std::unique_ptr<views::Widget> phantom_widget_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
