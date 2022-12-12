// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/containers/contains.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kExistingMiniViewsAnimationDuration =
    base::Milliseconds(250);

constexpr base::TimeDelta kRemovedMiniViewsFadeOutDuration =
    base::Milliseconds(200);

constexpr base::TimeDelta kZeroStateAnimationDuration = base::Milliseconds(200);

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

// Gets the scale transform for |view|, it can be scale up or scale down. The
// anchor of the scale animation will be a point whose |x| is the center of the
// desks bar while |y| is the top of the given |view|. GetMirroredX is used here
// to make sure the transform is correct while in RTL layout.
gfx::Transform GetScaleTransformForView(views::View* view, int bar_x_center) {
  return gfx::GetScaleTransform(
      gfx::Point(bar_x_center - view->GetMirroredX(), 0),
      kEnterOrExitZeroStateScale);
}

// Scales down the given |view| to |kEnterOrExitZeroStateScale| and fading out
// it at the same time.
void ScaleDownAndFadeOutView(views::View* view, int bar_x_center) {
  ui::Layer* layer = view->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);

  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);
}

// Scales up the given |view| from |kEnterOrExitZeroStateScale| to identity and
// fading in it at the same time.
void ScaleUpAndFadeInView(views::View* view, int bar_x_center) {
  DCHECK(view);
  ui::Layer* layer = view->layer();
  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);

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

void UpdateAccessibilityFocusInOverview() {
  auto* controller = Shell::Get()->overview_controller();
  DCHECK(controller->InOverviewSession());
  controller->overview_session()->UpdateAccessibilityFocus();
}

// A self-deleting object that performs a fade out animation on
// |removed_mini_view|'s layer by changing its opacity from 1 to 0 and scales
// down it around the center of |bar_view| while switching back to zero state.
// |removed_mini_view_| and the object itself will be deleted when the
// animation is complete.
// TODO(afakhry): Consider generalizing HidingWindowAnimationObserverBase to be
// reusable for the mini_view removal animation.
class RemovedMiniViewAnimation : public ui::ImplicitAnimationObserver {
 public:
  RemovedMiniViewAnimation(DeskMiniView* removed_mini_view,
                           DesksBarView* bar_view,
                           const bool to_zero_state)
      : removed_mini_view_(removed_mini_view), bar_view_(bar_view) {
    removed_mini_view_->set_is_animating_to_remove(true);
    ui::Layer* layer = removed_mini_view_->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kRemovedMiniViewsFadeOutDuration);
    settings.AddObserver(this);

