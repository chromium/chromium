// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_

#include "ui/aura/window.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

// Defines an interface for the delegate to handle events forwarded from
// `SplitViewDivider`. Implementations of this interface may do special handling
// during resizing, such as adjusting or translating the window bounds for
// performant resizing.
class LayoutDividerController {
 public:
  // Resizing functions used when resizing via the divider, where
  // `location_in_screen` is the location of the event that started this resize
  // and will be used to calculate the divider position.
  virtual void StartResizeWithDivider(const gfx::Point& location_in_screen) = 0;
  virtual void UpdateResizeWithDivider(
      const gfx::Point& location_in_screen) = 0;
  virtual void EndResizeWithDivider(const gfx::Point& location_in_screen) = 0;

  // Returns the windows associated with this delegate.
  virtual aura::Window::Windows GetLayoutWindows() const = 0;

 protected:
  virtual ~LayoutDividerController() = default;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_LAYOUT_DIVIDER_CONTROLLER_H_
