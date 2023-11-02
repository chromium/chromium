// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_MULTI_SCREEN_METRICS_RECORDER_H_
#define ASH_AMBIENT_METRICS_AMBIENT_MULTI_SCREEN_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/constants/ambient_animation_theme.h"
#include "base/containers/flat_set.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"

namespace ash {

// Records metrics for multi-screen usage in ambient mode:
// * The number of screens active during ambient mode.
// * The average instantaneous offset in timestamp between the animations
//   playing on each screen. This metric is not recorded for single-screen
//   ambient mode sessions.
// AmbientMultiScreenMetricsRecorder's lifetime is meant to match that of a
// single ambient mode session. Per-session metrics are recorded in its
// destructor.
class ASH_EXPORT AmbientMultiScreenMetricsRecorder
    : public lottie::AnimationObserver {
 public:
  explicit AmbientMultiScreenMetricsRecorder(AmbientAnimationTheme theme);
  AmbientMultiScreenMetricsRecorder(const AmbientMultiScreenMetricsRecorder&) =
      delete;
  AmbientMultiScreenMetricsRecorder& operator=(
      const AmbientMultiScreenMetricsRecorder&) = delete;
  ~AmbientMultiScreenMetricsRecorder() override;

  // Registers a screen and its corresponding |animation|. |animation| may be
  // null if the ambient UI does not have an associated animation (ex: slideshow
  // mode). AmbientMultiScreenMetricsRecorder may outlive the incoming
  // |animation| if desired.
  void RegisterScreen(lottie::Animation* animation);

 private:
  // Period at which we calculate the mean animation timestamp offset and
  // record it. All samples are cleared for the next recording.
  static constexpr base::TimeDelta kMeanTimestampOffsetFlushPeriod =
      base::Minutes(1);

  static bool IsPlaybackConfigValid(
      const absl::optional<lottie::Animation::PlaybackConfig>& playback_config);

  // lottie::AnimationObserver implementation:
  void AnimationFramePainted(const lottie::Animation* animation,
                             float t) override;
  void AnimationIsDeleting(const lottie::Animation* animation) override;

  absl::optional<base::TimeDelta> GetOffsetBetweenAnimations(
      const lottie::Animation& animation_l,
      const lottie::Animation& animation_r) const;

  const AmbientAnimationTheme theme_;

  int num_registered_screens_ = 0;
  base::flat_set<const lottie::Animation*> registered_animations_;
  base::ScopedMultiSourceObservation<lottie::Animation,
                                     lottie::AnimationObserver>
      animation_observations_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_MULTI_SCREEN_METRICS_RECORDER_H_
