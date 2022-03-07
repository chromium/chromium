// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_
#define ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/shelf/drag_handle.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

class ContextualNudge;
class Shelf;

// Handles the swipe up gesture on login shelf. The gesture is enabled only when
// the login screen stack registers a handler for the swipe gesture.
// Currently, the handler may be set during user first run flow on the final
// screen of the flow (where swipe up will finalize user setup flow and start
// the user session).
class ASH_EXPORT LoginShelfGestureController : public views::WidgetObserver {
 public:
  LoginShelfGestureController(Shelf* shelf,
                              DragHandle* drag_handle,
                              const std::u16string& gesture_nudge,
                              base::RepeatingClosure fling_handler,
                              base::OnceClosure exit_handler);
  LoginShelfGestureController(const LoginShelfGestureController& other) =
      delete;
  LoginShelfGestureController& operator=(
      const LoginShelfGestureController& other) = delete;
  ~LoginShelfGestureController() override;

  // Handles a gesture event on the login shelf.
  // Returns whether the controller handled the event.
  // The controller will handle SCROLL_BEGIN and SCROLL_UPDATE events if the
  // scroll direction changes towards the top of the screen (and is within the
  // shelf bounds).
  // SCROLL_END and SCROLL_FLING_START will be only handled if a SCROLL_BEGIN or
  // SCROLL_UPDATE was handled (i.e. if |active_| is true).
  bool HandleGestureEvent(const ui::GestureEvent& event_in_screen);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  ContextualNudge* nudge_for_testing() { return nudge_; }

  base::OneShotTimer* nudge_animation_timer_for_testing() {
    return &nudge_animation_timer_;
  }

 private:
  // Starts handling gesture drag if it's a start of upward swipe from the
  // shelf.
  bool MaybeStartGestureDrag(const ui::GestureEvent& event_in_screen);

  // Ends gesture drag, and runs fling_handler_ if the gesture was detected to
  // be upward fling from the shelf.
  void EndDrag(const ui::GestureEvent& event_in_screen);

  // Schedules one nudge and drag handle animation sequence - see
  // RunNudgeAnimation().
  // When the animation sequence is over, it schedules another animation
  // sequence to start after a delay.
  // The sequences are repeated until the login shelf gesture is disabled, or
  // the user taps the contextual nudge. |delay| - The delay with which the
  // animation sequence should start.
  void ScheduleNudgeAnimation(base::TimeDelta delay);

  // Runs one drag handle and nudge animation sequence. The sequene consists of
  // the following stages:
  // *   initial stage, where the nudge and drag handle move up,
  // *   throbbing, where the drag handle is moving up and down
  // *   the final stage, where both drag handle and contextual nudge move to
  //     their initial position.
  // |callback| - Run when the animation sequence completes.
  void RunNudgeAnimation(base::OnceClosure callback);

  // Handler for a tap on the login shelf gesture contextual nudge. It stops
  // animating the nudge and the drag handle.
  void HandleNudgeTap();

  // Whether a gesture drag is being handled by the controller.
  bool active_ = false;

  // Whether the gesture contextual nudge and drag handle animation has been
  // stopped - happens when user taps on the nudge.
  bool animation_stopped_ = false;

  Shelf* const shelf_;
  DragHandle* const drag_handle_;

  // The contextual nudge bubble for letting the user know they can swipe up to
  // perform an action.
  // It's a bubble dialog widget delegate, deleted when its widget is destroyed,
  // and the widget is owned by the window hierarchy.
  ContextualNudge* nudge_ = nullptr;

  // The callback to be run whenever swipe from shelf is detected.
  base::RepeatingClosure const fling_handler_;

  // Called when the swipe controller gets reset (at which point swipe from the
  // login shelf gesture will be disabled).
  base::OnceClosure exit_handler_;

  // Timer used to schedule an animation sequence.
  base::OneShotTimer nudge_animation_timer_;

  base::WeakPtrFactory<LoginShelfGestureController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_