    if (to_zero_state) {
      DCHECK(bar_view);
      layer->SetTransform(GetScaleTransformForView(
          removed_mini_view, bar_view->bounds().CenterPoint().x()));
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

    if (Shell::Get()->overview_controller()->InOverviewSession()) {
      DCHECK(bar_view_);
      bar_view_->UpdateDeskButtonsVisibility();
      UpdateAccessibilityFocusInOverview();
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  DeskMiniView* removed_mini_view_;
  DesksBarView* const bar_view_;
};

// A self-deleting object that performs bounds changes animation for the desks
// bar while it switches between zero state and expanded state.
// `is_bounds_animation_on_going_` will be used to help hold Layout calls during
// the animation. Since Layout is expensive and will be called lots of times
// during the bounds changes animation without doing this. The object itself
// will be deleted when the animation is complete.
class DesksBarBoundsAnimation : public ui::ImplicitAnimationObserver {
 public:
  DesksBarBoundsAnimation(DesksBarView* bar_view, bool to_zero_state)
      : bar_view_(bar_view) {
    auto* desks_widget = bar_view_->GetWidget();
    const gfx::Rect current_widget_bounds =
        desks_widget->GetWindowBoundsInScreen();
    gfx::Rect target_widget_bounds = current_widget_bounds;

    // When `to_zero_state` is false, desks bar is switching from zero to
    // expanded state.
    if (to_zero_state) {
      target_widget_bounds.set_height(DesksBarView::kZeroStateBarHeight);
      bar_view_->set_is_bounds_animation_on_going(true);
    } else {
      // While switching desks bar from zero state to expanded state, setting
      // its bounds to its bounds at expanded state directly without animation,
      // which will trigger Layout and make sure the contents of
      // desks bar(e.g, desk mini view, new desk button) are at the correct
      // positions before the animation. And set `is_bounds_animation_on_going_`
      // to be true, which will help hold Layout until the animation is done.
      // Then set the bounds of the desks bar back to its bounds at zero state
      // to start the bounds change animation. See more details at
      // `is_bounds_animation_on_going_`.
      target_widget_bounds.set_height(bar_view_->GetExpandedBarHeight(
          desks_widget->GetNativeWindow()->GetRootWindow()));
      desks_widget->SetBounds(target_widget_bounds);
      bar_view_->set_is_bounds_animation_on_going(true);
      desks_widget->SetBounds(current_widget_bounds);
    }

    ui::ScopedLayerAnimationSettings settings{
        desks_widget->GetLayer()->GetAnimator()};
    InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);
    settings.AddObserver(this);
    desks_widget->SetBounds(target_widget_bounds);
  }

  DesksBarBoundsAnimation(const DesksBarBoundsAnimation&) = delete;
  DesksBarBoundsAnimation& operator=(const DesksBarBoundsAnimation&) = delete;

  ~DesksBarBoundsAnimation() override {
    DCHECK(bar_view_);
    bar_view_->set_is_bounds_animation_on_going(false);
    if (Shell::Get()->overview_controller()->InOverviewSession()) {
      // Updated the desk buttons and layout the desks bar to make sure the
      // buttons visibility will be updated on desks bar state changes. Also
      // make sure the button's text will be updated correctly while going back
      // to zero state.
      bar_view_->UpdateDeskButtonsVisibility();
      bar_view_->Layout();
      UpdateAccessibilityFocusInOverview();
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  DesksBarView* const bar_view_;
};

}  // namespace

void PerformNewDeskMiniViewAnimation(
    std::vector<DeskMiniView*> new_mini_views,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    ExpandedDesksBarButton* expanded_state_new_desk_button,
    ExpandedDesksBarButton* expanded_state_desks_templates_button,
    int shift_x) {
  DCHECK(expanded_state_new_desk_button);
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  for (auto* mini_view : new_mini_views) {
    ui::Layer* layer = mini_view->layer();
    layer->SetOpacity(0);

    if (!mini_view->desk()->is_desk_being_removed())
      layer->SetTransform(mini_views_left_begin_transform);

    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);
    layer->SetOpacity(1);
    layer->SetTransform(kEndTransform);
  }

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  // The new desk button and the desk templates button in the expanded desks bar
  // always move to the right when a new desk is added.
  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  AnimateView(expanded_state_new_desk_button, button_transform);
  if (expanded_state_desks_templates_button)
    AnimateView(expanded_state_desks_templates_button, button_transform);
}

void PerformRemoveDeskMiniViewAnimation(
    DesksBarView* bar_view,
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    ExpandedDesksBarButton* expanded_state_new_desk_button,
    ExpandedDesksBarButton* expanded_state_desks_templates_button,
    int shift_x) {
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  new RemovedMiniViewAnimation(removed_mini_view, bar_view,
                               /*to_zero_state=*/false);

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  AnimateView(expanded_state_new_desk_button, button_transform);
  if (expanded_state_desks_templates_button)
    AnimateView(expanded_state_desks_templates_button, button_transform);
}

void PerformZeroStateToExpandedStateMiniViewAnimation(DesksBarView* bar_view) {
  new DesksBarBoundsAnimation(bar_view, /*to_zero_state=*/false);
  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (auto* mini_view : bar_view->mini_views())
    ScaleUpAndFadeInView(mini_view, bar_x_center);

  ScaleUpAndFadeInView(bar_view->expanded_state_new_desk_button(),
                       bar_x_center);
  if (auto* expanded_state_desks_templates_button =
          bar_view->expanded_state_desks_templates_button()) {
    ScaleUpAndFadeInView(expanded_state_desks_templates_button, bar_x_center);
  }
  PositionWindowsInOverview();
}

void PerformExpandedStateToZeroStateMiniViewAnimation(
    DesksBarView* bar_view,
    std::vector<DeskMiniView*> removed_mini_views) {
  for (auto* mini_view : removed_mini_views)
    new RemovedMiniViewAnimation(mini_view, bar_view, /*to_zero_state=*/true);
  new DesksBarBoundsAnimation(bar_view, /*to_zero_state=*/true);
  const gfx::Rect bounds = bar_view->bounds();
  ScaleDownAndFadeOutView(bar_view->expanded_state_new_desk_button(),
                          bounds.CenterPoint().x());
  if (auto* expanded_state_desks_templates_button =
          bar_view->expanded_state_desks_templates_button()) {
    ScaleDownAndFadeOutView(expanded_state_desks_templates_button,
                            bounds.CenterPoint().x());
  }

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

void PerformLibraryButtonVisibilityAnimation(
    const std::vector<DeskMiniView*>& mini_views,
    views::View* new_desk_button,
    int shift_x) {
  gfx::Transform translation;
  translation.Translate(shift_x, 0);
  AnimateMiniViews(mini_views, translation);
  AnimateView(new_desk_button, translation);
}

}  // namespace ash
