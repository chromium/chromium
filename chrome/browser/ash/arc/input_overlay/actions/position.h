// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace arc::input_overlay {

// Returns true if there is no value of `key` or there is positive value of the
// `key`.
bool ParsePositiveFraction(const base::Value::Dict& value,
                           const char* key,
                           std::optional<float>* output);

// Position class for touch point.
class Position {
 public:
  explicit Position(PositionType type);
  Position(const Position&);
  Position& operator=(const Position&);
  ~Position();

  static std::unique_ptr<Position> ConvertFromProto(const PositionProto& proto);

  bool ParseFromJson(const base::Value::Dict& value);
  // Return the position coords in `content_bounds`. `content_bounds` is bounds
  // excluding caption if the caption shows.
  gfx::PointF CalculatePosition(const gfx::RectF& content_bounds) const;
  // Normalize the `point` inside of `content_bounds`. `point` is relative
  // position inside of `content_bounds`.
  void Normalize(const gfx::Point& point, const gfx::RectF& content_bounds);
  std::unique_ptr<PositionProto> ConvertToProto() const;

  bool operator==(const Position& other) const;
  bool operator!=(const Position& other) const;

  PositionType position_type() const { return position_type_; }
  const gfx::PointF& anchor() const { return anchor_; }
  const gfx::Vector2dF& anchor_to_target() const { return anchor_to_target_; }
  void set_anchor_to_target(const gfx::Vector2dF& points_vector) {
    anchor_to_target_ = points_vector;
  }

  std::optional<float> x_on_y() const { return x_on_y_; }
  std::optional<float> y_on_x() const { return y_on_x_; }
  std::optional<float> aspect_ratio() const { return aspect_ratio_; }

 private:
  friend class PositionTest;

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
  bool ParseDefaultFromJson(const base::Value::Dict& value);
  // Parse position from Json.
  // Json value format:
  // {
  //   "type": "dependent-position",
  //   "anchor": [, ],
  //   "anchor_to_target": [, ],
  //   "x_on_y": 1.5, // Can be null for width-dependent position.
  //   "y_on_x": 2 // Can be null for height-dependent position.
  //   "aspect_ratio": 1.5 // Can be null for height- or width-dependent
  //                       // position.
  // }
  bool ParseDependentFromJson(const base::Value::Dict& value);

  gfx::PointF CalculateDefaultPosition(const gfx::RectF& content_bounds) const;
  gfx::PointF CalculateDependentPosition(
      const gfx::RectF& content_bounds) const;

  // kDefault: only `anchor_`, `anchor_to_target_`, `max_x_` and `max_y_` are
  // used for calculating position. kDependent: including above kDefault,
  // `x_on_y_`, `y_on_x_` and `aspect_ratio_` are also involved for calculating
  // position.
  PositionType position_type_;

  // Default anchor_ is (0, 0). Anchor is the point position where the UI
  // position is relative to. For example, a UI may be always relative to the
  // left-bottom.
  gfx::PointF anchor_;
  // The anchor_to_target_ is the vector from anchor to target UI
  // position. The value may be negative if the direction is different from
  // original x and y.
  gfx::Vector2dF anchor_to_target_;
  std::optional<int> max_x_;
  std::optional<int> max_y_;

  // Below is for dependent position.
  // This is for height-dependent position.
  // The length on the direction X to the anchor depends on the direction Y to
  // the anchor. If both x_on_y_ and y_on_x_ are not set, x_on_y_ is set to
  // default -1.
  std::optional<float> x_on_y_;

  // This is for width-dependent position.
  // The length on the direction Y to the anchor depends on the direction X to
  // the anchor.
  std::optional<float> y_on_x_;

  // The is for aspect-ratio-dependent position. Both x_on_y_ and y_on_x_ should
  // be set if aspect_ratio_ is set. If the window aspect ratio >=
  // aspect_ratio_, it becomes height-dependent position. Otherwise, it is
  // width-dependent position.
  std::optional<float> aspect_ratio_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
