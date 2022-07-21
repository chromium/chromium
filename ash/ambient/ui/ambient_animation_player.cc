// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_player.h"

#include <string>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {

namespace {

absl::optional<base::TimeDelta> FindCycleRestartTimestamp(
    const cc::SkottieWrapper& skottie) {
  static const base::NoDestructor<std::string> kRestartMarkerName(
      base::StrCat({kLottieCustomizableIdPrefix, "_Marker_CycleRestart"}));
  DCHECK(skottie.is_valid());
  absl::optional<base::TimeDelta> restart_timestamp;
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
    views::AnimatedImageView* animated_image_view)
    : animated_image_view_(animated_image_view) {
  DCHECK(animated_image_view_);
  lottie::Animation* animation = animated_image_view_->animated_image();
  DCHECK(animation);
  absl::optional<base::TimeDelta> cycle_restart_timestamp_found =
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
  animation_observation_.Observe(animation);
  animation->SetPlaybackSpeed(
      AmbientUiModel::Get()->animation_playback_speed());
  animated_image_view_->Play(lottie::Animation::PlaybackConfig::CreateWithStyle(
      lottie::Animation::Style::kLinear, *animation));
}

AmbientAnimationPlayer::~AmbientAnimationPlayer() {
  animated_image_view_->Stop();
}

void AmbientAnimationPlayer::AnimationCycleEnded(
    const lottie::Animation* animation) {
  DVLOG(1) << "First animation cycle ended. Restarting at "
           << cycle_restart_timestamp_;
  // No need to keep observing after the first animation cycle ends because all
  // future animation cycles will automatically loop below.
  animation_observation_.Reset();
  // Stop()/Play() are actually very light-weight operations. They do not cause
  // the animation to be re-loaded and only modify internal book-keeping state.
  // The latency between the last frame of the first animation cycle and the
  // first frame of the second cycle was compared against the same
  // frame-to-frame latency at the end of other animation cycles, and there was
  // no observable difference.
  animated_image_view_->Stop();
  animated_image_view_->Play(lottie::Animation::PlaybackConfig(
      {/*start_offset=*/cycle_restart_timestamp_,
       /*duration=*/
       animated_image_view_->animated_image()->GetAnimationDuration() -
           cycle_restart_timestamp_,
       lottie::Animation::Style::kLoop}));
}

}  // namespace ash
