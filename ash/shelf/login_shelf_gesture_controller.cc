// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_gesture_controller.h"

#include "ash/controls/contextual_nudge.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The upward velocity threshold for the swipe up from the login shelf to be
// reported as fling gesture.
constexpr float kVelocityToHomeScreenThreshold = 1000.f;

// The delay between the time the login shelf gesture nudge is shown, and the
// time it starts animating.
constexpr base::TimeDelta kNudgeAnimationEntranceDelay =
    base::Milliseconds(500);

// The duration of different parts of the nudge animation.
constexpr base::TimeDelta kNudgeAnimationStageDuration =
    base::Milliseconds(600);

// The duration of the animation that moves the drag handle and the contextual
// nudge to their initial position when the user cancels the nudge animation by
// tapping the contextual nudge.
constexpr base::TimeDelta kNudgeStopAnimationDuration = base::Milliseconds(150);

// The interval between the end of one nudge animation sequence, and the start
// of the next nudge animation sequence.
constexpr base::TimeDelta kAnimationInterval = base::Seconds(5);

// The offset drag handle and nudge widget have from the default position during
// the nudge animation sequence.
constexpr int kNudgeAnimationBaseOffset = -8;

// The number of times drag handle is moved up and down during single nudge
// animation cycle.
constexpr int kNudgeAnimationThrobIntervals = 3;

// The maximal offset drag handle has from the base position during throb
// section of the nudge animation.
constexpr int kNudgeAnimationThrobAmplitude = 6;

// Implicit animation observer that runs a callback once the animations
// complete, and then deletes itself.
class ImplicitAnimationCallbackRunner : public ui::ImplicitAnimationObserver {
 public:
  explicit ImplicitAnimationCallbackRunner(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  ~ImplicitAnimationCallbackRunner() override {
    StopObservingImplicitAnimations();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    StopObservingImplicitAnimations();
    std::move(callback_).Run();
    delete this;
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

LoginShelfGestureController::LoginShelfGestureController(
    Shelf* shelf,
    DragHandle* drag_handle,
    const std::u16string& gesture_nudge,
    base::RepeatingClosure fling_handler,
    base::OnceClosure exit_handler)
    : shelf_(shelf),
      drag_handle_(drag_handle),
      fling_handler_(std::move(fling_handler)),
      exit_handler_(std::move(exit_handler)) {
  DCHECK(fling_handler_);
  DCHECK(exit_handler_);

  const bool is_oobe = Shell::Get()->session_controller()->GetSessionState() ==
                       session_manager::SessionState::OOBE;
  const SkColor nudge_text_color =
      is_oobe ? gfx::kGoogleGrey700 : gfx::kGoogleGrey100;
  nudge_ = new ContextualNudge(
      drag_handle, nullptr /*parent_window*/, ContextualNudge::Position::kTop,
      gfx::Insets(8), gesture_nudge, nudge_text_color,
      base::BindRepeating(&LoginShelfGestureController::HandleNudgeTap,
                          weak_factory_.GetWeakPtr()));
  nudge_->GetWidget()->Show();
  nudge_->GetWidget()->AddObserver(this);

  ScheduleNudgeAnimation(kNudgeAnimationEntranceDelay);
}

LoginShelfGestureController::~LoginShelfGestureController() {
  if (nudge_) {
    nudge_->GetWidget()->RemoveObserver(this);
    nudge_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
  nudge_ = nullptr;

  std::move(exit_handler_).Run();
  CHECK(!IsInObserverList());
}

bool LoginShelfGestureController::HandleGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return MaybeStartGestureDrag(event_in_screen);

  // If the previous events in the gesture sequence did not start handling the
  // gesture, try again.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE)
    return active_ || MaybeStartGestureDrag(event_in_screen);

  if (!active_)
    return false;

  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    EndDrag(event_in_screen);
    return true;
  }

  // Ending non-fling gesture, or unexpected event (if different than
  // SCROLL_END), mark the controller as inactive, but report the event as
  // handled in the former case only.
  active_ = false;
  return event_in_screen.type() == ui::ET_GESTURE_SCROLL_END;
}

void LoginShelfGestureController::OnWidgetDestroying(views::Widget* widget) {
  nudge_ = nullptr;
  nudge_animation_timer_.Stop();
}

bool LoginShelfGestureController::MaybeStartGestureDrag(
    const ui::GestureEvent& event_in_screen) {
  DCHECK(event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
         event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE);

  // Ignore downward swipe for scroll begin.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      event_in_screen.details().scroll_y_hint() >= 0) {
    return false;
  }

  // Ignore downward swipe for scroll update.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE &&
      event_in_screen.details().scroll_y() >= 0) {
    return false;
  }

