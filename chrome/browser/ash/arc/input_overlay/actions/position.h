// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_

#include "base/values.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace arc {
namespace input_overlay {
// This is the base position for location. It includes anchor point
// and the vector from the anchor point to the target position.
class Position {
 public:
  Position();
  Position(const Position&);
  Position& operator=(const Position&);
  virtual ~Position();

  // Json value format:
  // {
  //   "type": "position",
  //   "anchor": [ // Optional.
  //     0,
  //     0
  //   ],
  //   "anchor_to_target": [
  //     0.1796875,
  //     0.25
  //   ],
  // "max_x": 50, // Optional.
  // "max_y": 50 // Optional.
  // }
  virtual bool ParseFromJson(const base::Value& value);
  // Return the position coords in |content_bounds| which excludes the caption
  // if the caption shows.
  virtual gfx::PointF CalculatePosition(const gfx::RectF& content_bounds) const;

  const gfx::PointF& anchor() const { return anchor_; }
  const gfx::Vector2dF& anchor_to_target() const { return anchor_to_target_; }

 private:
  friend class PositionTest;

  // Default anchor_ is (0, 0). Anchor is the point position where the UI
  // position is relative to. For example, a UI may be always relative to the
  // left-bottom.
  gfx::PointF anchor_;
  // The anchor_to_target_ is the vector from anchor to target UI
  // position. The value may be negative if the direction is different from
  // original x and y.
  gfx::Vector2dF anchor_to_target_;
  absl::optional<int> max_x_;
  absl::optional<int> max_y_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
