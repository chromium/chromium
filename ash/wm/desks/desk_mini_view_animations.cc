// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_state_new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kBarBackgroundDuration =
    base::TimeDelta::FromMilliseconds(200);

constexpr base::TimeDelta kExistingMiniViewsAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

constexpr base::TimeDelta kRemovedMiniViewsFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);

// |settings| will be initialized with a fast-out-slow-in animation with the
// given |duration|.
void InitScopedAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                                 base::TimeDelta duration) {
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Animates the transform of the layer of the given |view| from the supplied
// |begin_transform| to the identity transform.
void AnimateView(views::View* view, const gfx::Transform& begin_transform) {
  ui::Layer* layer = view->layer();
  layer->SetTransform(begin_transform);

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);
  layer->SetTransform(kEndTransform);
}

// See details at AnimateView.
void AnimateMiniViews(std::vector<DeskMiniView*> mini_views,
                      const gfx::Transform& begin_transform) {
  for (auto* mini_view : mini_views)
    AnimateView(mini_view, begin_transform);
}

// A self-deleting object that performs a fade out animation on
// |removed_mini_view|'s layer by changing its opacity from 1 to 0,
// and deleting |removed_mini_view| and itself when the animation is complete.
// TODO(afakhry): Consider generalizing HidingWindowAnimationObserverBase to be
// reusable for the mini_view removal animation.
class RemovedMiniViewFadeOutAnimation : public ui::ImplicitAnimationObserver {
 public:
  RemovedMiniViewFadeOutAnimation(DeskMiniView* removed_mini_view)
      : removed_mini_view_(removed_mini_view) {
    ui::Layer* layer = removed_mini_view_->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kRemovedMiniViewsFadeOutDuration);
    settings.AddObserver(this);
    layer->SetTransform(kEndTransform);
    layer->SetOpacity(0);
  }

  ~RemovedMiniViewFadeOutAnimation() override {
    DCHECK(removed_mini_view_->parent());
    removed_mini_view_->parent()->RemoveChildViewT(removed_mini_view_);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  DeskMiniView* removed_mini_view_;

  DISALLOW_COPY_AND_ASSIGN(RemovedMiniViewFadeOutAnimation);
};

}  // namespace

void PerformNewDeskMiniViewAnimation(
    DesksBarView* bar_view,
    const std::vector<DeskMiniView*>& new_mini_views,
    int shift_x,
    bool first_time_mini_views) {
  if (first_time_mini_views) {
    ui::Layer* layer = bar_view->background_view()->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kBarBackgroundDuration);
    layer->SetOpacity(1);

    // Expect that that bar background is translated off the screen when it's
    // the first time we're adding mini_views.
    DCHECK(!layer->GetTargetTransform().IsIdentity());

    layer->SetTransform(gfx::Transform());
    auto* controller = Shell::Get()->overview_controller();
    DCHECK(controller->InOverviewSession());
    controller->overview_session()->PositionWindows(true);
  }

  gfx::Transform begin_transform;
  begin_transform.Translate(shift_x, 0);

  for (auto* mini_view : bar_view->mini_views()) {
    const bool is_new = base::Contains(new_mini_views, mini_view);

    ui::Layer* layer = mini_view->layer();
    if (is_new)
      layer->SetOpacity(0.f);
    layer->SetTransform(begin_transform);

    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);

    // Fade in new desk mini_views and shift all of them (new & old) to the
    // left.
    if (is_new)
      layer->SetOpacity(1);
    layer->SetTransform(kEndTransform);
  }

  // The new desk button in the expanded desks bar moves at the opposite
  // direction of the existing mini views while creating a new mini view. The
  // existing mini views will move from right to left while the new desk button
  // will move from left to right. Since the newly added mini view will be added
  // between the last mini view and the new desk button.
  if (features::IsBentoEnabled()) {
    gfx::Transform new_desk_button_begin_transform;
    new_desk_button_begin_transform.Translate(-shift_x, 0);
    AnimateView(bar_view->expanded_state_new_desk_button(),
                new_desk_button_begin_transform);
  }
}

void PerformRemoveDeskMiniViewAnimation(
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    ExpandedStateNewDeskButton* expanded_state_new_desk_button,
    int shift_x) {
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  new RemovedMiniViewFadeOutAnimation(removed_mini_view);

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  if (features::IsBentoEnabled()) {
    AnimateView(expanded_state_new_desk_button,
                mini_views_right_begin_transform);
  }
}

}  // namespace ash
