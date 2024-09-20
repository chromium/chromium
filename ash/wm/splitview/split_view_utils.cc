// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "base/containers/adapters.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "components/app_restore/window_properties.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

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

constexpr char kSnapWindowSuggestionsHistogramPrefix[] =
    "Ash.SnapWindowSuggestions.";
constexpr char kHistogramPrefix[] = "Ash.SplitViewOverviewSession.";

constexpr char kWindowLayoutCompleteOnSessionExitRootWord[] =
    "WindowLayoutCompleteOnSessionExit";

constexpr char kExitPointRootWord[] = "ExitPoint";

struct AnimationValues {
  base::TimeDelta duration;
  gfx::Tween::Type tween_type;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy =
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET;
  base::TimeDelta delay;
};

AnimationValues GetAnimationValuesForType(SplitviewAnimationType type) {
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
      return {.duration = kHighlightsFadeInOut,
              .tween_type = gfx::Tween::FAST_OUT_SLOW_IN};
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN_CANNOT_SNAP:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
      return {.duration = kOtherFadeInOut,
              .tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN,
              .preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
              .delay = kOtherFadeInDelay};
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
      return {.duration = kOtherFadeInOut,
              .tween_type = gfx::Tween::FAST_OUT_LINEAR_IN};
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
      return {.duration = kPreviewAreaFadeOut,
              .tween_type = gfx::Tween::FAST_OUT_LINEAR_IN};
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
      return {.duration = kLabelAnimation,
              .tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN,
              .preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
              .delay = kLabelAnimationDelay};
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
      return {.duration = kLabelAnimation,
              .tween_type = gfx::Tween::FAST_OUT_LINEAR_IN};
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      return {.duration = kSplitviewWindowTransformDuration,
              .tween_type = gfx::Tween::FAST_OUT_SLOW_IN,
              .preemption_strategy =
                  ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET};
  }

  NOTREACHED();
}

void ApplyAnimationSettings(
    ui::LayerAnimator* animator,
    ui::LayerAnimationElement::AnimatableProperties animated_property,
    base::TimeDelta duration,
    gfx::Tween::Type tween,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    base::TimeDelta delay,
    ui::ScopedLayerAnimationSettings& out_settings) {
  CHECK_EQ(out_settings.GetAnimator(), animator);
  out_settings.SetTransitionDuration(duration);
  out_settings.SetTweenType(tween);
  out_settings.SetPreemptionStrategy(preemption_strategy);
  if (!delay.is_zero()) {
    animator->SchedulePauseForProperties(delay, animated_property);
  }
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
    case WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu:
      return "SnapByLacrosSnapButtonOrWindowLayoutMenu";
    case WindowSnapActionSource::kSnapBySwapWindowsInSnapGroup:
      return "SnapBySwapWindowsInSnapGroup";
  }
}

void AppendUIModeToHistogram(std::string& histogram_name) {
  histogram_name.append(display::Screen::GetScreen()->InTabletMode()
                            ? ".TabletMode"
                            : ".ClamshellMode");
}

// Returns true if there is no window in partial overview (excluding the given
// `window`).
bool IsPartialOverviewEmptyForActiveDesk(aura::Window* window) {
  // Use `BuildMruWindowList()` to include all window types, e.g. always-on-top
  // windows and floated windows.
  for (auto win :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (win != window && wm::GetTransientRoot(win) != window &&
        win->GetRootWindow() == window->GetRootWindow()) {
      return false;
    }
  }

  return true;
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
        window_util::AsBubbleDialogDelegate(transient_window);
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
  }

  if (layer->GetTargetOpacity() == target_opacity)
    return;

  const AnimationValues values = GetAnimationValuesForType(type);
  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(animator, ui::LayerAnimationElement::OPACITY,
                         values.duration, values.tween_type,
                         values.preemption_strategy, values.delay, settings);
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
  }

  const AnimationValues values = GetAnimationValuesForType(type);
  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  for (ui::ImplicitAnimationObserver* animation_observer :
       animation_observers) {
    settings.AddObserver(animation_observer);
  }
  ApplyAnimationSettings(animator, ui::LayerAnimationElement::TRANSFORM,
                         values.duration, values.tween_type,
                         values.preemption_strategy, values.delay, settings);
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
  }

  const AnimationValues values = GetAnimationValuesForType(type);
  ui::ScopedLayerAnimationSettings settings(animator);
  if (animation_observer.get()) {
    settings.AddObserver(animation_observer.release());
  }
  ApplyAnimationSettings(animator, ui::LayerAnimationElement::CLIP,
                         values.duration, values.tween_type,
                         values.preemption_strategy, values.delay, settings);
  layer->SetClipRect(target_clip_rect);
}

