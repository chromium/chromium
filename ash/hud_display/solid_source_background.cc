// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/solid_source_background.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

SolidSourceBackground::SolidSourceBackground(SkColor color,
                                             SkScalar top_rounding_radius)
    : top_rounding_radius_(top_rounding_radius) {
  SetNativeControlColor(color);
}

void SolidSourceBackground::Paint(gfx::Canvas* canvas,
                                  views::View* view) const {
  if (top_rounding_radius_ == 0) {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(get_color(), SkBlendMode::kSrc);
  } else {
    const SkScalar circle_size = top_rounding_radius_ * 2;
    const SkScalar right_edge = view->width();
    const SkScalar bottom_edge = view->height();

    SkPath path;
    path.moveTo(0, bottom_edge);
    // |false| will draw straight line to the start of the arc.
    path.arcTo({0, 0, circle_size, circle_size}, -180, 90, false);
    path.arcTo({right_edge - circle_size, 0, right_edge, circle_size}, -90, 90,
               false);
    path.lineTo(right_edge, bottom_edge);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawPath(path, flags);
  }
}

}  // namespace hud_display
}  // namespace ash
