// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kExistingMiniViewsAnimationDuration =
    base::Milliseconds(250);
constexpr base::TimeDelta kExistingMiniViewsAnimationDurationCrOSNext =
    base::Milliseconds(150);

constexpr base::TimeDelta kNewMiniViewsAnimationDelayDuration =
    base::Milliseconds(50);
constexpr base::TimeDelta kNewMiniViewsAnimationFadeDelayDuration =
    base::Milliseconds(100);

constexpr base::TimeDelta kNewMiniViewsScaleAnimationDuration =
    base::Milliseconds(150);
constexpr base::TimeDelta kNewMiniViewsFadeInAnimationDuration =
    base::Milliseconds(100);

constexpr base::TimeDelta kRemovedMiniViewsFadeOutDuration =
    base::Milliseconds(200);

constexpr base::TimeDelta kZeroStateAnimationDuration = base::Milliseconds(200);

// Animation duration when feature flag `Jellyroll` is enabled.
constexpr base::TimeDelta kZeroStateAnimationDurationCrOSNext =
    base::Milliseconds(150);

// Animation durations for scale up and scale down the desk icon button.
constexpr base::TimeDelta kScaleUpDeskIconButton = base::Milliseconds(150);
constexpr base::TimeDelta kScaleDownDeskIconButton = base::Milliseconds(50);

// Scale for entering/exiting zero state.
constexpr float kEnterOrExitZeroStateScale = 0.6f;

// Animation durations for fade in the label below the desk icon button.
constexpr base::TimeDelta kLabelFadeInDelay = base::Milliseconds(100);
constexpr base::TimeDelta kLabelFadeInDuration = base::Milliseconds(50);

// The animiation duration of desk bar slide out animation when exiting
// overview mode.
constexpr base::TimeDelta kExpandedDeskBarSlideDuration =
    base::Milliseconds(350);
constexpr base::TimeDelta kZeroDeskBarSlideDuration = base::Milliseconds(250);

// `settings` will be initialized with a fast-out-slow-in animation with the
// given `duration`.
void InitScopedAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                                 base::TimeDelta duration) {
  settings->SetTransitionDuration(duration);
  const gfx::Tween::Type tween_type = chromeos::features::IsJellyrollEnabled()
                                          ? gfx::Tween::ACCEL_20_DECEL_100
                                          : gfx::Tween::ACCEL_20_DECEL_60;
  settings->SetTweenType(tween_type);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Animates the transform of the layer of the given `view` from the supplied
// `begin_transform` to the identity transform.
void AnimateView(views::View* view, const gfx::Transform& begin_transform) {
  ui::Layer* layer = view->layer();
  layer->SetTransform(begin_transform);

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings,
                              chromeos::features::IsJellyrollEnabled()
                                  ? kExistingMiniViewsAnimationDurationCrOSNext
                                  : kExistingMiniViewsAnimationDuration);
  layer->SetTransform(kEndTransform);
}

// Note this function assumes that the given `view` is already set with its
// final visibility. If it's not visible, no need to fade it in. Return
// immediately instead.
void FadeInView(views::View* view,
                base::TimeDelta duration,
                base::TimeDelta delay) {
  if (!view->GetVisible()) {
    return;
  }

  auto* layer = view->layer();
  layer->SetOpacity(0.f);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .At(delay)
      .SetDuration(duration)
      .SetOpacity(layer, 1.f, gfx::Tween::ACCEL_20_DECEL_100);
}

// See details at AnimateView.
void AnimateMiniViews(std::vector<DeskMiniView*> mini_views,
                      const gfx::Transform& begin_transform) {
  for (auto* mini_view : mini_views) {
    AnimateView(mini_view, begin_transform);
  }
}

// Gets the scale transform for `view`, it can be scale up or scale down. The
// anchor of the scale animation will be a point whose `x` is the center of the
// desk bar while `y` is the top of the given `view`. GetMirroredX is used here
// to make sure the transform is correct while in RTL layout.
gfx::Transform GetScaleTransformForView(views::View* view, int bar_x_center) {
  return gfx::GetScaleTransform(
      gfx::Point(bar_x_center - view->GetMirroredX(), 0),
      kEnterOrExitZeroStateScale);
}

