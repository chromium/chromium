// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/rounded_window_targeter.h"

#include "base/check.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

RoundedWindowTargeter::RoundedWindowTargeter(int radius)
    : RoundedWindowTargeter(radius * 2, radius * 2, radius) {
  DCHECK_GT(radius, 0);
  DCHECK_EQ(rrectf_.GetType(), gfx::RRectF::Type::kSingle);
}

RoundedWindowTargeter::RoundedWindowTargeter(int width, int height, int radius)
    : rrectf_(0, 0, width, height, radius) {}

RoundedWindowTargeter::~RoundedWindowTargeter() = default;

bool RoundedWindowTargeter::EventLocationInsideBounds(
    aura::Window* target,
    const ui::LocatedEvent& event) const {
  gfx::Point point = ConvertEventLocationToWindowCoordinates(target, event);
  // Assumes a rectangle with height and width one is a point. This may match
  // 1px off at the bottom-right corner.
  // TODO(crbug.com/40773093): Expose SkRRect::ContainsPoint() instead.
  gfx::RectF rectf_point(point.x(), point.y(), 1, 1);
  return rrectf_.Contains(rectf_point);
}

}  // namespace ash
