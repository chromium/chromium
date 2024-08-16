// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/progress_indicator/progress_ring_pulse_animation.h"

#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"

namespace ash {
namespace {

// Animation -------------------------------------------------------------------

constexpr float kAnimationDurationInMs = 2331.f;

// Models a single key frame in the animation.
struct AnimationKeyFrame {
  float fraction = 0.f;
  float opacity = 0.f;
};

// The collection of all key frames in the animation.
constexpr AnimationKeyFrame kAnimationKeyFrames[] = {
    {.fraction = 0.f, .opacity = 1.f},                              // Start.
    {.fraction = 333.f / kAnimationDurationInMs, .opacity = 0.f},   // Fade out.
    {.fraction = 433.f / kAnimationDurationInMs, .opacity = 0.f},   // Hold.
    {.fraction = 766.f / kAnimationDurationInMs, .opacity = 1.f},   // Fade in.
    {.fraction = 999.f / kAnimationDurationInMs, .opacity = 1.f},   // Hold.
    {.fraction = 1332.f / kAnimationDurationInMs, .opacity = 0.f},  // Fade out.
    {.fraction = 1432.f / kAnimationDurationInMs, .opacity = 0.f},  // Hold.
    {.fraction = 1765.f / kAnimationDurationInMs, .opacity = 1.f},  // Fade in.
    {.fraction = 1998.f / kAnimationDurationInMs, .opacity = 1.f},  // Hold.
    {.fraction = 1.f, .opacity = 0.f}};                             // Fade out.

}  // namespace

// ProgressRingPulseAnimation --------------------------------------------------

ProgressRingPulseAnimation::ProgressRingPulseAnimation()
    : ProgressRingAnimation(Type::kPulse,
                            base::Milliseconds(kAnimationDurationInMs),
                            /*is_cyclic=*/false) {
#if DCHECK_IS_ON()
  constexpr size_t kAnimationKeyFramesCount = std::size(kAnimationKeyFrames);
  DCHECK_GE(kAnimationKeyFramesCount, 2u);
  for (size_t i = 0u; i < kAnimationKeyFramesCount; ++i) {
    if (i == 0u) {
      // The first animation key frame should be at `0.f`.
      DCHECK_EQ(kAnimationKeyFrames[i].fraction, 0.f);
    } else if (i == kAnimationKeyFramesCount - 1u) {
      // The last animation key frame should be at `1.f`.
      DCHECK_EQ(kAnimationKeyFrames[i].fraction, 1.f);
    } else {
      // Animation key frames should appear in sorted order.
      DCHECK_GT(kAnimationKeyFrames[i].fraction, 0.f);
      DCHECK_LT(kAnimationKeyFrames[i].fraction, 1.f);
      DCHECK_GT(kAnimationKeyFrames[i].fraction,
                kAnimationKeyFrames[i - 1u].fraction);
    }
  }
#endif  // DCHECK_IS_ON()
}

ProgressRingPulseAnimation::~ProgressRingPulseAnimation() = default;

void ProgressRingPulseAnimation::UpdateAnimatableProperties(
    double fraction,
    float* start_position,
    float* end_position,
    float* outer_ring_opacity) {
  *end_position = 1.f;

  // Loop over all animation key frames until the correct key frames for the
  // current animation `fraction` are found.
  for (size_t i = 1u; i < std::size(kAnimationKeyFrames); ++i) {
    if (fraction > kAnimationKeyFrames[i].fraction)
      continue;

    const AnimationKeyFrame& previous_key_frame = kAnimationKeyFrames[i - 1u];
    const AnimationKeyFrame& target_key_frame = kAnimationKeyFrames[i];

    // Update `fraction` so that it is still within the range [0, 1], but with
    // respect to the `previous_key_frame` and `target_key_frame`, instead of
    // with respect to the entire animation.
    fraction = (fraction - previous_key_frame.fraction) /
               (target_key_frame.fraction - previous_key_frame.fraction);

    // Interpolate `outer_ring_opacity` between the `previous_key_frame` and
    // `target_key_frame`.
    *outer_ring_opacity = gfx::Tween::FloatValueBetween(
        fraction, /*start_opacity=*/previous_key_frame.opacity,
        /*target_opacity=*/target_key_frame.opacity);
    return;
  }

  // This LOC should never be reached as the correct key frames for the current
  // animation `fraction` should have been found in the loop above.
  NOTREACHED();
}

}  // namespace ash
