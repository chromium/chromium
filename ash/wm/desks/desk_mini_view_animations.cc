// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kMiniViewsAddingAnimationDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kMiniViewsRemovingAnimationDuration =
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

constexpr base::TimeDelta kDeskBarBoundsScaleUpDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kDeskBarBoundsScaleDownDuration =
    base::Milliseconds(150);

constexpr base::TimeDelta kZeroStateAnimationDuration = base::Milliseconds(150);

// Animation durations for scale up and scale down the desk icon button.
constexpr base::TimeDelta kScaleUpDeskIconButton = base::Milliseconds(150);
constexpr base::TimeDelta kScaleDownDeskIconButton = base::Milliseconds(50);

// Scale for entering/exiting zero state.
constexpr float kEnterOrExitZeroStateScale = 0.6f;

// Animation durations for fade in the label below the desk icon button.
constexpr base::TimeDelta kLabelFadeInDelay = base::Milliseconds(100);
constexpr base::TimeDelta kLabelFadeInDuration = base::Milliseconds(50);

// The animation duration of desk bar slide out animation when exiting
// overview mode.
constexpr base::TimeDelta kExpandedDeskBarSlideDuration =
    base::Milliseconds(350);
constexpr base::TimeDelta kZeroDeskBarSlideDuration = base::Milliseconds(250);

// `settings` will be initialized with a fast-out-slow-in animation with the
// given `duration`.
void InitScopedAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                                 base::TimeDelta duration) {
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Animates the transform of the layer of the given `view` from the supplied
// `begin_transform` to the identity transform.
void AnimateView(views::View* view,
                 const gfx::Transform& begin_transform,
                 const base::TimeDelta duration) {
  ui::Layer* layer = view->layer();
  layer->SetTransform(begin_transform);
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, duration);
  layer->SetTransform(kEndTransform);
}

