// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_
#define ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace gfx {
class Point;
}

namespace ui {
class Event;
}

namespace views {
class View;
}

namespace aura {
class Window;
}

namespace ash {

// Interface provided to ShelfTooltipManager to create the tooltip for children.
class ASH_EXPORT ShelfTooltipDelegate {
 public:
  ShelfTooltipDelegate() = default;
  virtual ~ShelfTooltipDelegate() = default;

  // Returns true if a tooltip should be shown for |view|.
  virtual bool ShouldShowTooltipForView(const views::View* view) const = 0;

  // Returns true if the mouse cursor exits the area for launcher tooltip.
  virtual bool ShouldHideTooltip(const gfx::Point& cursor_point) const = 0;

  // Returns the list of open windows that correspond to the app represented by
  // this shelf view.
  virtual const std::vector<aura::Window*> GetOpenWindowsForView(
      views::View* view) = 0;

  // Returns the title of |view|.
  virtual base::string16 GetTitleForView(const views::View* view) const = 0;

  // Returns the view that should handle |event|.
  virtual views::View* GetViewForEvent(const ui::Event& event) = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_