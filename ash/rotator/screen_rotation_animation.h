// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATION_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATION_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class InterpolatedTransform;
class Layer;
}

namespace ash {

// A LayerAnimationElement that will animate a layer by rotating it around a
// pivot point.
class ASH_EXPORT ScreenRotationAnimation : public ui::LayerAnimationElement {
 public:
  // Creates an animation element that will rotate |layer| from |start_degrees|
  // to |end_degrees| around the given |pivot| and will take |duration| amount
  // of time.
  ScreenRotationAnimation(ui::Layer* layer,
                          int start_degrees,
                          int end_degrees,
                          float initial_opacity,
                          float target_opacity,
                          gfx::Point pivot,
                          base::TimeDelta duration,
                          gfx::Tween::Type twen_type);
  ~ScreenRotationAnimation() override;

 private:
  // Implementation of ui::LayerAnimationElement:
  void OnStart(ui::LayerAnimationDelegate* delegate) override;
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override;
  void OnGetTarget(TargetValue* target) const override;
  void OnAbort(ui::LayerAnimationDelegate* delegate) override;

  // The root InterpolatedTransform that defines the animation.
  std::unique_ptr<ui::InterpolatedTransform> interpolated_transform_;

  // The Tween type to use for the animation.
  gfx::Tween::Type tween_type_;

  // The initial layer opacity to start the animation with.
  float initial_opacity_;

  // The target layer opacity to end the animation with.
  float target_opacity_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimation);
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATION_H_
