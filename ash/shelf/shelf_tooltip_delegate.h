// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_
#define ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/types/expected.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ui {
class Event;
}

namespace views {
class View;
}

namespace ash {

// Interface provided to ShelfTooltipManager to create the tooltip for children.
class ASH_EXPORT ShelfTooltipDelegate {
 public:
  // The reason why no window preview should be shown for a shelf item.
  enum class NoPreviewReason {
    // The shelf item has multiple windows and uses an application menu instead
    // of a tooltip.
    kUseAppMenu,
    // The view is not a shelf item, or its shelf item has no open window.
    kNoSingleWindow,
  };

  ShelfTooltipDelegate() = default;
  virtual ~ShelfTooltipDelegate() = default;

  // Returns true if a tooltip should be shown for |view|.
  virtual bool ShouldShowTooltipForView(const views::View* view) const = 0;

  // Returns true if the mouse cursor exits the area for shelf tooltip, in
  // the coordinates of the `delegate_view`.
  virtual bool ShouldHideTooltip(const gfx::Point& cursor_point,
                                 views::View* delegate_view) const = 0;

  // Returns the title of |view|.
  virtual std::u16string GetTitleForView(const views::View* view) const = 0;

  // Returns the single open window of `view`'s shelf item to preview. If the
  // shelf item has multiple windows, shows the application menu if
  // `show_app_menu` is true and returns `NoPreviewReason::kUseAppMenu`, in
  // which case a tooltip bubble should not be shown.
  virtual base::expected<aura::Window*, NoPreviewReason>
  GetPreviewWindowOrShowAppMenu(views::View* view, bool show_app_menu) = 0;

  // Returns the view that should handle |event|.
  virtual views::View* GetViewForEvent(const ui::Event& event) = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_DELEGATE_H_
