// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_CONTROLLER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_CONTROLLER_H_

#include "ash/ambient/ui/ambient_animation_frame_rate_schedule.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"

namespace ash {

class FrameThrottlingController;

// Throttles (lowers) the BeginFrame rate of the //viz service so that the
// lottie::Animation is ultimately rendered at a lower frame rate. This is
// done to reduce power consumption, thus increasing battery life. The time at
// which to throttle and the amount to throttle by is embedded within the
// lottie::Animation itself and chosen by the motion designer. Generally
// speaking, the less motion there is, the more opportunity there is to
// throttle.
//
// Once this class is destroyed or all lottie::Animations end, the BeginFrame
// rate in //viz is restored to the default.
class ASH_EXPORT AmbientAnimationFrameRateController
    : public lottie::AnimationObserver,
      public aura::WindowObserver {
 public:
  explicit AmbientAnimationFrameRateController(
      FrameThrottlingController* frame_throttling_controller);
  AmbientAnimationFrameRateController(
      const AmbientAnimationFrameRateController&) = delete;
  AmbientAnimationFrameRateController& operator=(
      const AmbientAnimationFrameRateController&) = delete;
  ~AmbientAnimationFrameRateController() override;

  // Adds a |window| playing a lottie |animation| to throttle. The |window|
  // must have a valid viz::FrameSinkId assigned to it. All lottie animations
  // added must have a SkottieWrapper with the same id() (meaning the identical
  // animation files).
  //
  // If the |window| has already been added in the past, this call is a no-op.
  // The controller gracefully handles both the |window| and |animation| being
  // destroyed; they get removed from the throttling schedule internally.
  void AddWindowToThrottle(aura::Window* window, lottie::Animation* animation);

 private:
  // lottie::AnimationObserver implementation:
  void AnimationFramePainted(const lottie::Animation* animation,
                             float) override;
  void AnimationIsDeleting(const lottie::Animation* animation) override;

  // aura::WindowObserver implementation:
  void OnWindowDestroying(aura::Window* window) override;

  AmbientAnimationFrameRateScheduleIterator FindCurrentSection() const;
  AmbientAnimationFrameRateScheduleIterator GetNextScheduledSection(
      AmbientAnimationFrameRateScheduleIterator section_in) const;
  void ThrottleFrameRateForCurrentSection();
  void ThrottleFrameRate(base::TimeDelta frame_interval);
  void RemoveWindowToThrottle(aura::Window* window);
  void TrySetNewTrackingAnimation();

  const raw_ptr<FrameThrottlingController> frame_throttling_controller_;

  // Matches one of the lottie::Animations in |windows_to_throttle_|. Even
  // though the caller may add multiple lottie::Animations in
  // AddWindowToThrottle(), only one is picked arbitrarily to track progress for
  // throttling purposes. This is done for simplicity purposes. The underlying
  // assumption is that all lottie::Animations' timestamps are all closely
  // synchronized; this is ensured within AmbientAnimationPlayer. If the
  // |tracking_animation_| is destroyed while this class is active, a new one
  // is picked from the |windows_to_throttle_|.
  raw_ptr<lottie::Animation> tracking_animation_ = nullptr;
  AmbientAnimationFrameRateSchedule schedule_;

  // Points to the current section in the |schedule_| that's being played.
  // Set to |schedule_.end()| if the animation is not playing currently.
  AmbientAnimationFrameRateScheduleIterator current_section_;
  base::flat_map<aura::Window*, raw_ptr<lottie::Animation, CtnExperimental>>
      windows_to_throttle_;
  base::ScopedMultiSourceObservation<lottie::Animation,
                                     lottie::AnimationObserver>
      animation_observations_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_CONTROLLER_H_
