// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_DEPENDENT_POSITION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_DEPENDENT_POSITION_H_

#include "chrome/browser/ash/arc/input_overlay/actions/position.h"

namespace arc {
namespace input_overlay {
// Returns true if there is no value of |key| or there is positive value of the
// |key|.
bool ParsePositiveFraction(const base::Value& value,
                           const char* key,
                           absl::optional<float>* output);

// For dependent position, it can be height-dependent or width-dependent.
// If y_on_x is set, it is width-dependent.
// If x_on_y is set, it is height-dependent.
// If aspect_ratio is set, it is aspect_ratio dependent.
class DependentPosition : public Position {
 public:
  DependentPosition();
  DependentPosition(const DependentPosition&);
  DependentPosition& operator=(const DependentPosition&);
  ~DependentPosition() override;

  // Override from Position.
  //
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
  bool ParseFromJson(const base::Value& value) override;
  gfx::PointF CalculatePosition(
      const gfx::RectF& content_bounds) const override;

  absl::optional<float> x_on_y() const { return x_on_y_; }
  absl::optional<float> y_on_x() const { return y_on_x_; }
  absl::optional<float> aspect_ratio() const { return aspect_ratio_; }

 private:
  // This is for height-dependent position.
  // The length on the direction X to the anchor depends on the direction Y to
  // the anchor. If both x_on_y_ and y_on_x_ are not set, x_on_y_ is set to
  // default -1.
  absl::optional<float> x_on_y_;

  // This is for width-dependent position.
  // The length on the direction Y to the anchor depends on the direction X to
  // the anchor.
  absl::optional<float> y_on_x_;

  // The is for aspect-ratio-dependent position. Both x_on_y_ and y_on_x_ should
  // be set if aspect_ratio_ is set. If the window aspect ratio >=
  // aspect_ratio_, it becomes height-dependent position. Otherwise, it is
  // width-dependent position.
  absl::optional<float> aspect_ratio_;
};
}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_DEPENDENT_POSITION_H_
