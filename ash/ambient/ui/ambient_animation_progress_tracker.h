// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_PROGRESS_TRACKER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_PROGRESS_TRACKER_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"

namespace ash {

// Observes the progress of the N active lottie animations rendering during
// ambient mode and condenses all of their progress into one global number
// representative of the current progress of all N animations. The method for
// condensing the N timestamps into one is an internal implementation detail.
//
// AmbientAnimationProgressTracker is meant to be a long-lived class whose
// lifetime can span multiple ambient mode sessions if desired.
class ASH_EXPORT AmbientAnimationProgressTracker
    : public lottie::AnimationObserver {
 public:
  struct Progress {
    // The number of cycles the animation has completed thus far.
    int num_completed_cycles = 0;

    // The normalized timestamp of the most recent frame that the animation
    // has painted within the animation's current cycle.
    float current_timestamp = 0.f;
  };

  // Parameters for the currently active ambient session that do not change for
  // the duration of the session. All animations in the session are guaranteed
  // to share the same values for these parameters. A fatal error is raised if a
  // discrepancy is detected.
  struct ImmutableParams {
    ImmutableParams();
    ImmutableParams(const ImmutableParams& other);
    ImmutableParams& operator=(const ImmutableParams& other);
    ~ImmutableParams();

    // The total duration of the animation file.
    base::TimeDelta total_duration;

    // A subset of lottie::Animation::PlaybackConfig that should be the same for
    // all N animations.
    std::vector<lottie::Animation::CycleBoundaries> scheduled_cycles;
    lottie::Animation::Style style = lottie::Animation::Style::kLoop;
  };

  AmbientAnimationProgressTracker();
  AmbientAnimationProgressTracker(const AmbientAnimationProgressTracker&) =
      delete;
  AmbientAnimationProgressTracker& operator=(
      const AmbientAnimationProgressTracker&) = delete;
  ~AmbientAnimationProgressTracker() override;

  // Registers an |animation| to track. The |animation| may be destroyed either
  // before or after AmbientAnimationProgressTracker. The tracker automatically
  // stops tracking the |animation| when the animation is destroyed.
  void RegisterAnimation(lottie::Animation* animation);

  // Whether the tracker has detected at least one animation in the current
  // ambient session.
  bool HasActiveAnimations() const;

  // HasActiveAnimations() must be true before calling the methods below:
  //
  // The condensed progress of all active animations.
  Progress GetGlobalProgress() const;
  ImmutableParams GetImmutableParams() const;

 private:
  // lottie::AnimationObserver implementation:
  void AnimationWillStartPlaying(const lottie::Animation* animation) override;
  void AnimationStopped(const lottie::Animation* animation) override;
  void AnimationIsDeleting(const lottie::Animation* animation) override;

  void VerifyAnimationImmutableParams(const lottie::Animation& animation) const;

  base::ScopedMultiSourceObservation<lottie::Animation,
                                     lottie::AnimationObserver>
      animation_observations_{this};
  // Registered animations that have been Start()ed.
  base::flat_set<raw_ptr<const lottie::Animation, CtnExperimental>>
      started_animations_;
  // Registered animations that have not been Start()ed yet.
  base::flat_set<raw_ptr<const lottie::Animation, CtnExperimental>>
      inactive_animations_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_PROGRESS_TRACKER_H_
