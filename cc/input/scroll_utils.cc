// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_utils.h"

#include <algorithm>

#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// static
gfx::Vector2dF ScrollUtils::ResolveScrollPercentageToPixels(
    const gfx::Vector2dF& delta,
    const gfx::SizeF& scroller,
    const gfx::SizeF& viewport) {
  // Work with unsigned values and keep sign information in sign_x / sign_y.
  float sign_x = std::signbit(delta.x()) ? -1 : 1;
  float sign_y = std::signbit(delta.y()) ? -1 : 1;
  float delta_x = std::abs(delta.x());
  float delta_y = std::abs(delta.y());

  // Resolve and clamp horizontal scroll
  if (delta_x > 0)
    delta_x =
        std::max(1.0f, delta_x * std::min(scroller.width(), viewport.width()));

  // Resolve and clamps vertical scroll.
  if (delta_y > 0)
    delta_y = std::max(
        1.0f, delta_y * std::min(scroller.height(), viewport.height()));

  return gfx::Vector2dF(std::copysign(delta_x, sign_x),
                        std::copysign(delta_y, sign_y));
}

gfx::Vector2dF ScrollUtils::ResolvePixelScrollToPercentageForTesting(
    const gfx::Vector2dF& delta,
    const gfx::SizeF& scroller,
    const gfx::SizeF& viewport) {
  float delta_x = delta.x() / std::min(scroller.width(), viewport.width());
  float delta_y = delta.y() / std::min(scroller.height(), viewport.height());

  return gfx::Vector2dF(delta_x, delta_y);
}

}  // namespace cc
