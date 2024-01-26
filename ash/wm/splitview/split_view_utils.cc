// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// The animation speed at which the highlights fade in or out.
constexpr base::TimeDelta kHighlightsFadeInOut = base::Milliseconds(250);
// The animation speed which the other highlight fades in or out.
constexpr base::TimeDelta kOtherFadeInOut = base::Milliseconds(133);
// The delay before the other highlight starts fading in.
constexpr base::TimeDelta kOtherFadeInDelay = base::Milliseconds(117);
// The animation speed at which the preview area fades out (when you snap a
// window).
constexpr base::TimeDelta kPreviewAreaFadeOut = base::Milliseconds(67);
// The time duration for the indicator label opacity animations.
constexpr base::TimeDelta kLabelAnimation = base::Milliseconds(83);
// The delay before the indicator labels start fading in.
constexpr base::TimeDelta kLabelAnimationDelay = base::Milliseconds(167);

// Toast data.
constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";

constexpr char kHistogramPrefix[] = "Ash.SplitViewOverviewSession.";

constexpr char kWindowLayoutCompleteOnSessionExitRootWord[] =
    "WindowLayoutCompleteOnSessionExit";

constexpr char kExitPointRootWord[] = "ExitPoint";

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
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN_CANNOT_SNAP:
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
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN_CANNOT_SNAP:
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

// Returns the corresponding snap action source metric string component with
// given `snap_action_source`.
const char* GetSnapActionSourceMetricComponent(
    WindowSnapActionSource snap_action_source) {
  switch (snap_action_source) {
    case WindowSnapActionSource::kNotSpecified:
      return "NotSpecified";
    case WindowSnapActionSource::kDragWindowToEdgeToSnap:
      return "DragWindowToEdgeToSnap";
    case WindowSnapActionSource::kLongPressCaptionButtonToSnap:
      return "LongPressCaptionButtonToSnap";
    case WindowSnapActionSource::kKeyboardShortcutToSnap:
      return "KeyboardShortcutToSnap";
    case WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap:
      return "DragOrSelectOverviewWindowToSnap";
    case WindowSnapActionSource::kLongPressOverviewButtonToSnap:
      return "LongPressOverviewButtonToSnap";
    case WindowSnapActionSource::kDragUpFromShelfToSnap:
      return "DragUpFromShelfToSnap";
    case WindowSnapActionSource::kDragDownFromTopToSnap:
      return "DragDownFromTopToSnap";
    case WindowSnapActionSource::kDragTabToSnap:
      return "DragTabToSnap";
    case WindowSnapActionSource::kAutoSnapInSplitView:
      return "AutoSnapInSplitView";
    case WindowSnapActionSource::kSnapByWindowStateRestore:
      return "SnapByWindowStateRestore";
    case WindowSnapActionSource::kSnapByWindowLayoutMenu:
      return "SnapByWindowLayoutMenu";
    case WindowSnapActionSource::kSnapByFullRestoreOrDeskTemplateOrSavedDesk:
      return "SnapByFullRestoreOrDeskTemplateOrSavedDesk";
    case WindowSnapActionSource::kSnapByClamshellTabletTransition:
      return "SnapByClamshellTabletTransition";
    case WindowSnapActionSource::kSnapByDeskOrSessionChange:
      return "SnapByDeskOrSessionChange";
    case WindowSnapActionSource::kSnapGroupWindowUpdate:
      return "SnapGroupWindowUpdate";
    case WindowSnapActionSource::kTest:
      return "Test";
  }
}

