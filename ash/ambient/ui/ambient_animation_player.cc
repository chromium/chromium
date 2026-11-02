// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_player.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/ambient/ui/ambient_animation_frame_rate_schedule.h"
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
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {

namespace {

AmbientAnimationFrameRateSchedule BuildSchedule(lottie::Animation* animation) {
  DCHECK(animation);
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule(
          animation->skottie()->GetAllMarkers());
  if (schedule.empty()) {
    // This means the animation file needs to be fixed. This should never happen
    // in the field in theory, but just in case, resort to the default frame
    // rate schedule (no throttling).
    LOG(DFATAL) << "Ambient animation has invalid frame rate markers.";
    schedule = BuildDefaultFrameRateSchedule();
  }
  return schedule;
}

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
      progress_tracker_(progress_tracker),
      schedule_(BuildSchedule(animated_image_view->animated_image())),
      current_section_(schedule_.end()) {
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
  animation_observation_.Observe(animation);
  current_section_ = FindCurrentSection();
  ThrottleFrameRateForCurrentSection();
}

AmbientAnimationPlayer::~AmbientAnimationPlayer() {
  animated_image_view_->Stop();
  ThrottleFrameRate(std::nullopt);
}

void AmbientAnimationPlayer::AnimationFramePainted(
    const lottie::Animation* animation,
    float) {
  auto new_current_section = FindCurrentSection();
  if (new_current_section == current_section_) {
    return;
  }

  DVLOG(1) << "Found new frame rate section: " << *new_current_section;
  current_section_ = new_current_section;
  ThrottleFrameRateForCurrentSection();
}

AmbientAnimationFrameRateScheduleIterator
AmbientAnimationPlayer::FindCurrentSection() const {
  lottie::Animation* animation = animated_image_view_->animated_image();
  DCHECK(animation);
  std::optional<float> current_progress = animation->GetCurrentProgress();
  if (!current_progress) {
    DVLOG(1) << "Animation is not playing currently. Cannot map timestamp to "
                "scheduled frame rate.";
    return schedule_.end();
  }

  // Always start searching from the last section the animation was on. Since
  // animations progress linearly in small increments, most of the time, the
  // |current_section_| will not change.
  AmbientAnimationFrameRateScheduleIterator new_current_section =
      current_section_ == schedule_.end() ? schedule_.begin()
                                          : current_section_;
  AmbientAnimationFrameRateScheduleIterator orig_current_section =
      new_current_section;
  while (!new_current_section->Contains(*current_progress)) {
    ++new_current_section;
    // Note the AmbientAnimationFrameRateSchedule by design is contiguous. Every
    // possible timestamp falls within a section of the schedule, so it's
    // impossible to infinite loop here.
    DCHECK(new_current_section != orig_current_section)
        << "Infinite loop detected. AmbientAnimationFrameRateSchedule has gap "
           "and is malformed.";
    // The schedule is cyclic. Loop back to the beginning.
    if (new_current_section == schedule_.end()) {
      new_current_section = schedule_.begin();
    }
  }
  return new_current_section;
}

void AmbientAnimationPlayer::ThrottleFrameRateForCurrentSection() {
  // TODO(esum): There is a corner case not accounted for yet. Say the frame
  // interval is large (1 second). And say we throttle to 1 fps at time 10 sec,
  // and we need to restore the default 60 fps at 19.1 sec. We will get:
  // AnimationFramePainted(10 sec)
  // AnimationFramePainted(11 sec)
  // ...
  // AnimationFramePainted(19 sec) - Still not past the 19.1 second mark
  // AnimationFramePainted(20 sec) - Switch back to 60 fps here.
  //
  // This is bad because we switch back .9 seconds too late, which is a lot.
  // To fix this, we could start a timer that fires when the current section is
  // over and we need to switch to the new frame rate. It currently is not a
  // problem because in practice, the frame rates never get small enough to
  // notice this issue. But it will be a problem with the slideshow lottie
  // animation.
  if (current_section_ != schedule_.end()) {
    const base::TimeDelta frame_interval = current_section_->frame_interval;
    ThrottleFrameRate(frame_interval == kDefaultFrameInterval
                          ? std::nullopt
                          : std::make_optional(frame_interval));
  }
}

void AmbientAnimationPlayer::ThrottleFrameRate(
    std::optional<base::TimeDelta> frame_interval) {
  if (frame_interval) {
    DVLOG(1) << "Throttling frame rate to " << frame_interval->ToHz() << "hz";
  } else {
    VLOG(1) << "Resetting frame rate to default";
  }
  animated_image_view_->SetFrameInterval(frame_interval);
}

}  // namespace ash
