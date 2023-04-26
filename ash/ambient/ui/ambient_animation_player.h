// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_PLAYER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_PLAYER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/lottie/animation.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

class AmbientAnimationProgressTracker;

// Plays an AnimatedImageView in a loop until destruction. The "looping" logic
// meets ambient mode's custom requirements: The lottie animation may optionally
// have a "marker" embedded in it. The "marker" is a timestamp set by the motion
// designer, which in this case, indicates where the animation should restart
// after each cycle ends. If the marker timestamp is M and the total animation
// cycle duration is D (where 0 < M < D), then the animation cycles look
// like this:
// [0, D]
// [M, D]
// [M, D]
// ...
//
// Note the very first animation cycle is always played starting at time 0 (the
// very first frame in the animation).
//
// If the animation does not have a marker embedded in it, the default behavior
// is to restart at the beginning of the animation each time (M is effectively
// 0):
// [0, D]
// [0, D]
// [0, D]
// ...
class ASH_EXPORT AmbientAnimationPlayer {
 public:
  // Starts playing the |animated_image_view| immediately upon construction.
  explicit AmbientAnimationPlayer(
      views::AnimatedImageView* animated_image_view,
      AmbientAnimationProgressTracker* progress_tracker);
  AmbientAnimationPlayer(const AmbientAnimationPlayer&) = delete;
  AmbientAnimationPlayer& operator=(const AmbientAnimationPlayer&) = delete;
  ~AmbientAnimationPlayer();

 private:
  const raw_ptr<views::AnimatedImageView> animated_image_view_;
  const raw_ptr<AmbientAnimationProgressTracker> progress_tracker_;
  base::TimeDelta cycle_restart_timestamp_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_PLAYER_H_