// Note this function assumes that the given `view` is already set with its
// final visibility. If it's not visible, no need to fade it in. Return
// immediately instead.
void FadeInView(views::View* view,
                base::TimeDelta duration,
                base::TimeDelta delay) {
  if (!view || !view->GetVisible()) {
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
void AnimateMiniViews(
    std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views,
    const gfx::Transform& begin_transform,
    const base::TimeDelta duration) {
  for (ash::DeskMiniView* mini_view : mini_views) {
    AnimateView(mini_view, begin_transform, duration);
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

// Scales up the given `view` from `kEnterOrExitZeroStateScale` to identity and
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

// Performs the spawn animation for the given mini `view`.
void ScaleUpAndFadeInView(DeskMiniView* view) {
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

void AnimateDeskBarBounds(DeskBarViewBase* bar_view) {
  CHECK(bar_view);

  auto* desk_widget = bar_view->GetWidget();
  const gfx::Rect current_widget_bounds =
      desk_widget->GetWindowBoundsInScreen();
  gfx::Rect target_widget_bounds = current_widget_bounds;
  // While switching desk bar from zero state to expanded state, set its bounds
  // to the expanded state bounds directly without animation, which will attempt
  // to trigger layout and ensure the contents of desk bar(e.g, desk mini view,
  // new desk button) are at the correct positions before the animation. And set
  // `pause_layout_` to be true, which will avoid actually doing layout until
  // the animation is done. Then set the bounds of the desk bar back to its
  // bounds at zero state to start the bounds change animation. See more details
  // at `pause_layout_`.
  target_widget_bounds.set_height(DeskBarViewBase::GetPreferredBarHeight(
      desk_widget->GetNativeWindow()->GetRootWindow(),
      DeskBarViewBase::Type::kOverview, DeskBarViewBase::State::kExpanded));
  desk_widget->SetBounds(target_widget_bounds);
  bar_view->set_pause_layout(true);
  desk_widget->SetBounds(current_widget_bounds);

  ui::Layer* layer = desk_widget->GetLayer();
  base::OnceClosure ondone = base::BindOnce(
      base::BindOnce([](DeskBarViewBase* bar_view) {
        bar_view->set_pause_layout(false);
        // Ensure each button's visibility is accurate on desk bar state
        // changes and that each button's text is updated correctly while going
        // back to zero state.
        bar_view->UpdateDeskButtonsVisibility();
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
      .SetDuration(kZeroStateAnimationDuration)
      .SetTransform(layer, kEndTransform, gfx::Tween::ACCEL_20_DECEL_100);
  desk_widget->SetBounds(target_widget_bounds);
}

// Animates the scale up / down animation for the desk icon button.
void AnimateDeskIconButtonScale(DeskIconButton* button,
                                const gfx::Transform& scale_transform) {
  // Please note that since this is called after `button` is laid out in its
  // final position, the target state is its current state.
  const DeskIconButton::State target_state = button->state();
  const bool is_scale_up_animation =
      target_state == DeskIconButton::State::kActive;
  const gfx::RoundedCornersF initial_radius =
      gfx::RoundedCornersF(DeskIconButton::GetCornerRadiusOnState(
          is_scale_up_animation ? DeskIconButton::State::kExpanded
                                : DeskIconButton::State::kActive));

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
      DeskIconButton::GetCornerRadiusOnState(target_state));
  views::AnimationBuilder animation_builder;
  button->set_animation_abort_handle(animation_builder.GetAbortHandle());
  base::OnceClosure ondone = base::BindOnce(
      [](DeskIconButton* button) {
        const auto* overview_controller = Shell::Get()->overview_controller();
        if (overview_controller->InOverviewSession()) {
          button->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF());
          button->SetBackground(views::CreateRoundedRectBackground(
              button->background()->get_color(),
              DeskIconButton::GetCornerRadiusOnState(button->state())));
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

void PerformAddDeskMiniViewAnimation(
    std::vector<DeskMiniView*> new_mini_views) {
  for (auto* mini_view : new_mini_views) {
    if (!mini_view->desk()->is_desk_being_removed()) {
      ScaleUpAndFadeInView(mini_view);
    }
  }
}

void PerformRemoveDeskMiniViewAnimation(DeskMiniView* removed_mini_view) {
  DeskBarViewBase* bar_view = removed_mini_view->owner_bar();
  CHECK(bar_view);

  removed_mini_view->set_is_animating_to_remove(true);

  ui::Layer* layer = removed_mini_view->layer();
  const gfx::Tween::Type tween_type = gfx::Tween::ACCEL_20_DECEL_100;

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
      .SetTransform(layer, kEndTransform, tween_type)
      .SetOpacity(layer, 0.f, tween_type);
}

void PerformDeskBarChildViewShiftAnimation(
    DeskBarViewBase* bar_view,
    const base::flat_map<views::View*, int>& views_previous_x_map) {
  // Use the previous x location for each view to animate separately.
  for (auto& [view, old_x] : views_previous_x_map) {
    if (const int shift = old_x - view->GetBoundsInScreen().x()) {
      const auto begin_transform = gfx::Transform::MakeTranslation(shift, 0);
      AnimateView(view, begin_transform, kMiniViewsRemovingAnimationDuration);
    }
  }
}

void PerformDeskBarAddDeskAnimation(DeskBarViewBase* bar_view,
                                    const gfx::Rect& old_bar_bounds) {
  CHECK(bar_view->background_view());
  const gfx::Rect new_bar_bounds = bar_view->GetBoundsInScreen();
  ui::Layer* layer = bar_view->background_view()->layer();
  const gfx::Transform initial_state = gfx::TransformBetweenRects(
      gfx::RectF(new_bar_bounds), gfx::RectF(old_bar_bounds));
  views::AnimationBuilder animation_builder;
  bar_view->set_animation_abort_handle(animation_builder.GetAbortHandle());
  layer->SetTransform(initial_state);
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kDeskBarBoundsScaleUpDuration)
      .SetTransform(layer, gfx::Transform(), gfx::Tween::ACCEL_20_DECEL_100);
}

void PerformDeskBarRemoveDeskAnimation(DeskBarViewBase* bar_view,
                                       const gfx::Rect& old_background_bounds) {
  CHECK(bar_view->background_view());
  const gfx::Rect new_background_bounds =
      bar_view->background_view()->GetBoundsInScreen();
  ui::Layer* layer = bar_view->background_view()->layer();

  const gfx::Transform transform = gfx::TransformBetweenRects(
      gfx::RectF(new_background_bounds), gfx::RectF(old_background_bounds));

  base::OnceClosure ondone =
      base::BindOnce(base::BindOnce([](DeskBarViewBase* bar_view) {
                       bar_view->UpdateBarBounds();
                     }),
                     base::Unretained(bar_view));
  views::AnimationBuilder animation_builder;
  bar_view->set_animation_abort_handle(animation_builder.GetAbortHandle());
  auto split = base::SplitOnceCallback(std::move(ondone));
  layer->SetTransform(transform);
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(kDeskBarBoundsScaleDownDuration)
      .SetTransform(layer, kEndTransform, gfx::Tween::ACCEL_20_DECEL_100);
}

void PerformZeroStateToExpandedStateMiniViewAnimation(
    DeskBarViewBase* bar_view) {
  bar_view->new_desk_button()->UpdateState(DeskIconButton::State::kExpanded);
  auto* library_button = bar_view->library_button();

  if (library_button) {
    if (bar_view->overview_grid()->IsShowingSavedDeskLibrary()) {
      // For library button, when it's at zero state and clicked, the desks bar
      // will expand, the overview grid will show the saved desk library, the
      // library button should be activated and focused.
      library_button->UpdateState(DeskIconButton::State::kActive);
    } else {
      library_button->UpdateState(DeskIconButton::State::kExpanded);
    }
  }

  AnimateDeskBarBounds(bar_view);

  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (ash::DeskMiniView* mini_view : bar_view->mini_views()) {
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

  // This function should only be called in overview since there is no zero to
  // expanded state animation for the desk button bar.
  OverviewGrid* grid = bar_view->overview_grid();
  CHECK(grid);

  base::flat_set<OverviewItemBase*> ignored_items;
  if (auto* drag_controller =
          grid->overview_session()->window_drag_controller()) {
    auto* dragged_item = drag_controller->item();
    if (dragged_item && dragged_item->overview_grid() == grid) {
      ignored_items.insert(dragged_item);
    }
  }

  bar_view->overview_grid()->PositionWindows(/*animate=*/true, ignored_items);
}

void PerformReorderDeskMiniViewAnimation(
    int old_index,
    int new_index,
    const std::vector<raw_ptr<DeskMiniView, VectorExperimental>>& mini_views) {
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
  AnimateMiniViews(std::vector<raw_ptr<DeskMiniView, VectorExperimental>>(
                       start_iter + start_index, start_iter + end_index),
                   desks_transform, kMiniViewsAddingAnimationDuration);

  // Animate the mini view being reordered if it is visible.
  auto* reorder_view = mini_views[new_index].get();
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
  InitScopedAnimationSettings(&settings, kMiniViewsAddingAnimationDuration);
  layer->SetTransform(kEndTransform);
}

void PerformLibraryButtonVisibilityAnimation(
    const std::vector<raw_ptr<DeskMiniView, VectorExperimental>>& mini_views,
    views::View* new_desk_button,
    int shift_x) {
  gfx::Transform translation;
  translation.Translate(shift_x, 0);
  AnimateMiniViews(mini_views, translation, kMiniViewsAddingAnimationDuration);
  AnimateView(new_desk_button, translation, kMiniViewsAddingAnimationDuration);
}

void PerformDeskIconButtonScaleAnimation(
    DeskIconButton* button,
    DeskBarViewBase* bar_view,
    const gfx::Transform& new_desk_button_rects_transform,
    int shift_x) {
  AnimateDeskIconButtonScale(button, new_desk_button_rects_transform);

  gfx::Transform left_begin_transform;
  left_begin_transform.Translate(shift_x, 0);
  gfx::Transform right_begin_transform;
  right_begin_transform.Translate(-shift_x, 0);

  AnimateMiniViews(bar_view->mini_views(), left_begin_transform,
                   kMiniViewsAddingAnimationDuration);

  // If `button` is the new desk button, shift the library button to the right.
  // Otherwise if it's the library button, shift the new desk button to the
  // left.
  if (button == bar_view->new_desk_button()) {
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, right_begin_transform,
                  kMiniViewsAddingAnimationDuration);
      FadeInView(bar_view->new_desk_button_label(),
                 /*duration=*/kLabelFadeInDuration,
                 /*delay=*/kLabelFadeInDelay);
    }
  } else {
    AnimateView(bar_view->new_desk_button(), left_begin_transform,
                kMiniViewsAddingAnimationDuration);
    FadeInView(bar_view->library_button_label(),
               /*duration=*/kLabelFadeInDuration,
               /*delay=*/kLabelFadeInDelay);
  }
}

void PerformDeskBarSlideAnimation(std::unique_ptr<views::Widget> desks_widget,
                                  bool is_zero_state) {
  TRACE_EVENT0("ui", "PerformDeskBarSlideAnimation");

  // The desks widget should no longer process events at this point.
  PrepareWidgetForShutdownAnimation(desks_widget.get());

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
