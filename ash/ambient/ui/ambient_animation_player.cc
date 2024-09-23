// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_player.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/ambient/ui/ambient_animation_progress_tracker.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {

namespace {

std::optional<base::TimeDelta> FindCycleRestartTimestamp(
    const cc::SkottieWrapper& skottie) {
  static const base::NoDestructor<std::string> kRestartMarkerName(
      base::StrCat({kLottieCustomizableIdPrefix, "_Marker_CycleRestart"}));
  DCHECK(skottie.is_valid());
  std::optional<base::TimeDelta> restart_timestamp;
  for (const cc::SkottieMarker& marker : skottie.GetAllMarkers()) {
    if (marker.name != *kRestartMarkerName) {
      continue;
    } else if (restart_timestamp.has_value()) {
      LOG(DFATAL) << "Multiple markers with name " << *kRestartMarkerName
                  << " found in animation file. Defaulting to first one.";
    } else {
      // |marker.begin_time| is a normalized timestamp in range [0, 1), where 1
      // is the animation's cycle duration.
      restart_timestamp.emplace(base::Seconds(skottie.duration()) *
                                marker.begin_time);
      DVLOG(1) << "Found restart marker at timestamp " << *restart_timestamp;
    }
  }
  return restart_timestamp;
}

}  // namespace

AmbientAnimationPlayer::AmbientAnimationPlayer(
    views::AnimatedImageView* animated_image_view,
    AmbientAnimationProgressTracker* progress_tracker)
    : animated_image_view_(animated_image_view),
      progress_tracker_(progress_tracker) {
  DCHECK(animated_image_view_);
  lottie::Animation* animation = animated_image_view_->animated_image();
  DCHECK(animation);
  DCHECK(progress_tracker_);
  lottie::Animation::PlaybackConfig playback_config;
  // If there are existing animations in this ambient mode session, start
  // playing from the existing animations' timestamps. This gives rough
  // synchronization across animations. The animations' timestamps should not
  // diverge over time because AnimatedImageView makes time steps that
  // ultimately reflect real time (i.e. it does not step by a fixed amount each
  // frame). Thus, there may be transient periods where the animations diverge
  // due to random system instability, but they should all converge again
  // eventually.
  if (progress_tracker_->HasActiveAnimations()) {
    AmbientAnimationProgressTracker::ImmutableParams immutable_params =
        progress_tracker_->GetImmutableParams();
    AmbientAnimationProgressTracker::Progress global_progress =
        progress_tracker_->GetGlobalProgress();
    playback_config = {
        immutable_params.scheduled_cycles,
        global_progress.current_timestamp * immutable_params.total_duration,
        global_progress.num_completed_cycles, immutable_params.style};
  } else {
    std::optional<base::TimeDelta> cycle_restart_timestamp_found =
        FindCycleRestartTimestamp(*animation->skottie());
    if (cycle_restart_timestamp_found.has_value()) {
      cycle_restart_timestamp_ = *cycle_restart_timestamp_found;
      if (cycle_restart_timestamp_ >= animation->GetAnimationDuration()) {
        LOG(DFATAL) << "Animation has invalid cycle restart timestamp "
                    << cycle_restart_timestamp_ << ". Total cycle duration "
                    << animation->GetAnimationDuration();
      }
    } else {
      DVLOG(1) << "Restart marker not found in animation. Defaulting to cycle "
                  "restart at timestamp 0";
      DCHECK(cycle_restart_timestamp_.is_zero());
    }
    playback_config = lottie::Animation::PlaybackConfig(
        {{base::TimeDelta(), animation->GetAnimationDuration()},
         {cycle_restart_timestamp_, animation->GetAnimationDuration()}},
        /*initial_offset=*/base::TimeDelta(), /*initial_completed_cycles=*/0,
        lottie::Animation::Style::kLoop);
  }
  animation->SetPlaybackSpeed(
      AmbientUiModel::Get()->animation_playback_speed());
  progress_tracker_->RegisterAnimation(animation);
  animated_image_view_->Play(std::move(playback_config));
}

AmbientAnimationPlayer::~AmbientAnimationPlayer() {
  animated_image_view_->Stop();
}

}  // namespace ash
