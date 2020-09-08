// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {

namespace {

// The animation speed at which the highlights fade in or out.
constexpr base::TimeDelta kHighlightsFadeInOut =
    base::TimeDelta::FromMilliseconds(250);
// The animation speed which the other highlight fades in or out.
constexpr base::TimeDelta kOtherFadeInOut =
    base::TimeDelta::FromMilliseconds(133);
// The delay before the other highlight starts fading in.
constexpr base::TimeDelta kOtherFadeInDelay =
    base::TimeDelta::FromMilliseconds(117);
// The animation speed at which the preview area fades out (when you snap a
// window).
constexpr base::TimeDelta kPreviewAreaFadeOut =
    base::TimeDelta::FromMilliseconds(67);
// The time duration for the indicator label opacity animations.
constexpr base::TimeDelta kLabelAnimation =
    base::TimeDelta::FromMilliseconds(83);
// The delay before the indicator labels start fading in.
constexpr base::TimeDelta kLabelAnimationDelay =
    base::TimeDelta::FromMilliseconds(167);

// Toast data.
constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";
constexpr int kAppCannotSnapToastDurationMs = 2500;

// Gets the duration, tween type and delay before animation based on |type|.
void GetAnimationValuesForType(
    SplitviewAnimationType type,
    base::TimeDelta* out_duration,
    gfx::Tween::Type* out_tween_type,
    ui::LayerAnimator::PreemptionStrategy* out_preemption_strategy,
    base::TimeDelta* out_delay) {
  *out_preemption_strategy = ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
      *out_duration = kHighlightsFadeInOut;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
      *out_delay = kOtherFadeInDelay;
      *out_duration = kOtherFadeInOut;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
      *out_duration = kOtherFadeInOut;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
      *out_duration = kPreviewAreaFadeOut;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
      *out_delay = kLabelAnimationDelay;
      *out_duration = kLabelAnimation;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
      *out_duration = kLabelAnimation;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      *out_duration = kSplitviewWindowTransformDuration;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      *out_preemption_strategy =
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
      return;
  }

  NOTREACHED();
}

// Helper function to apply animation values to |settings|.
void ApplyAnimationSettings(
    ui::ScopedLayerAnimationSettings* settings,
    ui::LayerAnimator* animator,
    ui::LayerAnimationElement::AnimatableProperties animated_property,
    base::TimeDelta duration,
    gfx::Tween::Type tween,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    base::TimeDelta delay) {
  DCHECK_EQ(settings->GetAnimator(), animator);
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(tween);
  settings->SetPreemptionStrategy(preemption_strategy);
  if (!delay.is_zero())
    animator->SchedulePauseForProperties(delay, animated_property);
}

// Returns BubbleDialogDelegateView if |transient_window| is a bubble dialog.
views::BubbleDialogDelegate* AsBubbleDialogDelegate(
    aura::Window* transient_window) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(transient_window);
  if (!widget || !widget->widget_delegate())
    return nullptr;
  return widget->widget_delegate()->AsBubbleDialogDelegate();
}

}  // namespace

WindowTransformAnimationObserver::WindowTransformAnimationObserver(
    aura::Window* window)
    : window_(window) {
  window_->AddObserver(this);
}

WindowTransformAnimationObserver::~WindowTransformAnimationObserver() {
  if (window_)
    window_->RemoveObserver(this);
}

void WindowTransformAnimationObserver::OnImplicitAnimationsCompleted() {
  // After window transform animation is done and if the window's transform is
  // set to identity transform, force to relayout all its transient bubble
  // dialogs.
  if (!window_->layer()->GetTargetTransform().IsIdentity()) {
    delete this;
    return;
  }

  for (auto* transient_window :
       ::wm::TransientWindowManager::GetOrCreate(window_)
           ->transient_children()) {
    // For now we only care about bubble dialog type transient children.
    views::BubbleDialogDelegate* bubble_delegate_view =
        AsBubbleDialogDelegate(transient_window);
    if (bubble_delegate_view)
      bubble_delegate_view->OnAnchorBoundsChanged();
  }

  delete this;
}

void WindowTransformAnimationObserver::OnWindowDestroying(
    aura::Window* window) {
  delete this;
}

