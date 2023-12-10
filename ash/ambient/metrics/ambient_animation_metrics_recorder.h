// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_ANIMATION_METRICS_RECORDER_H_
#define ASH_AMBIENT_METRICS_AMBIENT_ANIMATION_METRICS_RECORDER_H_

#include <optional>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"

namespace ash {

// Records metrics specific to Lottie-animated themes (`kFeelTheBreeze` and
// `kFloatOnBy`). The recorder's lifetime should match that of a single ambient
// session.
class ASH_EXPORT AmbientAnimationMetricsRecorder
    : public lottie::AnimationObserver {
 public:
  // `current_ui_settings` must contain one of the Lottie-animated themes.
  explicit AmbientAnimationMetricsRecorder(
      AmbientUiSettings current_ui_settings);
  AmbientAnimationMetricsRecorder(const AmbientAnimationMetricsRecorder&) =
      delete;
  AmbientAnimationMetricsRecorder& operator=(
      const AmbientAnimationMetricsRecorder&) = delete;
  ~AmbientAnimationMetricsRecorder() override;

  // There should be one `RegisterAnimation()` call per screen in an ambient
  // session (each screen renders its own `animation`).
  void RegisterAnimation(lottie::Animation* animation);

 private:
  // Period at which we calculate the mean animation timestamp offset and
  // record it. All samples are cleared for the next recording.
  static constexpr base::TimeDelta kMeanTimestampOffsetFlushPeriod =
      base::Minutes(1);

  static bool IsPlaybackConfigValid(
      const std::optional<lottie::Animation::PlaybackConfig>& playback_config);

  // lottie::AnimationObserver implementation:
  void AnimationFramePainted(const lottie::Animation* animation,
                             float t) override;
  void AnimationIsDeleting(const lottie::Animation* animation) override;

  std::optional<base::TimeDelta> GetOffsetBetweenAnimations(
      const lottie::Animation& animation_l,
      const lottie::Animation& animation_r) const;

  const AmbientUiSettings ui_settings_;
  base::flat_set<const lottie::Animation*> registered_animations_;
  base::ScopedMultiSourceObservation<lottie::Animation,
                                     lottie::AnimationObserver>
      animation_observations_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_ANIMATION_METRICS_RECORDER_H_
