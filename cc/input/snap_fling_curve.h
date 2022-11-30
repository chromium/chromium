// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SNAP_FLING_CURVE_H_
#define CC_INPUT_SNAP_FLING_CURVE_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// The curve for the snap fling animation. The curve would generate a geometric
// sequence of deltas to be scrolled at each frame.
class CC_EXPORT SnapFlingCurve {
 public:
  // Creates the curve based on the start offset, target offset, and the first
  // inertial GSU's time_stamp.
  SnapFlingCurve(const gfx::PointF& start_offset,
                 const gfx::PointF& target_offset,
                 base::TimeTicks first_gsu_time);

  virtual ~SnapFlingCurve();

  // Estimate the total distance that will be scrolled given the first GSU's
  // delta
  static gfx::Vector2dF EstimateDisplacement(const gfx::Vector2dF& first_delta);

  // Returns the delta that should be scrolled at |time|.
  virtual gfx::Vector2dF GetScrollDelta(base::TimeTicks time);

  // Updates |current_displacement_|. This sync is necessary because the node
  // might be scrolled by other calls and the scrolls might be clamped.
  void UpdateCurrentOffset(const gfx::PointF& current_offset);

  // Returns true if the scroll has arrived at the snap destination.
  virtual bool IsFinished() const;

  base::TimeDelta duration() const { return duration_; }

 private:
  // Returns the curve's current distance at |current_time|.
  double GetCurrentCurveDistance(base::TimeDelta current_time);

  // The initial scroll offset of the scroller.
  const gfx::PointF start_offset_;

  // The total displacement to the snap position.
  const gfx::Vector2dF total_displacement_;
  // 1D representation of |total_displacement_|.
  const double total_distance_;

  // The current displacement that has been scrolled.
  gfx::Vector2dF current_displacement_;

  // The timestamp of the first GSU.
  const base::TimeTicks start_time_;

  // The number of deltas in the curve's geometric sequence.
  const double total_frames_;
  // The first delta that defines the curve's geometric sequence.
  const double first_delta_;
  // The total milliseconds needed to finish the curve.
  const base::TimeDelta duration_;

  bool is_finished_ = false;

  // |total_displacement_.x| / |total_distance_|
  double ratio_x_;
  // |total_displacement_.y| / |total_distance_|
  double ratio_y_;
};

}  // namespace cc

#endif  // CC_INPUT_SNAP_FLING_CURVE_H_
