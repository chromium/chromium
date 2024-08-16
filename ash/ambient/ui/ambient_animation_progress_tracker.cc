// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_progress_tracker.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"

namespace ash {

namespace {

void MoveAnimation(
    base::flat_set<raw_ptr<const lottie::Animation, CtnExperimental>>& from,
    base::flat_set<raw_ptr<const lottie::Animation, CtnExperimental>>& to,
    const lottie::Animation* animation) {
  if (to.contains(animation)) {
    CHECK(!from.contains(animation));
    return;
  }
  CHECK_EQ(from.erase(animation), 1u);
  to.insert(animation);
}

}  // namespace

AmbientAnimationProgressTracker::ImmutableParams::ImmutableParams() = default;

AmbientAnimationProgressTracker::ImmutableParams::ImmutableParams(
    const ImmutableParams& other) = default;

AmbientAnimationProgressTracker::ImmutableParams&
AmbientAnimationProgressTracker::ImmutableParams::operator=(
    const ImmutableParams& other) = default;

AmbientAnimationProgressTracker::ImmutableParams::~ImmutableParams() = default;

AmbientAnimationProgressTracker::AmbientAnimationProgressTracker() = default;

AmbientAnimationProgressTracker::~AmbientAnimationProgressTracker() = default;

void AmbientAnimationProgressTracker::RegisterAnimation(
    lottie::Animation* animation) {
  DCHECK(animation);
  if (animation_observations_.IsObservingSource(animation)) {
    return;
  }
  animation_observations_.AddObservation(animation);
  if (animation->GetPlaybackConfig()) {
    // The parameters verified here all concern "time" in the animation in some
    // form. They must match so that the "progress" returned by
    // GetGlobalProgress() has the same frame of reference across all
    // animations. Each animation's parameters are verified one time.
    VerifyAnimationImmutableParams(*animation);
    started_animations_.insert(animation);
  } else {
    DVLOG(4) << "Animation has not been Start()ed yet. Will be verified in "
                "AnimationWillStartPlaying() later.";
    inactive_animations_.insert(animation);
  }
}

bool AmbientAnimationProgressTracker::HasActiveAnimations() const {
  // Some of the started animations may not have painted a single frame yet. If
  // this is the case, GetCurrentProgress() will be null, so at least one of
  // them must return a non-null value for GetGlobalProgress() to return a valid
  // value.
  for (const lottie::Animation* animation : started_animations_) {
    if (animation->GetCurrentProgress())
      return true;
  }
  return false;
}

AmbientAnimationProgressTracker::Progress
AmbientAnimationProgressTracker::GetGlobalProgress() const {
  // Currently, the method for picking one "global" progress is trivial. It just
  // picks an arbitrary animation in the group because in practice, their
  // timestamps should not have diverged by an amount that is user-perceptible
  // (ex: less than 100 ms). If it's needed in the future, we can do something
  // more complex here like taking the median or mean progress of all
  // animations. But the complexity is currently not justified.
  for (const lottie::Animation* animation : started_animations_) {
    if (animation->GetCurrentProgress()) {
      DCHECK(animation->GetNumCompletedCycles());
      return {*animation->GetNumCompletedCycles(),
              *animation->GetCurrentProgress()};
    }
  }
  NOTREACHED() << "HasActiveAnimations() must be true before calling "
                  "GetGlobalProgress()";
}

AmbientAnimationProgressTracker::ImmutableParams
AmbientAnimationProgressTracker::GetImmutableParams() const {
  DCHECK(HasActiveAnimations());
  // The animation picked here is arbitrary since they all should have the same
  // immutable params.
  const lottie::Animation* animation = *started_animations_.begin();
  auto playback_config = animation->GetPlaybackConfig();
  ImmutableParams params;
  params.total_duration = animation->GetAnimationDuration();
  params.scheduled_cycles = playback_config->scheduled_cycles;
  params.style = playback_config->style;
  return params;
}

void AmbientAnimationProgressTracker::AnimationWillStartPlaying(
    const lottie::Animation* animation) {
  DCHECK(animation_observations_.IsObservingSource(
      const_cast<lottie::Animation*>(animation)));
  VerifyAnimationImmutableParams(*animation);
  MoveAnimation(/*from=*/inactive_animations_, /*to=*/started_animations_,
                animation);
}

void AmbientAnimationProgressTracker::AnimationStopped(
    const lottie::Animation* animation) {
  CHECK(animation_observations_.IsObservingSource(
      const_cast<lottie::Animation*>(animation)));
  MoveAnimation(/*from=*/started_animations_, /*to=*/inactive_animations_,
                animation);
}

void AmbientAnimationProgressTracker::AnimationIsDeleting(
    const lottie::Animation* animation) {
  DCHECK(animation_observations_.IsObservingSource(
      const_cast<lottie::Animation*>(animation)));
  animation_observations_.RemoveObservation(
      const_cast<lottie::Animation*>(animation));
  started_animations_.erase(animation);
  inactive_animations_.erase(animation);
}

void AmbientAnimationProgressTracker::VerifyAnimationImmutableParams(
    const lottie::Animation& animation) const {
  DCHECK(!started_animations_.contains(&animation));
  if (started_animations_.empty()) {
    DVLOG(4) << "Incoming animation is the first started in the session. No "
                "need to verify against other animations";
    return;
  }
  // The animation picked here is arbitrary since all existing animations should
  // have gone through this method, verifying that all of their immutable params
  // match.
  const lottie::Animation* existing_animation = *started_animations_.begin();
  DCHECK_EQ(animation.GetAnimationDuration(),
            existing_animation->GetAnimationDuration());
  auto incoming_playback_config = animation.GetPlaybackConfig();
  auto existing_playback_config = animation.GetPlaybackConfig();
  DCHECK(incoming_playback_config);
  DCHECK(existing_playback_config);
  DCHECK_EQ(incoming_playback_config->scheduled_cycles.size(),
            existing_playback_config->scheduled_cycles.size());
  for (size_t i = 0; i < incoming_playback_config->scheduled_cycles.size();
       ++i) {
    DCHECK_EQ(incoming_playback_config->scheduled_cycles[i].start_offset,
              existing_playback_config->scheduled_cycles[i].start_offset);
    DCHECK_EQ(incoming_playback_config->scheduled_cycles[i].end_offset,
              existing_playback_config->scheduled_cycles[i].end_offset);
  }
  DCHECK_EQ(incoming_playback_config->style, existing_playback_config->style);
}

}  // namespace ash
