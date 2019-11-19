// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_BAR_ITEM_BORDER_H_
#define ASH_WM_DESKS_DESKS_BAR_ITEM_BORDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"

namespace ash {

// Defines a border to be used on the views of the desks bar, such as the
// DeskPreviewView and the NewDeskButton. This paints a border around the view
// with an empty gap (padding) in-between, so that the border is more obvious
// against white or light backgrounds. If a |SK_ColorTRANSPARENT| was provided,
// it will paint nothing.
class DesksBarItemBorder : public views::Border {
 public:
  explicit DesksBarItemBorder(int corner_radius);
  ~DesksBarItemBorder() override = default;

  void set_color(SkColor color) { color_ = color; }

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int corner_radius_;

  SkColor color_ = SK_ColorTRANSPARENT;

  DISALLOW_COPY_AND_ASSIGN(DesksBarItemBorder);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_BAR_ITEM_BORDER_H_
