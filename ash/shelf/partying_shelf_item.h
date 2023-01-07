// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_PARTYING_SHELF_ITEM_H_
#define ASH_SHELF_PARTYING_SHELF_ITEM_H_

#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point_f.h"

namespace aura {
class Window;
}

namespace gfx {
class Animation;
class ImageSkia;
class Transform;
class Vector2dF;
}  // namespace gfx

namespace views {
class Widget;
}

namespace ash {

// A shelf item that moves in the work area and bounces off the perimeter with
// visually appealing squash and stretch. The computations use "mockup frames"
// as a unit of time, referring to a 24fps mockup. The animation is as follows:
// -----------------------------------------------------------------------------
//  0-5                 |The item crashes into `bounce_side_`.
//  5-11                |The item springs away from `bounce_side_`.
// 11-`mockup_duration_`|The item travels from `bounce_side_` to `target_side_`.
// -----------------------------------------------------------------------------
// When it ends, it restarts with `bounce_side_` set to the old `target_side_`.
class PartyingShelfItem : public gfx::LinearAnimation,
                          public gfx::AnimationDelegate {
 public:
  PartyingShelfItem(aura::Window* root_window,
                    const gfx::ImageSkia& image_skia,
                    int icon_size);
  PartyingShelfItem(const PartyingShelfItem&) = delete;
  PartyingShelfItem& operator=(const PartyingShelfItem&) = delete;
  ~PartyingShelfItem() override;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  // Indicates a side of the work area.
  enum Side { kTop, kBottom, kLeft, kRight };

  // Starts the animation.
  void Go();

  // Starts the animation without squash and stretch. The item just travels in a
  // straight line.
  void JustGoStraight();

  // Updates the transform associated with `widget_`.
  void UpdateTransform(float mockup_frame);

  // Computes the transform for the squash and stretch, and the travel from
  // `bounce_side_` to `target_side_`.
  // TODO(https://crbug.com/1262374): This function is called from
  // `AnimateToState` and it does a lot of computations that are independent of
  // `mockup_frame`. Avoid redoing computations unnecessarily. This may improve
  // performance significantly.
  gfx::Transform ComputeTransform(float mockup_frame) const;

  // Used by `ComputeTransform`.
  gfx::Transform ComputeTransformAboutOrigin(float mockup_frame) const;

  // Computes the centerpoint of the item, in its own coordinate system.
  gfx::PointF ComputeCenterPointInIcon() const;

  // Computes the centerpoint of the item, in the coordinate system of
  // `widget_`'s parent.
  gfx::PointF ComputeCenterPointInParent(float mockup_frame) const;

  // Computes the speed of the item's travel, in DIPs per mockup frame.
  float ComputeTravelSpeed() const;

  // Computes a unit vector in the direction of the item's travel.
  gfx::Vector2dF ComputeTravelDirection() const;

  // Returns a random entry point for the item, represented as a distance
  // clockwise from the origin along the work area perimeter. The entry point is
  // kept away from the shelf unless autohide is enabled.
  float RandDistanceClockwiseFromOriginToEntryPoint() const;

  // The item representation that is animated by `PartyingShelfItem`. Separate
  // from representations outside of `PartyingShelfItem` for items not partying.
  // The bounds are always at the top left of the parent, and the animation only
  // sets the transform, because if you change the bounds then you incur the
  // performance penalty of contents view relayout and repaint.
  views::Widget* widget_;
  // The item is square, with width and height both equal to `icon_size_`.
  const float icon_size_;
  // The animation duration in mockup frames, depending on how far the item must
  // travel to get from `bounce_side_` to `target_side_`. This is generally not
  // an integer, because it represents the exact time when the item reaches
  // `target_side_`, usually in between frames of the mockup.
  float mockup_length_;
  // The animation speed relative to the mockup. For example, if `speed_` is 2,
  // then the animation is twice as fast as the mockup.
  const float speed_;
  // The centerpoint of the item at mockup frame 0, in the coordinate system of
  // `widget_`'s parent.
  gfx::PointF center_point_at_0_in_parent_;
  // The side of the work area where the item bounces at the beginning.
  Side bounce_side_;
  // The side of the work area where the item will bounce next.
  Side target_side_;
  // True if the velocity component tangential to `bounce_side_` is negative.
  // Meaningless when `travel_angle_in_degrees_` is 0.
  bool leftward_or_upward_;
  // The angle, in degrees, between the direction of the item's travel and the
  // direction from `bounce_side_` straight into the work area.
  float travel_angle_in_degrees_;
};

}  // namespace ash

#endif  // ASH_SHELF_PARTYING_SHELF_ITEM_H_