// Scales down the given `view` to `kEnterOrExitZeroStateScale` and fading out
// it at the same time.
void ScaleDownAndFadeOutView(views::View* view, int bar_x_center) {
  ui::Layer* layer = view->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);

  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);
}

// Scales up the given `view` from `kEnterOrExitZeroStateScale` to identity and
// fading in it at the same time.
void ScaleUpAndFadeInView(views::View* view, int bar_x_center) {
  DCHECK(view);
  ui::Layer* layer = view->layer();
  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  const base::TimeDelta animation_duration =
      chromeos::features::IsJellyrollEnabled()
          ? kZeroStateAnimationDurationCrOSNext
          : kZeroStateAnimationDuration;
  InitScopedAnimationSettings(&settings, animation_duration);
  layer->SetTransform(kEndTransform);
  layer->SetOpacity(1.f);
}

// Performs the CrOS Next spawn animation for the given mini `view`.
void CrOSNextScaleUpAndFadeInView(DeskMiniView* view) {
  ui::Layer* preview_layer = view->desk_preview()->layer();
  ui::Layer* view_layer = view->layer();

  // Minimize the view to top center point.
  const gfx::Transform initial_state =
      gfx::GetScaleTransform(view->GetLocalBounds().top_center(), 0.1f);

  // Hide the view before scale up animation starts.
  view_layer->SetOpacity(0.f);
  preview_layer->SetTransform(initial_state);

  // Uses animation builder so that we can use `views::AnimationAbortHandle`.
  // Setting abort handle is important as it manages to abort ongoing
  // animation as documented in `DeskMiniView::animation_abort_handle_`.
  views::AnimationBuilder animation_builder;
  view->set_animation_abort_handle(animation_builder.GetAbortHandle());

  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .At(kNewMiniViewsAnimationFadeDelayDuration)
      .SetDuration(kNewMiniViewsFadeInAnimationDuration)
      .SetOpacity(view_layer, 1.f)
      .At(kNewMiniViewsAnimationDelayDuration)
      .SetDuration(kNewMiniViewsScaleAnimationDuration)
      .SetTransform(preview_layer, gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100);
}

void PositionWindowsInOverview() {
  auto* controller = Shell::Get()->overview_controller();
  DCHECK(controller->InOverviewSession());
  controller->overview_session()->PositionWindows(true);
}

// Performs a fade out animation on `removed_mini_view`'s layer by changing its
// opacity from 1 to 0 and scales down it around the center of `bar_view` while
// switching back to zero state. `removed_mini_view_` will be deleted when the
// animation is complete or aborted.
void AnimateDeskMiniViewRemove(DeskMiniView* removed_mini_view,
                               const bool to_zero_state) {
  DeskBarViewBase* bar_view = removed_mini_view->owner_bar();
  CHECK(bar_view);

  removed_mini_view->set_is_animating_to_remove(true);

  ui::Layer* layer = removed_mini_view->layer();
  const gfx::Transform transform =
      to_zero_state
          ? GetScaleTransformForView(removed_mini_view,
                                     bar_view->bounds().CenterPoint().x())
          : kEndTransform;
  const gfx::Tween::Type tween_type = chromeos::features::IsJellyrollEnabled()
                                          ? gfx::Tween::ACCEL_20_DECEL_100
                                          : gfx::Tween::ACCEL_20_DECEL_60;

  // Uses animation builder so that we can use `views::AnimationAbortHandle`.
  // Setting abort handle is important as it manages to abort ongoing
  // animation as documented in `DeskMiniView::animation_abort_handle_`.
  views::AnimationBuilder animation_builder;
  removed_mini_view->set_animation_abort_handle(
      animation_builder.GetAbortHandle());

  // This callback is designed to destroy the mini view instance only if it is
  // still in the view tree, meaning that the mini view has not yet been
  // destroyed as a result of the destruction of the entire desk bar.
  base::OnceClosure ondone = base::BindOnce(
      [](DeskMiniView* mini_view) {
        // Does nothing if the whole bar is destructing.
        views::View* parent = mini_view->parent();
        if (!parent) {
          return;
        }

        std::unique_ptr<DeskMiniView> to_delete =
            parent->RemoveChildViewT(mini_view);
        mini_view->owner_bar()->UpdateDeskButtonsVisibility();

        auto* overview_controller = Shell::Get()->overview_controller();
        if (mini_view->owner_bar()->type() ==
                DeskBarViewBase::Type::kOverview &&
            overview_controller->InOverviewSession()) {
          overview_controller->overview_session()->UpdateAccessibilityFocus();
        }
      },
      base::Unretained(removed_mini_view));

  auto split = base::SplitOnceCallback(std::move(ondone));
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(kRemovedMiniViewsFadeOutDuration)
      .SetTransform(layer, transform, tween_type)
      .SetOpacity(layer, 0.f, tween_type);
}