void AppendUIModeToHistogram(std::string& histogram_name) {
  histogram_name.append(display::Screen::GetScreen()->InTabletMode()
                            ? ".TabletMode"
                            : ".ClamshellMode");
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

  for (aura::Window* transient_window :
       wm::TransientWindowManager::GetOrCreate(window_)->transient_children()) {
    // For now we only care about bubble dialog type transient children.
    views::BubbleDialogDelegate* bubble_delegate_view =
        AsBubbleDialogDelegate(transient_window);
    if (bubble_delegate_view) {
      if (!bubble_delegate_view->GetAnchorRect().IsEmpty() ||
          bubble_delegate_view->GetAnchorView()) {
        bubble_delegate_view->OnAnchorBoundsChanged();
      }
    }
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
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN_CANNOT_SNAP:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN_CANNOT_SNAP:
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
    const std::vector<ui::ImplicitAnimationObserver*>& animation_observers) {
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
  for (ui::ImplicitAnimationObserver* animation_observer : animation_observers)
    settings.AddObserver(animation_observer);
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

int GetWindowLength(aura::Window* window, bool horizontal) {
  const auto& bounds = window->bounds();
  return horizontal ? bounds.width() : bounds.height();
}

bool IsPhysicalLeftOrTop(aura::Window* window) {
  chromeos::WindowStateType state_type =
      WindowState::Get(window)->GetStateType();
  if (IsLayoutPrimary(window)) {
    return state_type == chromeos::WindowStateType::kPrimarySnapped;
  }
  return state_type == chromeos::WindowStateType::kSecondarySnapped;
}

void SetWindowTransformDuringResizing(aura::Window* window,
                                      int divider_position) {
  const bool is_primary_window = IsPhysicalLeftOrTop(window);
  aura::Window* root_window = window->GetRootWindow();
  const int window_size = is_primary_window
                              ? divider_position
                              : GetDividerPositionUpperLimit(root_window) -
                                    divider_position -
                                    kSplitviewDividerShortSideLength;
  const bool horizontal = IsLayoutHorizontal(root_window);
  int distance = window_size - GetWindowLength(window, horizontal);
  gfx::Transform transform;
  if (distance < 0) {
    // If this is the secondary window, translate the other direction.
    distance = is_primary_window ? distance : -distance;
    transform.Translate(horizontal ? distance : 0, horizontal ? 0 : distance);
  }
  window_util::SetTransform(window, transform);
}

// TODO(michelefan): Revisit the logics when split view refactor is ready to
// make everything works with `kSnapGroup` enabled.
void MaybeRestoreSplitView(bool refresh_snapped_windows) {
  const bool should_restore =
      ShouldAllowSplitView() && (display::Screen::GetScreen()->InTabletMode() ||
                                 SnapGroupController::Get());
  if (!should_restore) {
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
      if (!split_view_controller->CanSnapWindow(window,
                                                chromeos::kDefaultSnapRatio)) {
        // Since we are in tablet mode, and this window is not snappable, we
        // should maximize it.
        WindowState::Get(window)->Maximize();
        continue;
      }

      switch (WindowState::Get(window)->GetStateType()) {
        case WindowStateType::kPrimarySnapped:
          if (!split_view_controller->primary_window()) {
            split_view_controller->SnapWindow(
                window, SnapPosition::kPrimary,
                WindowSnapActionSource::kSnapByDeskOrSessionChange);
          }
          break;

        case WindowStateType::kSecondarySnapped:
          if (!split_view_controller->secondary_window()) {
            split_view_controller->SnapWindow(
                window, SnapPosition::kSecondary,
                WindowSnapActionSource::kSnapByDeskOrSessionChange);
          }
          break;

        default:
          break;
      }

      if (split_view_controller->state() ==
          SplitViewController::State::kBothSnapped) {
        break;
      }
    }
  }

  // Ensure that overview mode is active if there is a window snapped to one of
  // the sides. Ensure overview mode is not active if there are two snapped
  // windows.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController::State state = split_view_controller->state();
  if (state == SplitViewController::State::kPrimarySnapped ||
      state == SplitViewController::State::kSecondarySnapped) {
    overview_controller->StartOverview(OverviewStartAction::kSplitView);
  } else if (state == SplitViewController::State::kBothSnapped) {
    overview_controller->EndOverview(OverviewEndAction::kSplitView);
  }
}

bool ShouldAllowSplitView() {
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
  Shell::Get()->toast_manager()->Show(
      ToastData(kAppCannotSnapToastId, ToastCatalogName::kAppCannotSnap,
                l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP),
                ToastData::kDefaultToastDuration,
                /*visible_on_lock_screen=*/false,
                /*has_dismiss_button=*/true));
}

