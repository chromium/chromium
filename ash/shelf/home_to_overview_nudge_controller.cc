// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_to_overview_nudge_controller.h"

#include "ash/controls/contextual_nudge.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {

namespace {

// The amount of time after home shelf is shown before showing the nudge.
constexpr base::TimeDelta kShowDelay = base::Seconds(2);

// The duration of nudge opacity animations.
constexpr base::TimeDelta kNudgeFadeDuration = base::Milliseconds(300);

// The duration of the nudge opacity and transform animations when the nudge
// gets hidden on user tap.
constexpr base::TimeDelta kNudgeHideOnTapDuration = base::Milliseconds(150);

// The duration of a single component of the nudge position animation - the
// nudge is transformed vertically up and down for a preset number of
// iterations.
constexpr base::TimeDelta kNudgeTransformComponentDuration =
    base::Milliseconds(600);

// The baseline vertical offset from default kShown state bounds added to
// hotseat position when the nudge is shown - this is the offset that the
// hotseat will have once show throb animation completes.
constexpr int kHotseatBaselineNudgeOffset = -22;

// The number of times the nudge should be moved up and down when the nudge is
// shown.
constexpr int kNudgeShowThrobIterations = 5;

// The vertical max vertical ofsset from the baseline position during nudge show
// animation.
constexpr int kNudgeShowThrobAmplitude = 6;

// The vertical distance between the nudge widget and the hotseat.
constexpr int kNudgeMargins = 4;

gfx::Tween::Type GetHideTransformTween(
    HomeToOverviewNudgeController::HideTransition transition) {
  switch (transition) {
    case HomeToOverviewNudgeController::HideTransition::kShelfStateChange:
    case HomeToOverviewNudgeController::HideTransition::kNudgeTimeout:
      return gfx::Tween::EASE_OUT_2;
    case HomeToOverviewNudgeController::HideTransition::kUserTap:
      return gfx::Tween::FAST_OUT_LINEAR_IN;
  }
}

base::TimeDelta GetHideTransformDuration(
    HomeToOverviewNudgeController::HideTransition transition) {
  switch (transition) {
    case HomeToOverviewNudgeController::HideTransition::kShelfStateChange:
    case HomeToOverviewNudgeController::HideTransition::kNudgeTimeout:
      return kNudgeTransformComponentDuration;
    case HomeToOverviewNudgeController::HideTransition::kUserTap:
      return kNudgeHideOnTapDuration;
  }
}

base::TimeDelta GetHideFadeDuration(
    HomeToOverviewNudgeController::HideTransition transition) {
  switch (transition) {
    case HomeToOverviewNudgeController::HideTransition::kShelfStateChange:
      return base::TimeDelta();
    case HomeToOverviewNudgeController::HideTransition::kUserTap:
      return kNudgeHideOnTapDuration;
    case HomeToOverviewNudgeController::HideTransition::kNudgeTimeout:
      return kNudgeFadeDuration;
  }
}

class ObserverToCloseWidget : public ui::ImplicitAnimationObserver {
 public:
  explicit ObserverToCloseWidget(views::Widget* widget) : widget_(widget) {}

  ObserverToCloseWidget(const ObserverToCloseWidget& other) = delete;
  ObserverToCloseWidget& operator=(const ObserverToCloseWidget& other) = delete;

  ~ObserverToCloseWidget() override { StopObservingImplicitAnimations(); }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    widget_->Close();
    delete this;
  }

 private:
  const raw_ptr<views::Widget> widget_;
};

}  // namespace

HomeToOverviewNudgeController::HomeToOverviewNudgeController(
    HotseatWidget* hotseat_widget)
    : hotseat_widget_(hotseat_widget) {}

HomeToOverviewNudgeController::~HomeToOverviewNudgeController() = default;