void DoSplitviewOpacityAnimation(ui::Layer* layer,
                                 SplitviewAnimationType type) {
  float target_opacity = 0.f;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
      target_opacity = 0.f;
      break;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
      target_opacity = kPreviewAreaHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
      target_opacity = kHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
      target_opacity = 1.f;
      break;
    default:
      NOTREACHED() << "Not a valid split view opacity animation type.";
      return;
  }

  if (layer->GetTargetOpacity() == target_opacity)
    return;

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::OPACITY, duration, tween,
                         preemption_strategy, delay);
  layer->SetOpacity(target_opacity);
}

void DoSplitviewTransformAnimation(
    ui::Layer* layer,
    SplitviewAnimationType type,
    const gfx::Transform& target_transform,
    std::unique_ptr<ui::ImplicitAnimationObserver> animation_observer) {
  if (layer->GetTargetTransform() == target_transform)
    return;

  switch (type) {
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      break;
    default:
      NOTREACHED() << "Not a valid split view transform type.";
      return;
  }

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  if (animation_observer.get())
    settings.AddObserver(animation_observer.release());
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::TRANSFORM, duration, tween,
                         preemption_strategy, delay);
  layer->SetTransform(target_transform);
}

void DoSplitviewClipRectAnimation(
    ui::Layer* layer,
    SplitviewAnimationType type,
    const gfx::Rect& target_clip_rect,
    std::unique_ptr<ui::ImplicitAnimationObserver> animation_observer) {
  ui::LayerAnimator* animator = layer->GetAnimator();
  if (animator->GetTargetClipRect() == target_clip_rect)
    return;

  switch (type) {
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
      break;
    default:
      NOTREACHED() << "Not a valid split view clip rect type.";
      return;
  }

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::ScopedLayerAnimationSettings settings(animator);
  if (animation_observer.get())
    settings.AddObserver(animation_observer.release());
  ApplyAnimationSettings(&settings, animator, ui::LayerAnimationElement::CLIP,
                         duration, tween, preemption_strategy, delay);
  layer->SetClipRect(target_clip_rect);
}

void MaybeRestoreSplitView(bool refresh_snapped_windows) {
  if (!ShouldAllowSplitView() ||
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    return;
  }

  // Search for snapped windows to detect if the now active user session, or
  // desk were in split view. In case multiple windows were snapped to one side,
  // one window after another, there may be multiple windows in a LEFT_SNAPPED
  // state or multiple windows in a RIGHT_SNAPPED state. For each of those two
  // state types that belongs to multiple windows, the relevant window will be
  // listed first among those windows, and a null check in the loop body below
  // will filter out the rest of them.
  // TODO(amusbach): The windows that were in split view may have later been
  // destroyed or changed to non-snapped states. Then the following for loop
  // could snap windows that were not in split view. Also, a window may have
  // become full screen, and if so, then it would be better not to reactivate
  // split view. See https://crbug.com/944134.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  if (refresh_snapped_windows) {
    const MruWindowTracker::WindowList windows =
        Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
            kActiveDesk);
    for (aura::Window* window : windows) {
      if (!split_view_controller->CanSnapWindow(window)) {
        // Since we are in tablet mode, and this window is not snappable, we
        // should maximize it.
        WindowState::Get(window)->Maximize();
        continue;
      }

      switch (WindowState::Get(window)->GetStateType()) {
        case WindowStateType::kLeftSnapped:
          if (!split_view_controller->left_window()) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::LEFT);
          }
          break;

        case WindowStateType::kRightSnapped:
          if (!split_view_controller->right_window()) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::RIGHT);
          }
          break;

        default:
          break;
      }

      if (split_view_controller->state() ==
          SplitViewController::State::kBothSnapped)
        break;
    }
  }

  // Ensure that overview mode is active if and only if there is a window
  // snapped to one side but no window snapped to the other side.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController::State state = split_view_controller->state();
  if (state == SplitViewController::State::kLeftSnapped ||
      state == SplitViewController::State::kRightSnapped) {
    overview_controller->StartOverview();
  } else {
    overview_controller->EndOverview();
  }
}

bool IsClamshellSplitViewModeEnabled() {
  return base::FeatureList::IsEnabled(features::kDragToSnapInClamshellMode);
}

bool AreMultiDisplayOverviewAndSplitViewEnabled() {
  return base::FeatureList::IsEnabled(
      features::kMultiDisplayOverviewAndSplitView);
}

