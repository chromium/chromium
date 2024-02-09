// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_ANIMATION_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_ANIMATION_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/progress_indicator/progress_indicator_animation.h"

namespace ash {

// An animation for a `ProgressIndicator` to be painted in lieu of the
// determinate progress ring that would otherwise be painted.
class ASH_EXPORT ProgressRingAnimation : public ProgressIndicatorAnimation {
 public:
  enum class Type {
    kIndeterminate,  // See `ProgressRingIndeterminateAnimation`.
    kPulse,          // See `ProgressRingPulseAnimation`.
  };

  ProgressRingAnimation(const ProgressRingAnimation&) = delete;
  ProgressRingAnimation& operator=(const ProgressRingAnimation&) = delete;
  ~ProgressRingAnimation() override;

  // Returns an initialized progress ring animation of the specified `type`.
  static std::unique_ptr<ProgressRingAnimation> CreateOfType(Type type);

  // Returns the specific type of this animation.
  Type type() const { return type_; }

  // Returns animatable properties.
  float start_position() const { return start_position_; }
  float end_position() const { return end_position_; }
  float outer_ring_opacity() const { return outer_ring_opacity_; }

 protected:
  ProgressRingAnimation(Type type, base::TimeDelta duration, bool is_cyclic);

  // Implementing classes should update any desired animatable properties as
  // appropriate for the specified animation `fraction`.
  virtual void UpdateAnimatableProperties(double fraction,
                                          float* start_position,
                                          float* end_position,
                                          float* opacity) = 0;

 private:
  // ProgressIndicatorAnimation:
  void UpdateAnimatableProperties(double fraction) override;

  // The specific type of this animation.
  const Type type_;

  // Animatable properties.
  float start_position_ = 0.f;
  float end_position_ = 0.f;
  float outer_ring_opacity_ = 1.f;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_ANIMATION_H_