int GetWindowLength(aura::Window* window, bool horizontal) {
  const auto& bounds = window->GetTargetBounds();
  return horizontal ? bounds.width() : bounds.height();
}

WindowStateType GetWindowStateTypeFromSnapPosition(SnapPosition snap_position) {
  switch (snap_position) {
    case SnapPosition::kPrimary:
      return WindowStateType::kPrimarySnapped;
    case SnapPosition::kSecondary:
      return WindowStateType::kSecondarySnapped;
    default:
      NOTREACHED();
  }
}

SnapPosition ToSnapPosition(chromeos::WindowStateType type) {
  switch (type) {
    case WindowStateType::kPrimarySnapped:
      return SnapPosition::kPrimary;
    case WindowStateType::kSecondarySnapped:
      return SnapPosition::kSecondary;
    default:
      NOTREACHED();
  }
}

SplitViewOverviewSession* GetSplitViewOverviewSession(aura::Window* window) {
  return RootWindowController::ForWindow(window)->split_view_overview_session();
}

bool IsSnapped(aura::Window* window) {
  return window && WindowState::Get(window)->IsSnapped();
}

void SetWindowTransformDuringResizing(aura::Window* window,
                                      int divider_position) {
  const bool is_primary_window = IsPhysicallyLeftOrTop(window);
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

void MaybeRestoreSplitView(bool refresh_snapped_windows) {
  if (!ShouldAllowSplitView() ||
      !display::Screen::GetScreen()->InTabletMode()) {
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

  // Disallow window dragging and split screen while ChromeVox is on in tablet
  // mode.
  if (display::Screen::GetScreen()->InTabletMode() &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return false;
  }

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
    const bool is_left_or_top =
        IsPhysicallyLeftOrTop(snap_position, root_window);
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

  // TODO(crbug.com/40191408): add DCHECK to avoid square size display.
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

bool IsPhysicallyLeftOrTop(SnapPosition position, aura::Window* window) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(window) ? SnapPosition::kPrimary
                                              : SnapPosition::kSecondary);
}

bool IsPhysicallyLeftOrTop(SnapPosition position,
                           const display::Display& display) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(display) ? SnapPosition::kPrimary
                                               : SnapPosition::kSecondary);
}

