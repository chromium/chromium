// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
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

// Animates the transforms of the layers of the given |mini_views| from the
// supplied |begin_transform| to the identity transform.
void AnimateMiniViews(std::vector<DeskMiniView*> mini_views,
                      const gfx::Transform& begin_transform) {
  for (auto* mini_view : mini_views) {
    ui::Layer* layer = mini_view->layer();
    layer->SetTransform(begin_transform);

    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);
    layer->SetTransform(kEndTransform);
  }
}

// A self-deleting object that takes ownership of the |removed_mini_view| and
// performs a fade out animation on its layer by changing its opacity from 1 to
// 0, and deleting it and itself when the animation is complete.
// TODO(afakhry): Consider generalizing HidingWindowAnimationObserverBase to be
// reusable for the mini_view removal animation.
class RemovedMiniViewFadeOutAnimation : public ui::ImplicitAnimationObserver {
 public:
  RemovedMiniViewFadeOutAnimation(
      std::unique_ptr<DeskMiniView> removed_mini_view)
      : removed_mini_view_(std::move(removed_mini_view)) {
    ui::Layer* layer = removed_mini_view_->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kRemovedMiniViewsFadeOutDuration);
    settings.AddObserver(this);
    layer->SetTransform(kEndTransform);
    layer->SetOpacity(0);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<DeskMiniView> removed_mini_view_;

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

  for (const auto& mini_view : bar_view->mini_views()) {
    const bool is_new = base::Contains(new_mini_views, mini_view.get());

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
}

void PerformRemoveDeskMiniViewAnimation(
    std::unique_ptr<DeskMiniView> removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x) {
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  new RemovedMiniViewFadeOutAnimation(std::move(removed_mini_view));

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);
}

}  // namespace ash
