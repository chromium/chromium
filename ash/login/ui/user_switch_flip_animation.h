// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_USER_SWITCH_FLIP_ANIMATION_H_
#define ASH_LOGIN_UI_USER_SWITCH_FLIP_ANIMATION_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/animation/tween.h"

namespace ui {
class InterpolatedTransform;
}  // namespace ui

namespace ash {

// A LayerAnimationElement that will animate a layer by rotating it around the
// y-axis.
class ASH_EXPORT UserSwitchFlipAnimation : public ui::LayerAnimationElement {
 public:
  // Creates an animation element that will rotate from |start_degrees| to
  // |midpoint_degrees| to |end_degrees| around the y axis, taking |duration|
  // amount of time, animating using |tween_type|.
  UserSwitchFlipAnimation(int width,
                          int start_degrees,
                          int midpoint_degrees,
                          int end_degrees,
                          base::TimeDelta duration,
                          gfx::Tween::Type tween_type,
                          base::OnceClosure on_midpoint);

  UserSwitchFlipAnimation(const UserSwitchFlipAnimation&) = delete;
  UserSwitchFlipAnimation& operator=(const UserSwitchFlipAnimation&) = delete;

  ~UserSwitchFlipAnimation() override;

  // ui::LayerAnimationElement:
  void OnStart(ui::LayerAnimationDelegate* delegate) override;
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override;
  void OnGetTarget(TargetValue* target) const override;
  void OnAbort(ui::LayerAnimationDelegate* delegate) override;

 private:
  // The root InterpolatedTransform that defines the animation.
  std::unique_ptr<ui::InterpolatedTransform> first_half_transform_;
  std::unique_ptr<ui::InterpolatedTransform> second_half_transform_;

  // The tween type to use for the animation.
  gfx::Tween::Type tween_type_;

  // Called when the animation is 50% complete.
  base::OnceClosure on_midpoint_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_USER_SWITCH_FLIP_ANIMATION_H_
