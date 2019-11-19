// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace base {
class TimeDelta;
}

namespace ash {

class HomeScreenController;

// Helper class to schedule Home Screen view animations.
class ASH_EXPORT HomeScreenPresenter {
 public:
  // The type of transition that should be applied to home screen view in
  // reaction to overview session ending or starting.
  // NOTE: kSlideHomeIn and kScaleHomeIn are conceptually the same, but assume
  // different starting state.
  enum class TransitionType {
    // Resets home screen vertical translation to 0 (i.e. moves the view into
    // full-screen bounds). Changes opacity to 1.0.
    kSlideHomeIn,

    // Vertically translates home screen down by a predetermined amount into
    // full
    // screen home bounds. Changes opacity to 0.0.
    kSlideHomeOut,

    // Resets home screen scale to 1.0 (i.e. move the view into fullscreen
    // bounds).
    // Changes opacity to 1.0.
    kScaleHomeIn,

    // Scales home screen down by a predetermined amount from the fullscreen
    // bounds (keeping the view centered in the original bounds). Changes
    // opacity to 0.0.
    kScaleHomeOut
  };

  explicit HomeScreenPresenter(HomeScreenController* controller);
  ~HomeScreenPresenter();

  // Schedules animation for the home screen when overview mode starts or ends.
  void ScheduleOverviewModeAnimation(TransitionType transition, bool animate);

 private:
  // Updates the home screen state to match the final state for |transition|.
  // If |animation_duration| is 0, the update will be immediate, otherwise the
  // update will be animated.
  void SetFinalHomeTransformForTransition(TransitionType transition,
                                          base::TimeDelta animation_duration);

  HomeScreenController* controller_;

  DISALLOW_COPY_AND_ASSIGN(HomeScreenPresenter);
};

}  // namespace ash

#endif  //  ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_
