// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
#define ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"

namespace ash {

// AmbientSessionMetricsRecorder's lifetime is meant to match that of a
// single ambient session:
// * Construction     - Ambient session starts by preparing any assets needed
//                      for rendering.
// * RegisterScreen() - Ambient session is rendering. There is one call for each
//                      screen (display).
// * Destruction      - Ambient session ends.
class ASH_EXPORT AmbientSessionMetricsRecorder
    : public lottie::AnimationObserver {
 public:
  // A custom `tick_clock` may be provided for testing purposes.
  explicit AmbientSessionMetricsRecorder(
      AmbientUiSettings ui_settings,
      const base::TickClock* tick_clock = nullptr);
  AmbientSessionMetricsRecorder(const AmbientSessionMetricsRecorder&) = delete;
  AmbientSessionMetricsRecorder& operator=(
      const AmbientSessionMetricsRecorder&) = delete;
  ~AmbientSessionMetricsRecorder() override;

  // Registers a screen and its corresponding |animation|. |animation| may be
  // null if the ambient UI does not have an associated animation (ex: slideshow
  // mode). AmbientSessionMetricsRecorder may outlive the incoming
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

  const AmbientUiSettings ui_settings_;
  const raw_ptr<const base::TickClock> clock_;
  const base::TimeTicks session_start_time_;

  int num_registered_screens_ = 0;
  base::flat_set<const lottie::Animation*> registered_animations_;
  base::ScopedMultiSourceObservation<lottie::Animation,
                                     lottie::AnimationObserver>
      animation_observations_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
