// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_POSITIONER_H_
#define ASH_WM_WINDOW_POSITIONER_H_

#include "ash/ash_export.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {

// A collection of utilities that assist with placing new windows.
class ASH_EXPORT WindowPositioner {
 public:
  // Computes and returns the bounds and show state for new window
  // based on the parameter passed AND existing windows. |is_saved_bounds|
  // indicates the |bounds_in_out| is the saved bounds.
  static void GetBoundsAndShowStateForNewWindow(
      bool is_saved_bounds,
      ui::WindowShowState show_state_in,
      gfx::Rect* bounds_in_out,
      ui::WindowShowState* show_state_out);

  // Check if after removal or hide of the given |removed_window| an
  // automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnHideOrRemove(
      const aura::Window* removed_window);

  // Turn the automatic positioning logic temporarily off. Returns the previous
  // state.
  static bool DisableAutoPositioning(bool ignore);

  // Check if after insertion or showing of the given |added_window|
  // an automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnShow(aura::Window* added_window);

  WindowPositioner();

  WindowPositioner(const WindowPositioner&) = delete;
  WindowPositioner& operator=(const WindowPositioner&) = delete;

  ~WindowPositioner();

 protected:
  friend class WindowPositionerTest;

  static constexpr int kWindowOffset = 32;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_POSITIONER_H_