bool IsPhysicallyLeftOrTop(aura::Window* window) {
  chromeos::WindowStateType state_type =
      WindowState::Get(window)->GetStateType();
  CHECK(chromeos::IsSnappedWindowStateType(state_type));
  if (IsLayoutPrimary(window)) {
    return state_type == chromeos::WindowStateType::kPrimarySnapped;
  }
  return state_type == chromeos::WindowStateType::kSecondarySnapped;
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

int CalculateDividerPosition(aura::Window* root_window,
                             SnapPosition snap_position,
                             float snap_ratio,
                             bool account_for_divider_width) {
  const int divider_upper_limit = GetDividerPositionUpperLimit(root_window);
  const int divider_delta =
      account_for_divider_width ? kSplitviewDividerShortSideLength : 0;

  // `snap_length` needs to be a float so that the rounding is performed at the
  // end of the computation of `next_divider_position`. It's important because a
  // 1-DIP gap between snapped windows precludes multiresizing. See b/262011280.
  const float snap_length = (divider_upper_limit - divider_delta) * snap_ratio;

  const bool is_layout_primary = IsLayoutPrimary(root_window);
  const bool snap_to_left_or_top =
      (is_layout_primary && snap_position == SnapPosition::kPrimary) ||
      (!is_layout_primary && snap_position == SnapPosition::kSecondary);
  return std::clamp(
      static_cast<int>(snap_to_left_or_top
                           ? snap_length
                           : divider_upper_limit - snap_length - divider_delta),
      0, divider_upper_limit);
}

int GetEquivalentDividerPosition(aura::Window* window,
                                 bool account_for_divider_width) {
  aura::Window* root_window = window->GetRootWindow();
  const bool horizontal = IsLayoutHorizontal(root_window);
  const int window_length = GetWindowLength(window, horizontal);
  const int divider_delta =
      account_for_divider_width ? kSplitviewDividerShortSideLength / 2.f : 0;
  return IsPhysicallyLeftOrTop(window)
             ? window_length - divider_delta
             : GetDividerPositionUpperLimit(root_window) - window_length -
                   divider_delta;
}

gfx::Rect CalculateSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* root_window,
    aura::Window* window_for_minimum_size,
    bool account_for_divider_width,
    int divider_position,
    bool is_resizing_with_divider) {
  const bool snap_left_or_top =
      IsPhysicallyLeftOrTop(snap_position, root_window);
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

  const int divider_width =
      account_for_divider_width ? kSplitviewDividerShortSideLength : 0;
  int window_size = snap_left_or_top
                        ? divider_position
                        : work_area_size - divider_position - divider_width;

  const bool horizontal = IsLayoutHorizontal(root_window);
  const int minimum =
      GetMinimumWindowLength(window_for_minimum_size, horizontal);
  DCHECK(window_for_minimum_size || minimum == 0);
  if (window_size < minimum) {
    if (in_tablet_mode && !is_resizing_with_divider) {
      // If window with `window_for_minimum_size` gets snapped, the
      // `split_view_divider_` will then be adjusted to its default position and
      // `window_size` will be computed accordingly.
      window_size = (work_area_size - kSplitviewDividerShortSideLength) / 2;
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

  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
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

SnapViewType ToSnapViewType(chromeos::WindowStateType state_type) {
  switch (state_type) {
    case chromeos::WindowStateType::kPrimarySnapped:
      return SnapViewType::kPrimary;
    case chromeos::WindowStateType::kSecondarySnapped:
      return SnapViewType::kSecondary;
    default:
      NOTREACHED();
  }
}

chromeos::WindowStateType ToWindowStateType(SnapViewType snap_type) {
  switch (snap_type) {
    case SnapViewType::kPrimary:
      return chromeos::WindowStateType::kPrimarySnapped;
    case SnapViewType::kSecondary:
      return chromeos::WindowStateType::kSecondarySnapped;
  }
}

SnapViewType GetOppositeSnapType(SnapViewType snap_type) {
  switch (snap_type) {
    case SnapViewType::kPrimary:
      return SnapViewType::kSecondary;
    case SnapViewType::kSecondary:
      return SnapViewType::kPrimary;
  }
}

SnapViewType GetOppositeSnapType(aura::Window* window) {
  return GetOppositeSnapType(
      ToSnapViewType(WindowState::Get(window)->GetStateType()));
}

bool CanSnapActionSourceStartFasterSplitView(
    WindowSnapActionSource snap_action_source) {
  switch (snap_action_source) {
    case WindowSnapActionSource::kDragWindowToEdgeToSnap:
    case WindowSnapActionSource::kSnapByWindowLayoutMenu:
    case WindowSnapActionSource::kLongPressCaptionButtonToSnap:
    case WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap:
    case WindowSnapActionSource::kTest:
    case WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu:
      // We only start partial overview for the above snap sources.
      return true;
    default:
      return false;
  }
}

bool ShouldExcludeForOcclusionCheck(const aura::Window* window,
                                    const aura::Window* target_root) {
  // `window` should be excluded for occlusion check under the following
  // conditions:
  // 1. When `window` is not on the same root window as `target_root`;
  // 2. When `window` does not belong to the active desk container, for example
  // always-on-top window, float or pip window;
  // 3. When it is not visible or minimized;
  if (window->GetRootWindow() != target_root || !window->IsVisible()) {
    return true;
  }

  if (!desks_util::IsActiveDeskContainer(window->parent())) {
    return true;
  }

  return WindowState::Get(window)->IsMinimized();
}

aura::Window::Windows GetActiveDeskAppWindowsInZOrder(aura::Window* root) {
  aura::Window::Windows windows;
  const auto children =
      desks_util::GetActiveDeskContainerForRoot(root)->children();
  // Iterate through the desk container's children in reversed order.
  for (const auto& child : base::Reversed(children)) {
    if (CanIncludeWindowInAppMruList(child)) {
      windows.push_back(child.get());
    }
  }
  return windows;
}

aura::Window* GetTopmostVisibleWindowOfSnapType(aura::Window* window_to_ignore,
                                                aura::Window* target_root,
                                                SnapViewType snap_type) {
  // `GetActiveDeskAppWindowsInZOrder()` will exclude transient windows like the
  // window layout menu and other bubble widgets.
  aura::Window::Windows windows = GetActiveDeskAppWindowsInZOrder(target_root);
  const chromeos::WindowStateType target_state_type =
      ToWindowStateType(snap_type);
  auto* overview_session = GetOverviewSession();

  // Track the union bounds of the windows that are more recently used than the
  // currently iterated window, i.e. `top_window` below to check the occlusion
  // state of the opposite snapped window.
  gfx::Rect union_bounds;
  for (aura::Window* top_window : windows) {
    // The `top_window` should be excluded for occlusion check when it is the
    // `window_to_ignore` itself or if `ShouldExcludeForOcclusionCheck()` is
    // true.
    const bool should_be_excluded_for_occlusion_check =
        top_window == window_to_ignore ||
        ShouldExcludeForOcclusionCheck(top_window, target_root);

    if (should_be_excluded_for_occlusion_check) {
      continue;
    }

    if (overview_session && overview_session->IsWindowInOverview(top_window)) {
      // Skip any windows that are in overview, since they are visually not
      // snapped to the user.
      continue;
    }

    const auto* top_window_state = WindowState::Get(top_window);
    const gfx::Rect top_window_bounds = top_window->GetBoundsInScreen();
    if (top_window_state->GetStateType() == target_state_type) {
      // Ensure that `top_window` is fully visible by checking:
      // 1. There is no window stacked above `top_window` with bounds
      // confined or confining `top_window`. Note that if `union_bounds` is
      // empty, `top_window` will be the topmost window snapped on the
      // opposite position;
      // 2. There is no window with bounds that intersect with `top_window`.
      // See http://b/320759574#comment3 for more details with graphs.
      if (!top_window_bounds.Intersects(union_bounds) &&
          !union_bounds.Intersects(top_window_bounds)) {
        return top_window;
      }
    }

    union_bounds.Union(top_window_bounds);
  }

  return nullptr;
}

aura::Window* GetOppositeVisibleSnappedWindow(aura::Window* window) {
  return GetTopmostVisibleWindowOfSnapType(window, window->GetRootWindow(),
                                           GetOppositeSnapType(window));
}

float GetSnapRatioGap(aura::Window* to_be_snapped,
                      aura::Window* opposite_snapped) {
  return std::abs(1.f - window_util::GetSnapRatioForWindow(to_be_snapped) -
                  window_util::GetSnapRatioForWindow(opposite_snapped));
}

bool IsSnapRatioGapWithinThreshold(aura::Window* to_be_snapped,
                                   aura::Window* opposite_snapped) {
  const float snap_ratio_gap = GetSnapRatioGap(to_be_snapped, opposite_snapped);
  // Use a more relaxed tolerance to allow approximate gaps.
  const float diff = snap_ratio_gap - kSnapToReplaceRatioDiffThreshold;
  return diff <= /*tolerance=*/0.01f;
}

float GetAutoSnapRatio(aura::Window* to_be_snapped_window,
                       aura::Window* target_root,
                       SnapViewType snap_type) {
  if (IsSnapGroupEnabledInClamshellMode()) {
    // `GetTopmostVisibleWindowOfSnapType()` will include windows in snap
    // groups.
    if (aura::Window* opposite_window =
            GetTopmostVisibleWindowOfSnapType(to_be_snapped_window, target_root,
                                              GetOppositeSnapType(snap_type))) {
      // If the gap between `opposite_window` and `to_be_snapped_window`,
      // which will always be the default snap ratio for drag to snap, exceeds
      // the threshold, we won't allow auto grouping, so we also don't update
      // the phantom snap ratio.
      if (!IsSnapRatioGapWithinThreshold(to_be_snapped_window,
                                         opposite_window)) {
        return chromeos::kDefaultSnapRatio;
      }
      return 1.f - window_util::GetSnapRatioForWindow(opposite_window);
    }
  }
  return chromeos::kDefaultSnapRatio;
}

bool ShouldConsiderWindowForSplitViewSetupView(
    aura::Window* window,
    WindowSnapActionSource snap_action_source) {
  if (!OverviewController::Get()->CanEnterOverview() ||
      IsPartialOverviewEmptyForActiveDesk(window)) {
    return false;
  }

  if (PrefService* pref =
          Shell::Get()->session_controller()->GetActivePrefService();
      pref && !pref->GetBoolean(prefs::kSnapWindowSuggestions)) {
    return false;
  }

  if (!CanSnapActionSourceStartFasterSplitView(snap_action_source)) {
    return false;
  }

  return !IsInOverviewSession();
}

bool CanStartSplitViewOverviewSessionInClamshell(
    aura::Window* window,
    WindowSnapActionSource snap_action_source) {
  if (IsInOverviewSession() && WindowState::Get(window)->IsSnapped()) {
    return !RootWindowController::ForWindow(window)
                ->split_view_overview_session();
  }

  // Skip starting `SplitViewOverviewSession` if a fully visible window snapped
  // on the opposite side. `GetOppositeVisibleSnappedWindow()` will exclude
  // windows that are *in* overview.
  if (GetOppositeVisibleSnappedWindow(window)) {
    return false;
  }

  return ShouldConsiderWindowForSplitViewSetupView(window, snap_action_source);
}

bool IsSnapGroupEnabledInClamshellMode() {
  return !display::Screen::GetScreen()->InTabletMode();
}

int GetWindowComponentForResize(aura::Window* window) {
  chromeos::WindowStateType state_type =
      WindowState::Get(window)->GetStateType();
  CHECK(chromeos::IsSnappedWindowStateType(state_type));
  // TODO(b/288356322): Update the component for vertical splitview.
  return state_type == chromeos::WindowStateType::kPrimarySnapped ? HTRIGHT
                                                                  : HTLEFT;
}

bool ShouldConsiderDivider(aura::Window* window) {
  if (IsSnapGroupEnabledInClamshellMode()) {
    if (auto* snap_group =
            SnapGroupController::Get()->GetSnapGroupForGivenWindow(window)) {
      return snap_group->snap_group_divider()->divider_widget();
    }
  }
  SplitViewController* split_view_controller =
      SplitViewController::Get(window->GetRootWindow());
  return split_view_controller->InSplitViewMode() &&
         split_view_controller->split_view_divider()->divider_widget();
}

bool CanWindowsFitInWorkArea(aura::Window* window1, aura::Window* window2) {
  DCHECK_EQ(window1->GetRootWindow(), window2->GetRootWindow());
  aura::Window* root_window = window1->GetRootWindow();
  const bool horizontal = IsLayoutHorizontal(root_window);
  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(root_window)
                                  .work_area();
  const int work_area_length =
      horizontal ? work_area.width() : work_area.height();
  return GetMinimumWindowLength(window1, horizontal) +
             GetMinimumWindowLength(window2, horizontal) +
             kSplitviewDividerShortSideLength <=
         work_area_length;
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

std::string BuildSnapWindowSuggestionsHistogramName(
    WindowSnapActionSource snap_action_source) {
  std::string histogram_name(kSnapWindowSuggestionsHistogramPrefix);
  histogram_name.append(GetSnapActionSourceMetricComponent(snap_action_source));
  return histogram_name;
}

}  // namespace ash