bool ShouldAllowSplitView() {
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !IsClamshellSplitViewModeEnabled()) {
    return false;
  }

  // Don't allow split view if we're in pinned mode.
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    return false;

  // TODO(crubg.com/853588): Disallow window dragging and split screen while
  // ChromeVox is on until they are in a usable state.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
    return false;

  return true;
}

void ShowAppCannotSnapToast() {
  Shell::Get()->toast_manager()->Show(ToastData(
      kAppCannotSnapToastId,
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP),
      kAppCannotSnapToastDurationMs, base::Optional<base::string16>()));
}

SplitViewController::SnapPosition GetSnapPositionForLocation(
    aura::Window* root_window,
    const gfx::Point& location_in_screen,
    const base::Optional<gfx::Point>& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset) {
  if (!ShouldAllowSplitView())
    return SplitViewController::NONE;

  const bool horizontal = SplitViewController::IsLayoutHorizontal();
  const bool right_side_up = SplitViewController::IsLayoutRightSideUp();

  // Check to see if the current event location |location_in_screen| is within
  // the drag indicators bounds.
  const gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window));
  SplitViewController::SnapPosition snap_position = SplitViewController::NONE;
  if (horizontal) {
    gfx::Rect area(work_area);
    area.Inset(horizontal_edge_inset, 0);
    if (location_in_screen.x() <= area.x()) {
      snap_position = right_side_up ? SplitViewController::LEFT
                                    : SplitViewController::RIGHT;
    } else if (location_in_screen.x() >= area.right() - 1) {
      snap_position = right_side_up ? SplitViewController::RIGHT
                                    : SplitViewController::LEFT;
    }
  } else {
    gfx::Rect area(work_area);
    area.Inset(0, vertical_edge_inset);
    if (location_in_screen.y() <= area.y()) {
      snap_position = right_side_up ? SplitViewController::LEFT
                                    : SplitViewController::RIGHT;
    } else if (location_in_screen.y() >= area.bottom() - 1) {
      snap_position = right_side_up ? SplitViewController::RIGHT
                                    : SplitViewController::LEFT;
    }
  }

  if (snap_position == SplitViewController::NONE)
    return snap_position;

  // To avoid accidental snap, the window needs to be dragged inside
  // |snap_distance_from_edge| from edge or dragged toward the edge for at least
  // |minimum_drag_distance| until it's dragged into |horizontal_edge_inset| or
  // |vertical_edge_inset| region.
  // The window should always be snapped if inside |snap_distance_from_edge|
  // from edge.
  bool drag_end_near_edge = false;
  gfx::Rect area(work_area);
  area.Inset(snap_distance_from_edge, snap_distance_from_edge);
  if (horizontal ? location_in_screen.x() < area.x() ||
                       location_in_screen.x() > area.right()
                 : location_in_screen.y() < area.y() ||
                       location_in_screen.y() > area.bottom()) {
    drag_end_near_edge = true;
  }

  if (!drag_end_near_edge && initial_location_in_screen) {
    // Check how far the window has been dragged.
    const auto distance = location_in_screen - *initial_location_in_screen;
    const int primary_axis_distance = horizontal ? distance.x() : distance.y();
    const bool is_left_or_top =
        SplitViewController::IsPhysicalLeftOrTop(snap_position);
    if ((is_left_or_top && primary_axis_distance > -minimum_drag_distance) ||
        (!is_left_or_top && primary_axis_distance < minimum_drag_distance)) {
      snap_position = SplitViewController::NONE;
    }
  }

  return snap_position;
}

SplitViewController::SnapPosition GetSnapPosition(
    aura::Window* root_window,
    aura::Window* window,
    const gfx::Point& location_in_screen,
    const gfx::Point& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset) {
  if (!SplitViewController::Get(root_window)->CanSnapWindow(window)) {
    return SplitViewController::NONE;
  }

  base::Optional<gfx::Point> initial_location_in_current_screen = base::nullopt;
  if (window->GetRootWindow() == root_window)
    initial_location_in_current_screen = initial_location_in_screen;

  return GetSnapPositionForLocation(
      root_window, location_in_screen, initial_location_in_current_screen,
      snap_distance_from_edge, minimum_drag_distance, horizontal_edge_inset,
      vertical_edge_inset);
}

}  // namespace ash
