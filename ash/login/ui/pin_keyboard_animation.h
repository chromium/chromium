// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PIN_KEYBOARD_ANIMATION_H_
#define ASH_LOGIN_UI_PIN_KEYBOARD_ANIMATION_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/animation/tween.h"

namespace ui {
class InterpolatedTransform;
}  // namespace ui

namespace ash {

// A LayerAnimationElement that will animate a layer by scaling it from 0% to
// 100% (or visa-versa) on the y-axis.
class ASH_EXPORT PinKeyboardAnimation : public ui::LayerAnimationElement {
 public:
  // Creates an animation element that will grow or shrink the attached layer.
  // |height| is the current height of the layer. This is required for proper
  // centering.
  PinKeyboardAnimation(bool grow,
                       int height,
                       base::TimeDelta duration,
                       gfx::Tween::Type tween_type);

  PinKeyboardAnimation(const PinKeyboardAnimation&) = delete;
  PinKeyboardAnimation& operator=(const PinKeyboardAnimation&) = delete;

  ~PinKeyboardAnimation() override;

  // ui::LayerAnimationElement:
  void OnStart(ui::LayerAnimationDelegate* delegate) override;
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override;
  void OnGetTarget(TargetValue* target) const override;
  void OnAbort(ui::LayerAnimationDelegate* delegate) override;

 private:
  // Transform used to scale the target.
  std::unique_ptr<ui::InterpolatedTransform> transform_;

  gfx::Tween::Type tween_type_;
  float start_opacity_ = 0;
  float end_opacity_ = 1;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PIN_KEYBOARD_ANIMATION_H_
