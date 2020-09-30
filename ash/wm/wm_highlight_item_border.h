// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_
#define ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"

namespace ash {

// Defines a border to be used on the views of window management items which can
// be highlighted in overview or window cycle, such as the DeskPreviewView,
// NewDeskButton and WindowMiniView. This paints a border around the view with
// an empty gap (padding) in-between, so that the border is more obvious against
// white or light backgrounds. If a |SK_ColorTRANSPARENT| was provided, it will
// paint nothing.
class WmHighlightItemBorder : public views::Border {
 public:

  explicit WmHighlightItemBorder(
      int corner_radius,
      gfx::Insets padding = gfx::Insets(0));
  ~WmHighlightItemBorder() override = default;

  // This highlight meant to indicate focus. No border will be painted if
  // |focused| is false.
  void SetFocused(bool focused);

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int corner_radius_;

  gfx::Insets border_insets_;

  DISALLOW_COPY_AND_ASSIGN(WmHighlightItemBorder);
};

}  // namespace ash

#endif  // ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_