void AnimateDeskBarBounds(DeskBarViewBase* bar_view, bool to_zero_state) {
  CHECK(bar_view);

  auto* desk_widget = bar_view->GetWidget();
  const gfx::Rect current_widget_bounds =
      desk_widget->GetWindowBoundsInScreen();
  gfx::Rect target_widget_bounds = current_widget_bounds;
  // When `to_zero_state` is false, desk bar is switching from zero to
  // expanded state.
  if (to_zero_state) {
    target_widget_bounds.set_height(kDeskBarZeroStateHeight);

    if (chromeos::features::IsJellyrollEnabled()) {
      // When `Jellyroll` is enabled, setting desk bar's bounds to its bounds at
      // zero state directly to layout its contents at the correct position
      // first before the animation. When `Jellyroll` is enabled, we use the
      // same buttons (default desk button and library) for both expanded state
      // and zero state, the scale up and fade in animation is applied to the
      // button during the desk bar states transition, thus the buttons need to
      // be layout and put at the correct positions before the animation starts.
      desk_widget->SetBounds(target_widget_bounds);
      bar_view->set_is_bounds_animation_on_going(true);
      desk_widget->SetBounds(current_widget_bounds);
    } else {
      bar_view->set_is_bounds_animation_on_going(true);
    }
  } else {
    // While switching desk bar from zero state to expanded state, setting
    // its bounds to its bounds at expanded state directly without animation,
    // which will trigger Layout and make sure the contents of
    // desk bar(e.g, desk mini view, new desk button) are at the correct
    // positions before the animation. And set `is_bounds_animation_on_going_`
    // to be true, which will help hold Layout until the animation is done.
    // Then set the bounds of the desk bar back to its bounds at zero state
    // to start the bounds change animation. See more details at
    // `is_bounds_animation_on_going_`.
    target_widget_bounds.set_height(DeskBarViewBase::GetPreferredBarHeight(
        desk_widget->GetNativeWindow()->GetRootWindow(),
        DeskBarViewBase::Type::kOverview, DeskBarViewBase::State::kExpanded));
    desk_widget->SetBounds(target_widget_bounds);
    bar_view->set_is_bounds_animation_on_going(true);
    desk_widget->SetBounds(current_widget_bounds);
  }

  ui::Layer* layer = desk_widget->GetLayer();
  const base::TimeDelta animation_duration =
      chromeos::features::IsJellyrollEnabled()
          ? kZeroStateAnimationDurationCrOSNext
          : kZeroStateAnimationDuration;
  const gfx::Tween::Type tween_type = chromeos::features::IsJellyrollEnabled()
                                          ? gfx::Tween::ACCEL_20_DECEL_100
                                          : gfx::Tween::ACCEL_20_DECEL_60;
  base::OnceClosure ondone = base::BindOnce(
      base::BindOnce([](DeskBarViewBase* bar_view) {
        bar_view->set_is_bounds_animation_on_going(false);

        // Updated the desk buttons and layout the desk bar to make sure the
        // buttons visibility will be updated on desk bar state changes. Also
        // make sure the button's text will be updated correctly while going
        // back to zero state.
        bar_view->UpdateDeskButtonsVisibility();
        bar_view->Layout();
        if (OverviewController* overview_controller =
                Shell::Get()->overview_controller()) {
          if (overview_controller->InOverviewSession()) {
            overview_controller->overview_session()->UpdateAccessibilityFocus();
          }
        }

        bar_view->OnUiUpdateDone();
      }),
      base::Unretained(bar_view));
  views::AnimationBuilder animation_builder;
  bar_view->set_animation_abort_handle(animation_builder.GetAbortHandle());
  auto split = base::SplitOnceCallback(std::move(ondone));
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(animation_duration)
      .SetTransform(layer, kEndTransform, tween_type);
  desk_widget->SetBounds(target_widget_bounds);
}

