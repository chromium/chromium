// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_item_border.h"

#include "cc/paint/paint_flags.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The desk preview border size and padding in dips.
constexpr int kBorderSize = 2;
constexpr int kBorderPadding = 2;

}  // namespace

DesksBarItemBorder::DesksBarItemBorder(int corner_radius)
    : corner_radius_(corner_radius) {}

void DesksBarItemBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  if (color_ == SK_ColorTRANSPARENT)
    return;

  cc::PaintFlags flags;
  flags.setStrokeWidth(kBorderSize);
  flags.setColor(color_);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  gfx::RectF bounds(view.GetLocalBounds());
  // The following inset is needed for the rounded corners of the border to
  // look correct. Otherwise, the borders will be painted at the edge of the
  // view, resulting in this border looking chopped.
  bounds.Inset(kBorderSize / 2, kBorderSize / 2);
  canvas->DrawRoundRect(bounds, corner_radius_, flags);
}

gfx::Insets DesksBarItemBorder::GetInsets() const {
  constexpr gfx::Insets kInsets{kBorderSize + kBorderPadding};
  return kInsets;
}

gfx::Size DesksBarItemBorder::GetMinimumSize() const {
  constexpr gfx::Size kMinSize{2 * (kBorderSize + kBorderPadding),
                               2 * (kBorderSize + kBorderPadding)};
  return kMinSize;
}

}  // namespace ash
