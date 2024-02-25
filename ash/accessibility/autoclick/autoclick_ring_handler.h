// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_RING_HANDLER_H_
#define ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_RING_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

namespace ash {

// AutoclickRingHandler displays an animated affordance that is shown
// on autoclick gesture. The animation is a semi-transparent ring which
// fills with white.
class AutoclickRingHandler : public gfx::LinearAnimation {
 public:
  AutoclickRingHandler();

  AutoclickRingHandler(const AutoclickRingHandler&) = delete;
  AutoclickRingHandler& operator=(const AutoclickRingHandler&) = delete;

  ~AutoclickRingHandler() override;

  void StartGesture(base::TimeDelta duration,
                    const gfx::Point& center_point_in_screen,
                    views::Widget* widget);
  void StopGesture();
  void SetGestureCenter(const gfx::Point& center_point_in_screen,
                        views::Widget* widget);

  void SetSize(int radius);

 private:
  class AutoclickRingView;

  // The default values of the autoclick ring widget size.
  const int kAutoclickRingInnerRadius = 20;

  enum class AnimationType {
    kNone,
    kGrowAnimation,
  };

  void StartAnimation(base::TimeDelta duration);
  void StopAutoclickRing();

  // Overridden from gfx::LinearAnimation.
  void AnimateToState(double state) override;
  void AnimationStopped() override;

  raw_ptr<AutoclickRingView, DanglingUntriaged> view_ = nullptr;
  raw_ptr<views::Widget> ring_widget_ = nullptr;
  // Location of the simulated mouse event from auto click in screen
  // coordinates.
  gfx::Point tap_down_location_;
  AnimationType current_animation_type_ = AnimationType::kNone;
  base::TimeDelta animation_duration_;
  int radius_ = kAutoclickRingInnerRadius;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_RING_HANDLER_H_