// Animates the scale up / down animation for the cros next desk icon button.
void AnimateCrOSNextDeskIconButtonScale(CrOSNextDeskIconButton* button,
                                        const gfx::Transform& scale_transform) {
  // Please note that since this is called after `button` is laid out in its
  // final position, the target state is its current state.
  const CrOSNextDeskIconButton::State target_state = button->state();
  const bool is_scale_up_animation =
      target_state == CrOSNextDeskIconButton::State::kActive;
  const gfx::RoundedCornersF initial_radius =
      gfx::RoundedCornersF(CrOSNextDeskIconButton::GetCornerRadiusOnState(
          is_scale_up_animation ? CrOSNextDeskIconButton::State::kExpanded
                                : CrOSNextDeskIconButton::State::kActive));

  // Since the corner radius of `button` is updated on the state changes, to
  // apply the animation for the corner radius change, set and apply the corner
  // radius animation on the layer, and set the solid background (no corner
  // radius) to the new desk button in the meanwhile. At the end of the
  // animation, set the layer's corner radius back to 0, and apply the corner
  // radius back to the background. The reason is that the focus ring is painted
  // on a layer which is a child of `button`'s layer. If `button` has a clip
  // rect, the clip rect will affect it's children and the focus ring won't be
  // visible. Please refer to the `Layout` function of `FocusRing` for more
  // implementation details.
  auto* layer = button->layer();
  layer->SetRoundedCornerRadius(initial_radius);
  button->SetBackground(
      views::CreateSolidBackground(button->background()->get_color()));

  layer->SetTransform(scale_transform);

  const auto duration =
      is_scale_up_animation ? kScaleUpDeskIconButton : kScaleDownDeskIconButton;
  const gfx::RoundedCornersF end_radius = gfx::RoundedCornersF(
      CrOSNextDeskIconButton::GetCornerRadiusOnState(target_state));
  views::AnimationBuilder animation_builder;
  button->set_animation_abort_handle(animation_builder.GetAbortHandle());
  base::OnceClosure ondone = base::BindOnce(
      [](CrOSNextDeskIconButton* button) {
        const auto* overview_controller = Shell::Get()->overview_controller();
        if (overview_controller->InOverviewSession()) {
          button->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF());
          button->SetBackground(views::CreateRoundedRectBackground(
              button->background()->get_color(),
              CrOSNextDeskIconButton::GetCornerRadiusOnState(button->state())));
        }
      },
      base::Unretained(button));
  auto split = base::SplitOnceCallback(std::move(ondone));
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(duration)
      .SetRoundedCorners(layer, end_radius, gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(layer, kEndTransform, gfx::Tween::ACCEL_20_DECEL_100);
}

}  // namespace