void HomeToOverviewNudgeController::SetNudgeAllowedForCurrentShelf(
    bool allowed) {
  if (nudge_allowed_for_shelf_state_ == allowed)
    return;
  nudge_allowed_for_shelf_state_ = allowed;

  if (!nudge_allowed_for_shelf_state_) {
    nudge_show_timer_.Stop();
    nudge_hide_timer_.Stop();
    HideNudge(HideTransition::kShelfStateChange);
    return;
  }

  // Make sure that the overview, if opened, would show at least two app
  // windows.
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (windows.size() < 2)
    return;

  DCHECK(!nudge_);

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!contextual_tooltip::ShouldShowNudge(
          pref_service, contextual_tooltip::TooltipType::kHomeToOverview,
          nullptr)) {
    return;
  }

  nudge_hide_timer_.Stop();
  nudge_show_timer_.Start(
      FROM_HERE, kShowDelay,
      base::BindOnce(&HomeToOverviewNudgeController::ShowNudge,
                     base::Unretained(this)));
}

void HomeToOverviewNudgeController::OnWidgetDestroying(views::Widget* widget) {
  nudge_ = nullptr;
  widget_observations_.RemoveAllObservations();
}

void HomeToOverviewNudgeController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (widget == hotseat_widget_)
    UpdateNudgeAnchorBounds();
}

bool HomeToOverviewNudgeController::HasShowTimerForTesting() const {
  return nudge_show_timer_.IsRunning();
}

void HomeToOverviewNudgeController::FireShowTimerForTesting() {
  nudge_show_timer_.FireNow();
}

bool HomeToOverviewNudgeController::HasHideTimerForTesting() const {
  return nudge_hide_timer_.IsRunning();
}

void HomeToOverviewNudgeController::FireHideTimerForTesting() {
  nudge_hide_timer_.FireNow();
}