SnapPosition GetSnapPositionForLocation(
    aura::Window* root_window,
    const gfx::Point& location_in_screen,
    const std::optional<gfx::Point>& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset) {
  if (!ShouldAllowSplitView())
    return SnapPosition::kNone;

  const bool horizontal = IsLayoutHorizontal(root_window);
  const bool right_side_up = IsLayoutPrimary(root_window);

  // Check to see if the current event location |location_in_screen| is within
  // the drag indicators bounds.
  const gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window));
  SnapPosition snap_position = SnapPosition::kNone;
  if (horizontal) {
    gfx::Rect area(work_area);
    area.Inset(gfx::Insets::VH(0, horizontal_edge_inset));
    if (location_in_screen.x() <= area.x()) {
      snap_position =
          right_side_up ? SnapPosition::kPrimary : SnapPosition::kSecondary;
    } else if (location_in_screen.x() >= area.right() - 1) {
      snap_position =
          right_side_up ? SnapPosition::kSecondary : SnapPosition::kPrimary;
    }
  } else {
    gfx::Rect area(work_area);
    area.Inset(gfx::Insets::VH(vertical_edge_inset, 0));
    if (location_in_screen.y() <= area.y()) {
      snap_position =
          right_side_up ? SnapPosition::kPrimary : SnapPosition::kSecondary;
    } else if (location_in_screen.y() >= area.bottom() - 1) {
      snap_position =
          right_side_up ? SnapPosition::kSecondary : SnapPosition::kPrimary;
    }
  }

  if (snap_position == SnapPosition::kNone) {
    return snap_position;
  }

  // To avoid accidental snap, the window needs to be dragged inside
  // |snap_distance_from_edge| from edge or dragged toward the edge for at least
  // |minimum_drag_distance| until it's dragged into |horizontal_edge_inset| or
  // |vertical_edge_inset| region.
  // The window should always be snapped if inside |snap_distance_from_edge|
  // from edge.
  bool drag_end_near_edge = false;
  gfx::Rect area(work_area);
  area.Inset(snap_distance_from_edge);
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
    const bool is_left_or_top = IsPhysicalLeftOrTop(snap_position, root_window);
    if ((is_left_or_top && primary_axis_distance > -minimum_drag_distance) ||
        (!is_left_or_top && primary_axis_distance < minimum_drag_distance)) {
      snap_position = SnapPosition::kNone;
    }
  }

  return snap_position;
}

SnapPosition GetSnapPosition(aura::Window* root_window,
                             aura::Window* window,
                             const gfx::Point& location_in_screen,
                             const gfx::Point& initial_location_in_screen,
                             int snap_distance_from_edge,
                             int minimum_drag_distance,
                             int horizontal_edge_inset,
                             int vertical_edge_inset) {
  if (!SplitViewController::Get(root_window)
           ->CanSnapWindow(window, chromeos::kDefaultSnapRatio)) {
    return SnapPosition::kNone;
  }

  std::optional<gfx::Point> initial_location_in_current_screen = std::nullopt;
  if (window->GetRootWindow() == root_window)
    initial_location_in_current_screen = initial_location_in_screen;

  return GetSnapPositionForLocation(
      root_window, location_in_screen, initial_location_in_current_screen,
      snap_distance_from_edge, minimum_drag_distance, horizontal_edge_inset,
      vertical_edge_inset);
}

bool IsLayoutHorizontal(aura::Window* window) {
  return IsLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

bool IsLayoutHorizontal(const display::Display& display) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return IsCurrentScreenOrientationLandscape();
  }

  // TODO(crbug.com/1233192): add DCHECK to avoid square size display.
  DCHECK(display.is_valid());
  return chromeos::IsLandscapeOrientation(GetSnapDisplayOrientation(display));
}

