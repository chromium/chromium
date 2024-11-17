// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "cc/paint/skottie_wrapper.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/window.h"

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

}  // namespace

AmbientAnimationFrameRateController::AmbientAnimationFrameRateController(
    FrameThrottlingController* frame_throttling_controller)
    : frame_throttling_controller_(frame_throttling_controller),
      current_section_(schedule_.end()) {
  DCHECK(frame_throttling_controller_);
}

AmbientAnimationFrameRateController::~AmbientAnimationFrameRateController() {
  ThrottleFrameRate(kDefaultFrameInterval);
}

void AmbientAnimationFrameRateController::AnimationFramePainted(
    const lottie::Animation* animation,
    float) {
  if (animation != tracking_animation_.get())
    return;

  auto new_current_section = FindCurrentSection();
  if (new_current_section == current_section_)
    return;

  DVLOG(1) << "Found new frame rate section: " << *new_current_section;
  current_section_ = new_current_section;
  ThrottleFrameRateForCurrentSection();
}

// Note either AnimationIsDeleting() or OnWindowDestroying() could come first.
// Whichever one does should cause both the window and animation to be removed
// from book-keeping entirely.
void AmbientAnimationFrameRateController::AnimationIsDeleting(
    const lottie::Animation* deleting_animation) {
  aura::Window* window_to_remove = nullptr;
  for (const auto& [window, animation] : windows_to_throttle_) {
    if (animation == deleting_animation) {
      window_to_remove = window;
      break;
    }
  }
  DCHECK(window_to_remove);
  RemoveWindowToThrottle(window_to_remove);
}

void AmbientAnimationFrameRateController::OnWindowDestroying(
    aura::Window* window) {
  RemoveWindowToThrottle(window);
}

void AmbientAnimationFrameRateController::AddWindowToThrottle(
    aura::Window* window,
    lottie::Animation* animation) {
  DCHECK(window);
  DCHECK(window->GetFrameSinkId().is_valid())
      << "Window missing frame sink id: " << window->GetId();
  DCHECK(animation);
  if (windows_to_throttle_.contains(window))
    return;

  if (tracking_animation_) {
    DCHECK_EQ(tracking_animation_->skottie()->id(), animation->skottie()->id())
        << "All lottie animations must have the same json content";
  }
  windows_to_throttle_[window] = animation;
  TrySetNewTrackingAnimation();
  // Always observe even if the incoming |animation| is not the
  // |tracking_animation_| so that we get notified when AnimationIsDeleting().
  animation_observations_.AddObservation(animation);
  window_observations_.AddObservation(window);
  // Update throttling with the expanded list of |windows_to_throttle_|.
  ThrottleFrameRateForCurrentSection();
}

AmbientAnimationFrameRateScheduleIterator
AmbientAnimationFrameRateController::FindCurrentSection() const {
  DCHECK(tracking_animation_);
  std::optional<float> current_progress =
      tracking_animation_->GetCurrentProgress();
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
    if (new_current_section == schedule_.end())
      new_current_section = schedule_.begin();
  }
  return new_current_section;
}

void AmbientAnimationFrameRateController::ThrottleFrameRateForCurrentSection() {
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
  if (current_section_ != schedule_.end())
    ThrottleFrameRate(current_section_->frame_interval);
}

void AmbientAnimationFrameRateController::ThrottleFrameRate(
    base::TimeDelta frame_interval) {
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows_as_vector;
  for (const auto& [window, animation] : windows_to_throttle_) {
    windows_as_vector.push_back(window);
  }

  if (frame_interval == kDefaultFrameInterval) {
    VLOG(1) << "Resetting frame rate to default";
    frame_throttling_controller_->EndThrottling();
  } else {
    DVLOG(1) << "Throttling frame rate to " << frame_interval.ToHz() << "hz";
    frame_throttling_controller_->StartThrottling(windows_as_vector,
                                                  frame_interval);
  }
}

void AmbientAnimationFrameRateController::RemoveWindowToThrottle(
    aura::Window* window) {
  window_observations_.RemoveObservation(window);
  auto iter = windows_to_throttle_.find(window);
  DCHECK(iter != windows_to_throttle_.end());
  lottie::Animation* unregistered_animation = iter->second;
  animation_observations_.RemoveObservation(unregistered_animation);
  windows_to_throttle_.erase(iter);
  if (unregistered_animation != tracking_animation_)
    return;

  tracking_animation_ = nullptr;
  TrySetNewTrackingAnimation();
  if (tracking_animation_) {
    DVLOG(1)
        << "Observing new lottie Animation. Resetting frame rate throttling.";
    ThrottleFrameRateForCurrentSection();
  } else {
    DVLOG(1) << "No more lottie animations are active. Restoring default frame "
                "rate and going idle...";
    ThrottleFrameRate(kDefaultFrameInterval);
  }
}

void AmbientAnimationFrameRateController::TrySetNewTrackingAnimation() {
  if (tracking_animation_) {
    DVLOG(4) << "Tracking animation already set.";
    return;
  }

  if (windows_to_throttle_.empty()) {
    DVLOG(4) << "No lottie animations to track. Going idle.";
    return;
  }

  tracking_animation_ = windows_to_throttle_.begin()->second.get();
  schedule_ = BuildSchedule(tracking_animation_.get());
  // Set |current_section_| to be |schedule_.end()| temporarily so that
  // FindCurrentSection() knows to start searching the new schedule from
  // scratch.
  current_section_ = schedule_.end();
  current_section_ = FindCurrentSection();
}

}  // namespace ash
