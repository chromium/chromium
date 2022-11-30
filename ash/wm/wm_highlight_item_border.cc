// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_highlight_item_border.h"

#include "ash/style/ash_color_provider.h"
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

WmHighlightItemBorder::WmHighlightItemBorder(int corner_radius,
                                             gfx::Insets padding)
    : views::Border(SK_ColorTRANSPARENT),
      corner_radius_(corner_radius),
      border_insets_(gfx::Insets(kBorderSize + kBorderPadding) + padding) {}

bool WmHighlightItemBorder::SetFocused(bool focused) {
  // Note that all WM features that use this custom border currently have dark
  // mode as the default color mode.
  const SkColor new_color =
      focused ? AshColorProvider::Get()->GetControlsLayerColor(
                    AshColorProvider::ControlsLayerType::kFocusRingColor)
              : SK_ColorTRANSPARENT;
  if (new_color == color())
    return false;
  set_color(new_color);
  return true;
}

void WmHighlightItemBorder::Paint(const views::View& view,
                                  gfx::Canvas* canvas) {
  if (color() == SK_ColorTRANSPARENT)
    return;

  cc::PaintFlags flags;
  flags.setStrokeWidth(kBorderSize);
  flags.setColor(color());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  gfx::RectF bounds(view.GetLocalBounds());
  // The following inset is needed for the rounded corners of the border to
  // look correct. Otherwise, the borders will be painted at the edge of the
  // view, resulting in this border looking chopped.
  bounds.Inset(kBorderSize / 2);
  canvas->DrawRoundRect(bounds, corner_radius_, flags);
}

gfx::Insets WmHighlightItemBorder::GetInsets() const {
  return border_insets_;
}

gfx::Size WmHighlightItemBorder::GetMinimumSize() const {
  const int minmum_length = 2 * (kBorderSize + kBorderPadding);
  return gfx::Size(minmum_length, minmum_length);
}

}  // namespace ash
