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
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform_util.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kBarBackgroundDuration =
    base::TimeDelta::FromMilliseconds(200);

constexpr base::TimeDelta kExistingMiniViewsAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

constexpr base::TimeDelta kRemovedMiniViewsFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);

constexpr base::TimeDelta kZeroStateAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

// Scale for entering/exiting zero state.
constexpr float kEnterOrExitZeroStateScale = 0.6f;

// |settings| will be initialized with a fast-out-slow-in animation with the
// given |duration|.
void InitScopedAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                                 base::TimeDelta duration) {
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(gfx::Tween::ACCEL_20_DECEL_60);
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

// Scales down the given |view| to |kEnterOrExitZeroStateScale| and fading out
// it at the same time. Scale down animation will be around the |start| point.
void ScaleDownAndFadeOutView(views::View* view, const gfx::Point& start) {
  ui::Layer* layer = view->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);

  const gfx::Point end = view->bounds().CenterPoint();
  layer->SetTransform(gfx::GetScaleTransform(
      gfx::Point(start.x() - end.x(), start.y() - end.y()),
      kEnterOrExitZeroStateScale));
  layer->SetOpacity(0.f);
}

// Scales up the given |view| from |kEnterOrExitZeroStateScale| to identity and
// fading in it at the same time. Scale up animation will be around the |start|
// point.
void ScaleUpAndFadeInView(views::View* view, const gfx::Point& start) {
  DCHECK(view);
  const gfx::Point end = view->bounds().CenterPoint();
  ui::Layer* layer = view->layer();
  layer->SetTransform(gfx::GetScaleTransform(
      gfx::Point(start.x() - end.x(), start.y() - end.y()),
      kEnterOrExitZeroStateScale));

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);
  layer->SetTransform(kEndTransform);
  layer->SetOpacity(1.f);
}

void PositionWindowsInOverview() {
  auto* controller = Shell::Get()->overview_controller();
  DCHECK(controller->InOverviewSession());
  controller->overview_session()->PositionWindows(true);
}

// A self-deleting object that performs a fade out animation on
// |removed_mini_view|'s layer by changing its opacity from 1 to 0 and scales
// down it around the center of |bar_view| while switching back to zero state in
// Bento. |removed_mini_view_| and the object itserlf will be deleted when the
// animation is complete.
// TODO(afakhry): Consider generalizing HidingWindowAnimationObserverBase to be
// reusable for the mini_view removal animation.
class RemovedMiniViewAnimation : public ui::ImplicitAnimationObserver {
 public:
  RemovedMiniViewAnimation(DeskMiniView* removed_mini_view,
                           DesksBarView* bar_view,
                           const bool to_zero_state)
      : removed_mini_view_(removed_mini_view),
        bar_view_(bar_view),
        to_zero_state_(to_zero_state) {
    ui::Layer* layer = removed_mini_view_->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kRemovedMiniViewsFadeOutDuration);
    settings.AddObserver(this);

    if (to_zero_state_) {
      DCHECK(bar_view_);
      const gfx::Point start = bar_view->bounds().CenterPoint();
      const gfx::Point end = removed_mini_view->bounds().CenterPoint();
      layer->SetTransform(gfx::GetScaleTransform(
          gfx::Point(start.x() - end.x(), start.y() - end.y()),
          kEnterOrExitZeroStateScale));
    } else {
      layer->SetTransform(kEndTransform);
    }
    layer->SetOpacity(0);
  }

  RemovedMiniViewAnimation(const RemovedMiniViewAnimation&) = delete;
  RemovedMiniViewAnimation& operator=(const RemovedMiniViewAnimation&) = delete;

  ~RemovedMiniViewAnimation() override {
    DCHECK(removed_mini_view_->parent());
    removed_mini_view_->parent()->RemoveChildViewT(removed_mini_view_);
    if (to_zero_state_) {
      DCHECK(bar_view_);
      // Layout the desks bar to make sure the buttons visibilities and button's
      // text can be updated correctly while going back to zero state.
      bar_view_->Layout();
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  DeskMiniView* removed_mini_view_;
  DesksBarView* bar_view_;
  const bool to_zero_state_;
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
    PositionWindowsInOverview();
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

  new RemovedMiniViewAnimation(removed_mini_view, /*bar_view=*/nullptr,
                               /*to_zero_state=*/false);

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  if (features::IsBentoEnabled()) {
    AnimateView(expanded_state_new_desk_button,
                mini_views_right_begin_transform);
  }
}

void PerformZeroStateToExpandedStateMiniViewAnimation(DesksBarView* bar_view) {
  ui::Layer* layer = bar_view->background_view()->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);
  layer->SetTransform(kEndTransform);

  const gfx::Point start = bar_view->bounds().CenterPoint();
  for (auto* mini_view : bar_view->mini_views())
    ScaleUpAndFadeInView(mini_view, start);

  ScaleUpAndFadeInView(bar_view->expanded_state_new_desk_button(), start);
  PositionWindowsInOverview();
}

void PerformExpandedStateToZeroStateMiniViewAnimation(
    DesksBarView* bar_view,
    std::vector<DeskMiniView*> removed_mini_views) {
  for (auto* mini_view : removed_mini_views)
    new RemovedMiniViewAnimation(mini_view, bar_view, /*to_zero_state=*/true);

  const gfx::Rect bounds = bar_view->bounds();
  ScaleDownAndFadeOutView(bar_view->expanded_state_new_desk_button(),
                          bounds.CenterPoint());

  ui::Layer* layer = bar_view->background_view()->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);

  gfx::Transform transform;
  transform.Translate(0,
                      -(bounds.height() - DesksBarView::kZeroStateBarHeight));
  layer->SetTransform(transform);
  PositionWindowsInOverview();
}

void PerformReorderDeskMiniViewAnimation(
    int old_index,
    int new_index,
    const std::vector<DeskMiniView*>& mini_views) {
  const int views_size = static_cast<int>(mini_views.size());

  DCHECK_GE(old_index, 0);
  DCHECK_LT(old_index, views_size);
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, views_size);

  if (old_index == new_index)
    return;

  // Reordering should be finished before calling this function. The source view
  // and the target view has been exchanged. The range should be selected
  // according to current mini views position.
  const bool move_right = old_index < new_index;
  const int start_index = move_right ? old_index : new_index + 1;
  const int end_index = move_right ? new_index : old_index + 1;

  // Since |old_index| and |new_index| are unequal valid indices, there
  // must be at least two desks.
  int shift_x = mini_views[0]->GetMirroredBounds().x() -
                mini_views[1]->GetMirroredBounds().x();
  shift_x = move_right ? -shift_x : shift_x;
  gfx::Transform desks_transform;
  desks_transform.Translate(shift_x, 0);

  auto start_iter = mini_views.begin();
  AnimateMiniViews(std::vector<DeskMiniView*>(start_iter + start_index,
                                              start_iter + end_index),
                   desks_transform);

  // Animate the mini view being reordered if it is visible.
  auto* reorder_view = mini_views[new_index];
  ui::Layer* layer = reorder_view->layer();
  if (layer->opacity() == 0.0f)
    return;

  // Back to old position.
  gfx::Transform reorder_desk_transform;
  reorder_desk_transform.Translate(
      mini_views[old_index]->GetMirroredBounds().x() -
          reorder_view->GetMirroredBounds().x(),
      0);
  layer->SetTransform(reorder_desk_transform);

  // Animate movement.
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);
  layer->SetTransform(kEndTransform);
}

}  // namespace ash
