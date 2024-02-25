// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_ICON_ANIMATION_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_ICON_ANIMATION_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/progress_indicator/progress_indicator_animation.h"

namespace ash {

// An animation for a `ProgressIndicator`'s icon.
class ASH_EXPORT ProgressIconAnimation : public ProgressIndicatorAnimation {
 public:
  ProgressIconAnimation(const ProgressIconAnimation&) = delete;
  ProgressIconAnimation& operator=(const ProgressIconAnimation&) = delete;
  ~ProgressIconAnimation() override;

  // Returns an initialized progress icon animation.
  static std::unique_ptr<ProgressIconAnimation> Create();

  // Animatable properties.
  float inner_icon_translate_y_scale_factor() const {
    return inner_icon_translate_y_scale_factor_;
  }
  float inner_ring_stroke_width_scale_factor() const {
    return inner_ring_stroke_width_scale_factor_;
  }
  float opacity() const { return opacity_; }

 private:
  ProgressIconAnimation();

  // ProgressIndicatorAnimation:
  void UpdateAnimatableProperties(double fraction) override;

  // Animatable properties.
  float inner_icon_translate_y_scale_factor_ = -0.5f;
  float inner_ring_stroke_width_scale_factor_ = 0.f;
  float opacity_ = 0.f;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_ICON_ANIMATION_H_
