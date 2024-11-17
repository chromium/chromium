// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_
#define ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"

namespace views {
class BubbleDialogDelegateView;
class View;
}  // namespace views

namespace ash {

// The implementation of tooltip bubbles for the shelf.
class ASH_EXPORT ShelfTooltipBubble : public ShelfBubble {
 public:
  ShelfTooltipBubble(views::View* anchor,
                     ShelfAlignment alignment,
                     const std::u16string& text,
                     std::optional<views::BubbleBorder::Arrow> arrow_position);

  ShelfTooltipBubble(const ShelfTooltipBubble&) = delete;
  ShelfTooltipBubble& operator=(const ShelfTooltipBubble&) = delete;

 protected:
  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_
