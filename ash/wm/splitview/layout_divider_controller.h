// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_

#include "ash/wm/splitview/split_view_types.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Defines an interface for the delegate to handle events forwarded from
// `SplitViewDivider`. Implementations of this interface may do special handling
// during resizing, such as adjusting or translating the window bounds for
// performant resizing.
class LayoutDividerController {
 public:
  // Returns the root window for the layout.
  virtual aura::Window* GetRootWindow() const = 0;

  // Resizing functions used when resizing via the divider, where
  // `location_in_screen` is the location of the event that started this resize
  // and will be used to calculate the divider position.
  virtual void StartResizeWithDivider(const gfx::Point& location_in_screen) = 0;
  virtual void UpdateResizeWithDivider(
      const gfx::Point& location_in_screen) = 0;

  // Returns true if the delegate is finished with resizing and can hand back
  // resizing work to `SplitViewDivider`, otherwise returns false, e.g. if the
  // divider is performing a snap animation.
  virtual bool EndResizeWithDivider(const gfx::Point& location_in_screen) = 0;

  // Called when the divider is about to end resizing by finishing window
  // resizing and cleaning up drag details.
  virtual void OnResizeEnding() = 0;

  // Called when the divider has finished cleaning up window resizing.
  virtual void OnResizeEnded() = 0;

  // Swaps the window(s). If in tablet mode, it is triggered by `kDoubleTap`
  // with only one window snapped, the window will be snapped to the other
  // position. For all other cases with the windows in `GetLayoutWindows()`
  // available, the two windows will be swapped together with their bounds.
  virtual void SwapWindows() = 0;

  // Gets snapped bounds in screen coordinates based on `snap_position` and
  // `snap_ratio`. The snapped bounds are updated to accommodate for the
  // `SplitViewDivider` so that the windows and `SplitViewDivider` are not
  // overlapped.
  virtual gfx::Rect GetSnappedWindowBoundsInScreen(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size,
      float snap_ratio,
      bool account_for_divider_width) const = 0;

  // Returns the snap position of the given snapped `window`.
  // `SnapPosition::kPrimary` is returned if the window is the primary snapped
  // window. `SnapPosition::kSecondary` is returned if the window is the
  // secondary snapped window.
  virtual SnapPosition GetPositionOfSnappedWindow(
      const aura::Window* window) const = 0;

 protected:
  virtual ~LayoutDividerController() = default;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_