void PerformNewDeskMiniViewAnimation(
    DeskBarViewBase* bar_view,
    std::vector<DeskMiniView*> new_mini_views,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x) {
  if (chromeos::features::IsJellyrollEnabled()) {
    DCHECK(bar_view->new_desk_button());
  } else {
    DCHECK(bar_view->expanded_state_new_desk_button());
  }

  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  for (auto* mini_view : new_mini_views) {
    if (chromeos::features::IsJellyrollEnabled()) {
      if (!mini_view->desk()->is_desk_being_removed()) {
        CrOSNextScaleUpAndFadeInView(mini_view);
      }
    } else {
      ui::Layer* layer = mini_view->layer();
      layer->SetOpacity(0);

      if (!mini_view->desk()->is_desk_being_removed()) {
        layer->SetTransform(mini_views_left_begin_transform);
      }

      ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
      InitScopedAnimationSettings(&settings,
                                  kExistingMiniViewsAnimationDuration);
      layer->SetOpacity(1);
      layer->SetTransform(kEndTransform);
    }
  }

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  // The new desk button and the library button in the expanded desk bar
  // always move to the right when a new desk is added.
  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  if (chromeos::features::IsJellyrollEnabled()) {
    AnimateView(bar_view->new_desk_button(), button_transform);
    if (bar_view->new_desk_button_label()->GetVisible()) {
      AnimateView(bar_view->new_desk_button_label(), button_transform);
    }
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, button_transform);
      if (bar_view->library_button_label()->GetVisible()) {
        AnimateView(bar_view->library_button_label(), button_transform);
      }
    }
  } else {
    AnimateView(bar_view->expanded_state_new_desk_button(), button_transform);
    if (auto* expanded_state_library_button =
            bar_view->expanded_state_library_button()) {
      AnimateView(expanded_state_library_button, button_transform);
    }
  }
}

void PerformRemoveDeskMiniViewAnimation(
    DeskBarViewBase* bar_view,
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x) {
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  AnimateDeskMiniViewRemove(removed_mini_view,
                            /*to_zero_state=*/false);

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  if (chromeos::features::IsJellyrollEnabled()) {
    AnimateView(bar_view->new_desk_button(), button_transform);
    if (bar_view->new_desk_button_label()->GetVisible()) {
      AnimateView(bar_view->new_desk_button_label(), button_transform);
    }
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, button_transform);
      if (bar_view->library_button_label()->GetVisible()) {
        AnimateView(bar_view->library_button_label(), button_transform);
      }
    }
  } else {
    AnimateView(bar_view->expanded_state_new_desk_button(), button_transform);
    if (auto* expanded_state_library_button =
            bar_view->expanded_state_library_button()) {
      AnimateView(expanded_state_library_button, button_transform);
    }
  }
}

void PerformZeroStateToExpandedStateMiniViewAnimation(
    DeskBarViewBase* bar_view) {
  AnimateDeskBarBounds(bar_view, /*to_zero_state=*/false);
  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (auto* mini_view : bar_view->mini_views())
    ScaleUpAndFadeInView(mini_view, bar_x_center);

  ScaleUpAndFadeInView(bar_view->expanded_state_new_desk_button(),
                       bar_x_center);
  if (auto* expanded_state_library_button =
          bar_view->expanded_state_library_button()) {
    ScaleUpAndFadeInView(expanded_state_library_button, bar_x_center);
  }
  PositionWindowsInOverview();
}

void PerformZeroStateToExpandedStateMiniViewAnimationCrOSNext(
    DeskBarViewBase* bar_view) {
  bar_view->new_desk_button()->UpdateState(
      CrOSNextDeskIconButton::State::kExpanded);
  auto* library_button = bar_view->library_button();

  if (library_button) {
    if (bar_view->overview_grid()->IsShowingSavedDeskLibrary()) {
      // For library button, when it's at zero state and clicked, the desks bar
      // will expand, the overview grid will show the saved desk library, the
      // library button should be activated and focused.
      library_button->UpdateState(CrOSNextDeskIconButton::State::kActive);
    } else {
      library_button->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
    }
  }

  AnimateDeskBarBounds(bar_view, /*to_zero_state=*/false);

  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (auto* mini_view : bar_view->mini_views()) {
    ScaleUpAndFadeInView(mini_view, bar_x_center);
  }

  ScaleUpAndFadeInView(bar_view->new_desk_button(), bar_x_center);
  if (library_button) {
    ScaleUpAndFadeInView(library_button, bar_x_center);
    // Library button could be at active state when the desk bar is switched
    // from the zero state to the expanded state, for example when clicking on
    // the library button at zero state. Thus we should also try to fade in the
    // library button label here.
    FadeInView(bar_view->library_button_label(),
               /*duration=*/kLabelFadeInDuration,
               /*delay=*/kLabelFadeInDelay);
  }

  PositionWindowsInOverview();
}

