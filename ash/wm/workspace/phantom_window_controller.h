// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
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

  PhantomWindowController(const PhantomWindowController&) = delete;
  PhantomWindowController& operator=(const PhantomWindowController&) = delete;

  // Hides the phantom window without any animation.
  ~PhantomWindowController();

  // Shows the phantom window and animates expanding from
  // |kScrimStartBoundsRatio| of the full size to the full size which is
  // `snap_window_bounds_in_screen.Insets(kPhantomWindowInsets)`.
  void Show(const gfx::Rect& window_bounds_in_screen);
  void HideMaximizeCue();
  void ShowMaximizeCue();

  // Transforms the phantom widget from top-snapped to maximized phantom for
  // the target maximized window bounds |window_bounds_in_screen|.
  void TransformPhantomWidgetFromSnapTopToMaximize(
      const gfx::Rect& window_bounds_in_screen);

  // Returns the target snapped or maximized window bounds which is the phantom
  // bounds |target_bounds_in_screen_| with offsets |kPhantomWindowInsets|.
  gfx::Rect GetTargetWindowBounds() const;

  aura::Window* window() { return window_; }

  // Returns |maximize_cue_widget_|.
  views::Widget* GetMaximizeCueForTesting() const;

  // Returns |target_bounds_in_screen_|.
  const gfx::Rect& GetTargetBoundsInScreenForTesting() const;

 private:
  // Creates, shows and returns a phantom widget at |bounds|
  // with kShellWindowId_ShelfContainer in |root_window| as a parent.
  std::unique_ptr<views::Widget> CreatePhantomWidget(
      aura::Window* root_window,
      const gfx::Rect& bounds_in_screen);

  // Creates and returns a maximize cue widget in
  // |kShellWindowId_OverlayContainer| in a given |root_window|.
  std::unique_ptr<views::Widget> CreateMaximizeCue(aura::Window* root_window);

  // Show phantom widget animating from the current widget size to
  // |target_bounds_in_screen| and animating to full opacity.
  void ShowPhantomWidget();

  // Window that the phantom window is stacked above.
  raw_ptr<aura::Window> window_;

  // Target bounds of |phantom_widget_| in screen coordinates for animation.
  gfx::Rect target_bounds_in_screen_;

  // Phantom representation of the window.
  std::unique_ptr<views::Widget> phantom_widget_;

  // Maximize cue on top-snap phantom to inform users to hold longer if they
  // want to maximize instead of snap top.
  std::unique_ptr<views::Widget> maximize_cue_widget_;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