bool IsLayoutPrimary(aura::Window* window) {
  return IsLayoutPrimary(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

bool IsLayoutPrimary(const display::Display& display) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return IsCurrentScreenOrientationPrimary();
  }

  DCHECK(display.is_valid());
  return chromeos::IsPrimaryOrientation(GetSnapDisplayOrientation(display));
}

bool IsPhysicalLeftOrTop(SnapPosition position, aura::Window* window) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(window) ? SnapPosition::kPrimary
                                              : SnapPosition::kSecondary);
}

bool IsPhysicalLeftOrTop(SnapPosition position,
                         const display::Display& display) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(display) ? SnapPosition::kPrimary
                                               : SnapPosition::kSecondary);
}

int GetDividerPositionUpperLimit(aura::Window* root_window) {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  return IsLayoutHorizontal(root_window) ? work_area_bounds.width()
                                         : work_area_bounds.height();
}

// Returns the minimum length of the window according to the screen orientation.
int GetMinimumWindowLength(aura::Window* window, bool horizontal) {
  int minimum_width = 0;
  if (window && window->delegate()) {
    gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    minimum_width = horizontal ? minimum_size.width() : minimum_size.height();
  }
  return minimum_width;
}

int CalculateDividerPosition(SnapPosition snap_position,
                             aura::Window* root_window,
                             float snap_ratio,
                             bool account_for_divider_width) {
  const int divider_upper_limit = GetDividerPositionUpperLimit(root_window);
  // `snap_width` needs to be a float so that the rounding is performed at the
  // end of the computation of `next_divider_position`. It's important because a
  // 1-DIP gap between snapped windows precludes multiresizing. See b/262011280.
  const float snap_width = divider_upper_limit * snap_ratio;
  int next_divider_position = snap_position == SnapPosition::kPrimary
                                  ? snap_width
                                  : divider_upper_limit - snap_width;
  if (account_for_divider_width) {
    next_divider_position -= kSplitviewDividerShortSideLength / 2;
  }
  return next_divider_position;
}

int GetEquivalentDividerPosition(aura::Window* window,
                                 bool account_for_divider_width) {
  aura::Window* root_window = window->GetRootWindow();
  const bool horizontal = IsLayoutHorizontal(root_window);
  const int window_length = GetWindowLength(window, horizontal);
  const bool is_physical_left_or_top = IsPhysicalLeftOrTop(window);
  int divider_position =
      is_physical_left_or_top
          ? window_length
          : GetDividerPositionUpperLimit(root_window) - window_length;
  if (account_for_divider_width) {
    const int factor = is_physical_left_or_top ? -1 : 1;
    divider_position += factor * kSplitviewDividerShortSideLength / 2;
  }
  return divider_position;
}

