// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_
#define ASH_SHELF_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

// Used by ShelfLayoutManager to handle gesture drag events while the
// handler is in kSwipeHomeToOverview mode. The controller handles swipe gesture
// from hot seat on the home screen. The gesture, if detected, transitions the
// home screen to the overview UI.
class ASH_EXPORT SwipeHomeToOverviewController {
 public:
  // The minimum vertical distance between gesture location and the bottom of
  // work area for the gesture to trigger transition to overview.
  static constexpr int kVerticalThresholdForOverviewTransition = 56;

  // The max pointer velocity at which home transitions to overview.
  static constexpr int kMovementVelocityThreshold = 10;

  // The constructor that uses default tick clock for
  // |overview_transition_timer_|. Should be preferred outside of test
  // environment.
  explicit SwipeHomeToOverviewController(int64_t display_id);

  // |display_id| - the ID of display on which the gesture started.
  // |tick_clock| - the tick clock that should be used by
  // |overview_transition_timer_|.
  SwipeHomeToOverviewController(int64_t display_id,
                                const base::TickClock* tick_clock);

  SwipeHomeToOverviewController(const SwipeHomeToOverviewController&) = delete;
  SwipeHomeToOverviewController& operator=(
      const SwipeHomeToOverviewController&) = delete;

  ~SwipeHomeToOverviewController();

  // Called during swiping up on the shelf.
  void Drag(const gfx::PointF& location_in_screen,
            float scroll_x,
            float scroll_y);
  void EndDrag(const gfx::PointF& location_in_screen,
               std::optional<float> velocity_y);
  void CancelDrag();

  base::OneShotTimer* overview_transition_timer_for_testing() {
    return &overview_transition_timer_;
  }

 private:
  enum class State { kInitial, kTrackingDrag, kFinished };

  // Starts |overview_transition_timer_|. No-op if the timer is already running.
  void ScheduleFinalizeDragAndShowOverview();

  // Finalizes the drag sequence, and starts overview session.
  // Note that the controller might keep getting drag updates as the user keeps
  // moving the pointer - those events will be ignored.
  void FinalizeDragAndShowOverview();

  // Finalizes the drag sequence by staying on the home screen.
  // |go_back| - if the gesture should invoke home screen back action.
  void FinalizeDragAndStayOnHomeScreen(bool go_back);

  const int64_t display_id_;

  State state_ = State::kInitial;

  // The y in-screen coordinate at which drag, when stopped, will cause
  // transition to overview. Overview session will be started iff the drag stops
  // above this line.
  // The value is set when the controller transitions to kStarted state.
  int overview_transition_threshold_y_ = 0;

  // Home screen is scaled down depending on gesture vertical distance from the
  // shelf - this is the target gesture vertical location at which home screen
  // should be scaled by the target scale factor.
  int scaling_threshold_y_ = 0;

  // The timer to run FinalizeDragAndShowOverview().
  base::OneShotTimer overview_transition_timer_;
};

}  // namespace ash

#endif  // ASH_SHELF_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_