void HomeToOverviewNudgeController::ShowNudge() {
  DCHECK(!nudge_);

  // The nudge is effectively anchored below the hotseat widget, but the nudge
  // center is not generally aligned with the hotseat widget center. The nudge
  // should be horizontally centered in the screen, which might not be the
  // case for the hotseat widget bounds on home screen.
  // To work around this, HomeToOverviewNudgeController will update the anchor
  // bounds directly - see UpdateNudgeAnchorBounds().
  nudge_ = new ContextualNudge(
      nullptr, hotseat_widget_->GetNativeWindow()->parent(),
      ContextualNudge::Position::kBottom, gfx::Insets(kNudgeMargins),
      l10n_util::GetStringUTF16(IDS_ASH_HOME_TO_OVERVIEW_CONTEXTUAL_NUDGE),
      base::BindRepeating(&HomeToOverviewNudgeController::HandleNudgeTap,
                          weak_factory_.GetWeakPtr()));

  UpdateNudgeAnchorBounds();

  widget_observations_.AddObservation(nudge_->GetWidget());
  widget_observations_.AddObservation(hotseat_widget_.get());

  nudge_->GetWidget()->Show();
  nudge_->GetWidget()->GetLayer()->SetTransform(gfx::Transform());
  nudge_->label()->layer()->SetOpacity(0.0f);

  hotseat_widget_->GetLayerForNudgeAnimation()->SetTransform(gfx::Transform());

  base::TimeDelta total_animation_duration;

  // Initial animation - nudge slides in form the bottom, and hotseat moves up.
  auto animate_initial_transform = [](ui::Layer* layer) -> base::TimeDelta {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetTransitionDuration(kNudgeTransformComponentDuration);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    gfx::Transform transform;
    transform.Translate(0, kHotseatBaselineNudgeOffset);
    layer->SetTransform(transform);

    return layer->GetAnimator()->GetTransitionDuration();
  };

  total_animation_duration +=
      animate_initial_transform(hotseat_widget_->GetLayerForNudgeAnimation());
  animate_initial_transform(nudge_->GetWidget()->GetLayer());

  // Additionally the nudge label should fade in.
  {
    ui::ScopedLayerAnimationSettings settings(
        nudge_->label()->layer()->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetTransitionDuration(kNudgeFadeDuration);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    nudge_->label()->layer()->SetOpacity(1.0f);
  }

  auto enqueue_loop_transform = [](ui::Layer* layer,
                                   bool up) -> base::TimeDelta {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
    settings.SetTransitionDuration(kNudgeTransformComponentDuration);
    // Use enqueue preemption strategy, as the animation is expected to run
    // after other previously scheduled animations.
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

    gfx::Transform transform;
    transform.Translate(0, kHotseatBaselineNudgeOffset +
                               (up ? 0 : 1) * kNudgeShowThrobAmplitude);
    layer->SetTransform(transform);

    return layer->GetAnimator()->GetTransitionDuration();
  };

  // Enqueue series of animated up-down transforms on the nudge and the hotseat.
  // The final position should match the position after the initial animated
  // transform.
  for (int i = 0; i < kNudgeShowThrobIterations; ++i) {
    total_animation_duration += enqueue_loop_transform(
        hotseat_widget_->GetLayerForNudgeAnimation(), false /*up*/);
    enqueue_loop_transform(nudge_->GetWidget()->GetLayer(), false /*up*/);

    total_animation_duration += enqueue_loop_transform(
        hotseat_widget_->GetLayerForNudgeAnimation(), true /*up*/);
    enqueue_loop_transform(nudge_->GetWidget()->GetLayer(), true /*up*/);
  }

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  base::TimeDelta nudge_duration = contextual_tooltip::GetNudgeTimeout(
      pref_service, contextual_tooltip::TooltipType::kHomeToOverview);
  contextual_tooltip::HandleNudgeShown(
      pref_service, contextual_tooltip::TooltipType::kHomeToOverview);

  // If the nudge has a timeout, schedule a task to hide it. The timeout should
  // start when the animation sequence finishes.
  if (!nudge_duration.is_zero()) {
    nudge_hide_timer_.Start(
        FROM_HERE, nudge_duration + total_animation_duration,
        base::BindOnce(&HomeToOverviewNudgeController::HideNudge,
                       base::Unretained(this), HideTransition::kNudgeTimeout));
  }
}

void HomeToOverviewNudgeController::HideNudge(HideTransition transition) {
  if (!nudge_)
    return;

  auto animate_hide_transform = [](HideTransition transition,
                                   ui::Layer* layer) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(GetHideTransformTween(transition));
    settings.SetTransitionDuration(GetHideTransformDuration(transition));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    layer->SetTransform(gfx::Transform());
  };

  animate_hide_transform(transition,
                         hotseat_widget_->GetLayerForNudgeAnimation());
  animate_hide_transform(transition, nudge_->GetWidget()->GetLayer());

  {
    ui::ScopedLayerAnimationSettings settings(
        nudge_->label()->layer()->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetTransitionDuration(GetHideFadeDuration(transition));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(new ObserverToCloseWidget(nudge_->GetWidget()));

    nudge_->label()->layer()->SetOpacity(0.0f);
  }

  widget_observations_.RemoveAllObservations();
  nudge_ = nullptr;

  // Invalidated nudge tap handler callbacks.
  weak_factory_.InvalidateWeakPtrs();
}

void HomeToOverviewNudgeController::UpdateNudgeAnchorBounds() {
  // Update the nudge anchor bounds - use the hotseat bounds vertical
  // coordinates, so the nudge follows the vertical hotseat position, but the
  // shelf window horizontal coordinates to center nudge horizontally in the
  // shelf widget bounds (the hotseat widget parent).
  const gfx::Rect hotseat_bounds =
      hotseat_widget_->GetNativeWindow()->GetTargetBounds();
  const gfx::Rect shelf_bounds =
      hotseat_widget_->GetNativeWindow()->parent()->GetTargetBounds();
  nudge_->UpdateAnchorRect(
      gfx::Rect(gfx::Point(shelf_bounds.x(), hotseat_bounds.y()),
                gfx::Size(shelf_bounds.width(), hotseat_bounds.height())));
}

void HomeToOverviewNudgeController::HandleNudgeTap() {
  HideNudge(HideTransition::kUserTap);
}

}  // namespace ash
