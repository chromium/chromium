// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_ring_indeterminate_animation.h"

#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/paint_throbber.h"

namespace ash {
namespace {

// This animation is cyclic and uses `start_time()` rather than the animation
// fraction to update animatable properties. That being the case, this value
// doesn't really matter but is chosen to be on the order of minutes to minimize
// overhead that may occur during cyclic animation restart.
constexpr base::TimeDelta kAnimationDuration = base::Minutes(1);

// Helpers ---------------------------------------------------------------------

// Converts the specified `angle_in_degrees` to a position in the range [0, 1].
float ConvertFromAngleToPosition(float angle_in_degrees) {
  DCHECK_GE(angle_in_degrees, 0.f);
  float position = angle_in_degrees / 360.f;
  if (position >= 1.f) {
    // NOTE: The `position` should be wrapped around the interval [0, 1], not be
    // clamped. This is accomplished by removing the integral component.
    position -= static_cast<int>(position);
  }
  return position;
}

}  // namespace

// ProgressRingIndeterminateAnimation ------------------------------------------

ProgressRingIndeterminateAnimation::ProgressRingIndeterminateAnimation()
    : ProgressRingAnimation(Type::kIndeterminate,
                            kAnimationDuration,
                            /*is_cyclic=*/true) {}

ProgressRingIndeterminateAnimation::~ProgressRingIndeterminateAnimation() =
    default;

void ProgressRingIndeterminateAnimation::UpdateAnimatableProperties(
    double fraction,
    float* start_position,
    float* end_position,
    float* outer_ring_opacity) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time();

  // Since `elapsed_time` is used rather than the animation `fraction`, it is
  // necessary to manually account for animation duration scaling.
  const float duration_multiplier =
      ui::ScopedAnimationDurationScaleMode::duration_multiplier();
  if (duration_multiplier != 0.f)
    elapsed_time /= duration_multiplier;

  const gfx::ThrobberSpinningState state =
      gfx::CalculateThrobberSpinningState(elapsed_time);

  // The spinning throbber offsets its `start_angle` by `270.f` degrees to
  // account for the fact that `SkPath` treats zero degrees as being aligned
  // with the positive x-axis. Undo that offset.
  float start_angle = state.start_angle - 270.f;
  *start_position = ConvertFromAngleToPosition(start_angle);

  // Add `360.f` degrees to ensure that `end_angle` is positive given that
  // `sweep_angle` may be negative if the spinning throbber is sweeping counter
  // clockwise. Adding `360.f` degrees does not change the calculated position
  // since it will be wrapped around the interval [0, 1].
  float end_angle = start_angle + 360.f + state.sweep_angle;
  *end_position = ConvertFromAngleToPosition(end_angle);

  // If `sweep_angle` is negative, the spinning throbber is sweeping counter
  // clockwise. In this case, swap `start_position` and `end_position`.
  if (state.sweep_angle < 0.f)
    std::swap(*start_position, *end_position);
}

}  // namespace ash
