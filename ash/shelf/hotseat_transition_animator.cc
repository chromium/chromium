// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_transition_animator.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

void ReportSmoothness(HotseatState new_state, int value) {
  switch (new_state) {
    case HotseatState::kShownClamshell:
    case HotseatState::kShownHomeLauncher:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.HotseatTransition.AnimationSmoothness."
          "TransitionToShownHotseat",
          value);
      break;
    case HotseatState::kExtended:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.HotseatTransition.AnimationSmoothness."
          "TransitionToExtendedHotseat",
          value);
      break;
    case HotseatState::kHidden:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.HotseatTransition.AnimationSmoothness."
          "TransitionToHiddenHotseat",
          value);
      break;
    case HotseatState::kNone:
      DCHECK(false);
      break;
  }
}

}  // namespace

HotseatTransitionAnimator::HotseatTransitionAnimator(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget) {}

HotseatTransitionAnimator::~HotseatTransitionAnimator() {
  StopObservingImplicitAnimations();
}

void HotseatTransitionAnimator::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  DoAnimation(old_state, new_state);
}

void HotseatTransitionAnimator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HotseatTransitionAnimator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HotseatTransitionAnimator::OnImplicitAnimationsCompleted() {
  std::move(animation_complete_callback_).Run();

  if (test_observer_)
    test_observer_->OnTransitionTestAnimationEnded();
}

void HotseatTransitionAnimator::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  // NOTE: This will be called only once (or zero times) each time for`
  // DoAnimation because we only have one LayerAnimationSequence for this
  // particular animation. If another is added we will have to modify how this
  // is sent out.
  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationAborted();
}

void HotseatTransitionAnimator::SetAnimationsEnabledInSessionState(
    bool enabled) {
  animations_enabled_for_current_session_state_ = enabled;

  ui::Layer* animating_background = shelf_widget_->GetAnimatingBackground();
  if (!enabled && animating_background->GetAnimator()->is_animating())
    animating_background->GetAnimator()->StopAnimating();
}

void HotseatTransitionAnimator::SetTestObserver(TestObserver* test_observer) {
  test_observer_ = test_observer;
}

void HotseatTransitionAnimator::DoAnimation(HotseatState old_state,
                                            HotseatState new_state) {
  const bool animating_to_shown_background =
      new_state != HotseatState::kShownHomeLauncher;
  gfx::Transform transform;
  if (animating_to_shown_background)
    transform.Translate(0, -ShelfConfig::Get()->in_app_shelf_size());

  if (!ShouldDoAnimation(old_state, new_state)) {
    shelf_widget_->GetAnimatingBackground()->SetTransform(transform);
    return;
  }

  StopObservingImplicitAnimations();

  shelf_widget_->GetAnimatingBackground()->SetColor(
      ShelfConfig::Get()->GetMaximizedShelfColor(shelf_widget_));

  gfx::Rect drag_handle_bounds(shelf_widget_->GetAnimatingBackground()->size());
  drag_handle_bounds.ClampToCenteredSize(ShelfConfig::Get()->DragHandleSize());
  shelf_widget_->GetAnimatingDragHandle()->SetBounds(drag_handle_bounds);

  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationWillStart(old_state, new_state);

  {
    ui::ScopedLayerAnimationSettings shelf_bg_animation_setter(
        shelf_widget_->GetAnimatingBackground()->GetAnimator());
    shelf_bg_animation_setter.SetTransitionDuration(
        ShelfConfig::Get()->hotseat_background_animation_duration());
    shelf_bg_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    shelf_bg_animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_complete_callback_ = base::BindOnce(
        &HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded,
        weak_ptr_factory_.GetWeakPtr(), old_state, new_state);
    shelf_bg_animation_setter.AddObserver(this);

    ui::AnimationThroughputReporter reporter(
        shelf_bg_animation_setter.GetAnimator(),
        metrics_util::ForSmoothnessV3(
            base::BindRepeating(&ReportSmoothness, new_state)));

    shelf_widget_->GetAnimatingBackground()->SetTransform(transform);
  }
}

bool HotseatTransitionAnimator::ShouldDoAnimation(HotseatState old_state,
                                                  HotseatState new_state) {
  if (!animations_enabled_for_current_session_state_)
    return false;

  // The shelf should be directly hidden without animation if the auto hide
  // state is auto hidden.
  if (shelf_widget_->shelf_layout_manager()->is_shelf_auto_hidden())
    return false;

  return (new_state == HotseatState::kShownHomeLauncher ||
          old_state == HotseatState::kShownHomeLauncher) &&
         !(new_state == HotseatState::kShownClamshell ||
           old_state == HotseatState::kShownClamshell) &&
         display::Screen::GetScreen()->InTabletMode();
}

void HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded(
    HotseatState old_state,
    HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationEnded(old_state, new_state);
}

}  // namespace ash