  // Ignore swipes that are outside of the shelf bounds.
  if (event_in_screen.location().y() <
      shelf_->shelf_widget()->GetWindowBoundsInScreen().y()) {
    return false;
  }

  active_ = true;
  return true;
}

void LoginShelfGestureController::EndDrag(
    const ui::GestureEvent& event_in_screen) {
  DCHECK_EQ(event_in_screen.type(), ui::ET_SCROLL_FLING_START);

  active_ = false;

  // If the drag ends below the shelf, do not go to home screen (theoratically
  // it may happen in kExtended hotseat case when drag can start and end below
  // the shelf).
  if (event_in_screen.location().y() >=
      shelf_->shelf_widget()->GetWindowBoundsInScreen().y()) {
    return;
  }

  const int velocity_y = event_in_screen.details().velocity_y();
  if (velocity_y > -kVelocityToHomeScreenThreshold)
    return;

  fling_handler_.Run();
}

void LoginShelfGestureController::ScheduleNudgeAnimation(
    base::TimeDelta delay) {
  if (!nudge_ || animation_stopped_)
    return;

  nudge_animation_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(
          &LoginShelfGestureController::RunNudgeAnimation,
          base::Unretained(this),
          base::BindOnce(&LoginShelfGestureController::ScheduleNudgeAnimation,
                         weak_factory_.GetWeakPtr(), kAnimationInterval)));
}

void LoginShelfGestureController::RunNudgeAnimation(
    base::OnceClosure callback) {
  auto animate_entrance = [](ui::Layer* layer) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetTransitionDuration(kNudgeAnimationStageDuration);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    gfx::Transform transform;
    transform.Translate(0, kNudgeAnimationBaseOffset);
    layer->SetTransform(transform);
  };

  animate_entrance(nudge_->GetWidget()->GetLayer());
  animate_entrance(drag_handle_->layer());

  auto animate_throb = [](ui::Layer* layer, bool down) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
    settings.SetTransitionDuration(kNudgeAnimationStageDuration);
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

    gfx::Transform transform;
    transform.Translate(0, kNudgeAnimationBaseOffset +
                               kNudgeAnimationThrobAmplitude * (down ? 1 : 0));
    layer->SetTransform(transform);
  };

  for (int i = 0; i < kNudgeAnimationThrobIntervals; ++i) {
    animate_throb(drag_handle_->layer(), /*down=*/true);
    animate_throb(drag_handle_->layer(), /*down=*/false);

    // Keep the animation going for the nudge, even though it's kept in place
    // The primary goal is to "pause" the animation while drag handle is
    // throbbing, and prevent the last animation stage from starting too soon.
    animate_throb(nudge_->GetWidget()->GetLayer(), /*down=*/false);
    animate_throb(nudge_->GetWidget()->GetLayer(), /*down=*/false);
  }

  auto animate_exit = [](ui::Layer* layer, base::OnceClosure callback) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
    settings.SetTransitionDuration(kNudgeAnimationStageDuration);
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    if (callback) {
      settings.AddObserver(
          new ImplicitAnimationCallbackRunner(std::move(callback)));
    }

    layer->SetTransform(gfx::Transform());
  };

  animate_exit(nudge_->GetWidget()->GetLayer(), base::OnceClosure());
  animate_exit(drag_handle_->layer(), std::move(callback));
}

void LoginShelfGestureController::HandleNudgeTap() {
  if (animation_stopped_)
    return;

  animation_stopped_ = true;
  nudge_animation_timer_.Stop();

  auto animate_exit = [](ui::Layer* layer) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    settings.SetTransitionDuration(kNudgeStopAnimationDuration);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    layer->SetTransform(gfx::Transform());
  };

  animate_exit(nudge_->GetWidget()->GetLayer());
  animate_exit(drag_handle_->layer());
}

}  // namespace ash