gfx::Rect CalculateSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* root_window,
    aura::Window* window_for_minimum_size,
    int divider_position,
    int divider_width,
    bool is_resizing_with_divider) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  const bool horizontal = IsLayoutHorizontal(root_window);
  const bool snap_left_or_top = IsPhysicalLeftOrTop(snap_position, root_window);
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  const int work_area_size = GetDividerPositionUpperLimit(root_window);

  // Edit `divider_position` if window restore is currently restoring a snapped
  // window; take into account the snap percentage saved by the window. Only do
  // this for clamshell mode; in tablet mode we are OK with restoring to the
  // default half snap state.
  if (divider_position < 0 && !in_tablet_mode) {
    if (auto* window = WindowRestoreController::Get()->to_be_snapped_window()) {
      app_restore::WindowInfo* window_info =
          window->GetProperty(app_restore::kWindowInfoKey);
      if (window_info && window_info->snap_percentage) {
        const int snap_percentage = *window_info->snap_percentage;
        divider_position = snap_percentage * work_area_size / 100;
        if (!snap_left_or_top) {
          divider_position = work_area_size - divider_position;
        }
      }
    }
  }

  int window_size;
  if (snap_left_or_top) {
    // If there is a divider widget, `divider_position` will have already been
    // subtracted to account for the divider width.
    // TODO(sophiewen): Consolidate subtracting `divider_width` for both
    // primary and secondary windows.
    window_size = divider_position;
  } else {
    window_size = work_area_size - divider_position - divider_width;
  }

  const int minimum =
      GetMinimumWindowLength(window_for_minimum_size, horizontal);
  DCHECK(window_for_minimum_size || minimum == 0);
  if (window_size < minimum) {
    if (in_tablet_mode && !is_resizing_with_divider) {
      // If window with `window_for_minimum_size` gets snapped, the
      // `split_view_divider_` will then be adjusted to its default position and
      // `window_size` will be computed accordingly.
      window_size = work_area_size / 2 - kSplitviewDividerShortSideLength / 2;
      // If `work_area_size` is odd, then the default divider position is
      // rounded down, toward the left or top, but then if `snap_left_or_top` is
      // false, that means `window_size` should now be rounded up.
      if (!snap_left_or_top && work_area_size % 2 == 1) {
        ++window_size;
      }
    } else {
      window_size = minimum;
    }
  }

  if (window_for_minimum_size && !in_tablet_mode) {
    // Apply the unresizable snapping constraint to the snapped bounds if we're
    // in the clamshell mode.
    const gfx::Size* preferred_size =
        window_for_minimum_size->GetProperty(kUnresizableSnappedSizeKey);
    if (preferred_size &&
        !WindowState::Get(window_for_minimum_size)->CanResize()) {
      if (horizontal && preferred_size->width() > 0) {
        window_size = preferred_size->width();
      }
      if (!horizontal && preferred_size->height() > 0) {
        window_size = preferred_size->height();
      }
    }
  }

  // Get the parameter values for which `gfx::Rect::SetByBounds` would recreate
  // `work_area_bounds_in_screen`.
  int left = work_area_bounds_in_screen.x();
  int top = work_area_bounds_in_screen.y();
  int right = work_area_bounds_in_screen.right();
  int bottom = work_area_bounds_in_screen.bottom();

  // Make `snapped_window_bounds_in_screen` by modifying one of the above four
  // values: the one that represents the inner edge of the snapped bounds.
  int& left_or_top = horizontal ? left : top;
  int& right_or_bottom = horizontal ? right : bottom;
  if (snap_left_or_top) {
    right_or_bottom = left_or_top + window_size;
  } else {
    left_or_top = right_or_bottom - window_size;
  }

  gfx::Rect snapped_window_bounds_in_screen;
  snapped_window_bounds_in_screen.SetByBounds(left, top, right, bottom);
  return snapped_window_bounds_in_screen;
}

bool IsSnapGroupEnabledInClamshellMode() {
  auto* snap_group_controller = SnapGroupController::Get();
  return snap_group_controller && !display::Screen::GetScreen()->InTabletMode();
}

int GetWindowComponentForResize(aura::Window* window) {
  chromeos::WindowStateType state_type =
      WindowState::Get(window)->GetStateType();
  CHECK(chromeos::IsSnappedWindowStateType(state_type));
  // TODO(b/288356322): Update the component for vertical splitview.
  return state_type == chromeos::WindowStateType::kPrimarySnapped ? HTRIGHT
                                                                  : HTLEFT;
}

ASH_EXPORT std::string BuildWindowLayoutCompleteOnSessionExitHistogram() {
  std::string histogram_name(kHistogramPrefix);
  histogram_name.append(kWindowLayoutCompleteOnSessionExitRootWord);
  AppendUIModeToHistogram(histogram_name);
  return histogram_name;
}

ASH_EXPORT std::string BuildSplitViewOverviewExitPointHistogramName(
    WindowSnapActionSource snap_action_source) {
  std::string histogram_name(kHistogramPrefix);
  histogram_name.append(GetSnapActionSourceMetricComponent(snap_action_source));
  histogram_name.append(".");
  histogram_name.append(kExitPointRootWord);
  AppendUIModeToHistogram(histogram_name);
  return histogram_name;
}

}  // namespace ash