void PerformExpandedStateToZeroStateMiniViewAnimation(
    DeskBarViewBase* bar_view,
    std::vector<DeskMiniView*> removed_mini_views) {
  for (auto* mini_view : removed_mini_views) {
    AnimateDeskMiniViewRemove(mini_view, /*to_zero_state=*/true);
  }
  AnimateDeskBarBounds(bar_view, /*to_zero_state=*/true);
  const gfx::Rect bounds = bar_view->bounds();
  ScaleDownAndFadeOutView(bar_view->expanded_state_new_desk_button(),
                          bounds.CenterPoint().x());
  if (auto* expanded_state_library_button =
          bar_view->expanded_state_library_button()) {
    ScaleDownAndFadeOutView(expanded_state_library_button,
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

  // Since `old_index` and `new_index` are unequal valid indices, there
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

void PerformDeskIconButtonScaleAnimationCrOSNext(
    CrOSNextDeskIconButton* button,
    DeskBarViewBase* bar_view,
    const gfx::Transform& new_desk_button_rects_transform,
    int shift_x) {
  AnimateCrOSNextDeskIconButtonScale(button, new_desk_button_rects_transform);

  gfx::Transform left_begin_transform;
  left_begin_transform.Translate(shift_x, 0);
  gfx::Transform right_begin_transform;
  right_begin_transform.Translate(-shift_x, 0);

  AnimateMiniViews(bar_view->mini_views(), left_begin_transform);

  // If `button` is the new desk button, shift the library button to the right.
  // Otherwise if it's the library button, shift the new desk button to the
  // left.
  if (button == bar_view->new_desk_button()) {
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, right_begin_transform);
      FadeInView(bar_view->new_desk_button_label(),
                 /*duration=*/kLabelFadeInDuration,
                 /*delay=*/kLabelFadeInDelay);
    }
  } else {
    AnimateView(bar_view->new_desk_button(), left_begin_transform);
    FadeInView(bar_view->library_button_label(),
               /*duration=*/kLabelFadeInDuration,
               /*delay=*/kLabelFadeInDelay);
  }
}

void PerformDeskBarSlideAnimation(std::unique_ptr<views::Widget> desks_widget,
                                  bool is_zero_state) {
  TRACE_EVENT0("ui", "PerformDeskBarSlideAnimation");

  // The desks widget should no longer process events at this point.
  desks_widget->SetVisibilityChangedAnimationsEnabled(false);
  desks_widget->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
  desks_widget->widget_delegate()->SetCanActivate(false);

  gfx::Transform transform;
  transform.Translate(0, -desks_widget->GetWindowBoundsInScreen().height());

  // Complete any on going animations before starting this one.
  ui::Layer* layer = desks_widget->GetLayer();
  layer->CompleteAllAnimations();

  // `CleanupAnimationObserver` ownership is passed to the overview controller
  // which has a longer lifetime so animations can continue even after the
  // overview session is destroyed. The observer owns the widget and will be
  // deleted with overview controller, or when the animation is completed.
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  auto exit_observer =
      std::make_unique<CleanupAnimationObserver>(std::move(desks_widget));
  settings.AddObserver(exit_observer.get());
  settings.SetTransitionDuration(is_zero_state ? kZeroDeskBarSlideDuration
                                               : kExpandedDeskBarSlideDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
  Shell::Get()->overview_controller()->AddExitAnimationObserver(
      std::move(exit_observer));
  layer->SetTransform(transform);
}

}  // namespace ash
