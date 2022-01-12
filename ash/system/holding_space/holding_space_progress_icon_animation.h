// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_ICON_ANIMATION_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_ICON_ANIMATION_H_

#include "ash/system/holding_space/holding_space_progress_indicator_animation.h"

namespace ash {

// An animation for a `HoldingSpaceProgressIndicator`'s icon.
class HoldingSpaceProgressIconAnimation
    : public HoldingSpaceProgressIndicatorAnimation {
 public:
  HoldingSpaceProgressIconAnimation();
  HoldingSpaceProgressIconAnimation(const HoldingSpaceProgressIconAnimation&) =
      delete;
  HoldingSpaceProgressIconAnimation& operator=(
      const HoldingSpaceProgressIconAnimation&) = delete;
  ~HoldingSpaceProgressIconAnimation() override;

  // Animatable properties.
  float inner_icon_translate_y_scale_factor() const {
    return inner_icon_translate_y_scale_factor_;
  }
  float inner_ring_stroke_width_scale_factor() const {
    return inner_ring_stroke_width_scale_factor_;
  }

 private:
  // HoldingSpaceProgressIndicatorAnimation:
  void UpdateAnimatableProperties(double fraction) override;

  // Animatable properties.
  float inner_icon_translate_y_scale_factor_ = 0.f;
  float inner_ring_stroke_width_scale_factor_ = 1.f;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_ICON_ANIMATION_H_
