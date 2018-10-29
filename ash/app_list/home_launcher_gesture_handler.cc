// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "ui/aura/client/window_types.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The animation speed at which the window moves when the gesture is released.
constexpr base::TimeDelta kAnimationDurationMs =
    base::TimeDelta::FromMilliseconds(250);

// The animation speed at which the window moves when a window is acitvated from
// the shelf, or deacitvated via home launcher button minimize.
constexpr base::TimeDelta kActivationChangedAnimationDurationMs =
    base::TimeDelta::FromMilliseconds(350);

// The velocity the app list or shelf must be dragged in order to transition to
// the next state regardless of where the gesture ends, measured in DIPs/event.
constexpr int kScrollVelocityThreshold = 6;

// The width of the target of screen bounds will be the work area width times
// this ratio.
constexpr float kWidthRatio = 0.8f;

// Checks if |window| can be hidden or shown with a gesture.
bool CanProcessWindow(aura::Window* window,
                      HomeLauncherGestureHandler::Mode mode) {
  if (!window)
    return false;

  if (!window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSlideUpToShow) {
    return false;
  }

  if (window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSlideDownToHide) {
    return false;
  }

  if (!Shell::Get()->app_list_controller()->IsHomeLauncherEnabledInTabletMode())
    return false;

  if (window->type() == aura::client::WINDOW_TYPE_POPUP)
    return false;

  if (::wm::GetTransientParent(window))
    return false;

  return true;
}

// Find the transform that will convert |src| to |dst|.
gfx::Transform CalculateTransform(const gfx::RectF& src,
                                  const gfx::RectF& dst) {
  return gfx::Transform(dst.width() / src.width(), 0, 0,
                        dst.height() / src.height(), dst.x() - src.x(),
                        dst.y() - src.y());
}

// Get the target offscreen workspace bounds.
gfx::RectF GetOffscreenWorkspaceBounds(const gfx::RectF& work_area) {
  gfx::RectF new_work_area;
  new_work_area.set_x(((1.f - kWidthRatio) / 2.f) * work_area.width() +
                      work_area.x());
  new_work_area.set_width(kWidthRatio * work_area.width());
  new_work_area.set_height(kWidthRatio * work_area.height());
  new_work_area.set_y(work_area.y() - work_area.height());
  return new_work_area;
}

// Get the target bounds of a window. It should maintain the same ratios
// relative the work area.
gfx::RectF GetOffscreenWindowBounds(aura::Window* window,
                                    const gfx::RectF& src_work_area,
                                    const gfx::RectF& dst_work_area) {
  gfx::RectF bounds = gfx::RectF(window->GetTargetBounds());
  float ratio = dst_work_area.width() / src_work_area.width();

  gfx::RectF dst_bounds;
  dst_bounds.set_x(bounds.x() * ratio + dst_work_area.x());
  dst_bounds.set_y(bounds.y() * ratio + dst_work_area.y());
  dst_bounds.set_width(bounds.width() * ratio);
  dst_bounds.set_height(bounds.height() * ratio);
  return dst_bounds;
}

// Given a |location_in_screen|, find out where it lies as a ratio in the
// work area, where the top of the work area is 0.f and the bottom is 1.f.
double GetHeightInWorkAreaAsRatio(const gfx::Point& location_in_screen,
                                  const gfx::Rect& work_area) {
  int clamped_y = base::ClampToRange(location_in_screen.y(), work_area.y(),
                                     work_area.bottom());
  double ratio =
      static_cast<double>(clamped_y) / static_cast<double>(work_area.height());
  return 1.0 - ratio;
}

bool IsLastEventInTopHalf(const gfx::Point& location_in_screen,
                          const gfx::Rect& work_area) {
  return GetHeightInWorkAreaAsRatio(location_in_screen, work_area) > 0.5;
}

// Returns the window of the widget which contains the workspace backdrop. May
// be nullptr if the backdrop is not shown.
aura::Window* GetBackdropWindow(aura::Window* window) {
  WorkspaceLayoutManager* layout_manager =
      RootWindowController::ForWindow(window->GetRootWindow())
          ->workspace_controller()
          ->layout_manager();
  return layout_manager
             ? layout_manager->backdrop_controller()->backdrop_window()
             : nullptr;
}

// Returns the window of the widget of the split view divider. May be nullptr if
// split view is not active.
aura::Window* GetDividerWindow() {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (!split_view_controller->IsSplitViewModeActive())
    return nullptr;
  return split_view_controller->split_view_divider()
      ->divider_widget()
      ->GetNativeWindow();
}

}  // namespace

HomeLauncherGestureHandler::HomeLauncherGestureHandler(
    AppListControllerImpl* app_list_controller)
    : app_list_controller_(app_list_controller) {
  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

HomeLauncherGestureHandler::~HomeLauncherGestureHandler() {
  StopObservingImplicitAnimations();
}

bool HomeLauncherGestureHandler::OnPressEvent(Mode mode,
                                              const gfx::Point& location) {
  // Do not start a new session if a window is currently being processed.
  if (!IsIdle())
    return false;

  display_ = display::Screen::GetScreen()->GetDisplayNearestPoint(location);
  if (!display_.is_valid())
    return false;

  if (!SetUpWindows(mode, /*window=*/nullptr))
    return false;

  mode_ = mode;
  initial_event_location_ = location;
  last_event_location_ = base::make_optional(location);

  UpdateWindows(0.0, /*animate=*/false);
  return true;
}

bool HomeLauncherGestureHandler::OnScrollEvent(const gfx::Point& location,
                                               float scroll_y) {
  if (!IsDragInProgress())
    return false;

  last_event_location_ = base::make_optional(location);
  last_scroll_y_ = scroll_y;
  if (mode_ == Mode::kSlideUpToShow &&
      (*last_event_location_ - initial_event_location_).y() > 0) {
    UpdateWindows(0.0, /*animate=*/false);
    return true;
  }

  DCHECK(display_.is_valid());
  UpdateWindows(GetHeightInWorkAreaAsRatio(location, display_.work_area()),
                /*animate=*/false);
  return true;
}

bool HomeLauncherGestureHandler::OnReleaseEvent(const gfx::Point& location,
                                                bool* out_dragged_down) {
  if (!IsDragInProgress()) {
    if (window_) {
      // |window_| may not be nullptr when this release event is triggered by
      // opening |window_| with modal dialog in OnPressEvent(). In that case,
      // just leave the |window_| in show state and stop tracking.
      RemoveObserversAndStopTracking();
      return true;
    }
    return false;
  }

  last_event_location_ = base::make_optional(location);
  if (out_dragged_down) {
    DCHECK_EQ(mode_, Mode::kSlideUpToShow);
    *out_dragged_down =
        (*last_event_location_ - initial_event_location_).y() > 0;
  }
  AnimateToFinalState();
  return true;
}

void HomeLauncherGestureHandler::Cancel() {
  if (!IsDragInProgress())
    return;

  AnimateToFinalState();
  return;
}

bool HomeLauncherGestureHandler::ShowHomeLauncher(
    const display::Display& display) {
  if (!IsIdle())
    return false;

  if (!display.is_valid())
    return false;

  if (!SetUpWindows(Mode::kSlideUpToShow, /*window=*/nullptr))
    return false;

  display_ = display;
  mode_ = Mode::kSlideUpToShow;

  UpdateWindows(0.0, /*animate=*/false);
  AnimateToFinalState();
  return true;
}

bool HomeLauncherGestureHandler::HideHomeLauncherForWindow(
    const display::Display& display,
    aura::Window* window) {
  if (!IsIdle())
    return false;

  if (!display.is_valid())
    return false;

  if (!SetUpWindows(Mode::kSlideDownToHide, window))
    return false;

  display_ = display;
  mode_ = Mode::kSlideDownToHide;

  UpdateWindows(1.0, /*animate=*/false);
  AnimateToFinalState();
  return true;
}

void HomeLauncherGestureHandler::OnWindowDestroying(aura::Window* window) {
  if (window == window_) {
    for (auto* hidden_window : hidden_windows_)
      hidden_window->Show();

    RemoveObserversAndStopTracking();
    return;
  }

  if (window == window2_) {
    DCHECK(window_);
    window->RemoveObserver(this);
    window2_ = nullptr;
    return;
  }

  if (transient_descendants_values_.find(window) !=
      transient_descendants_values_.end()) {
    window->RemoveObserver(this);
    transient_descendants_values_.erase(window);
    return;
  }

  if (transient_descendants_values2_.find(window) !=
      transient_descendants_values2_.end()) {
    window->RemoveObserver(this);
    transient_descendants_values2_.erase(window);
    return;
  }

  DCHECK(base::ContainsValue(hidden_windows_, window));
  window->RemoveObserver(this);
  hidden_windows_.erase(
      std::find(hidden_windows_.begin(), hidden_windows_.end(), window));
}

void HomeLauncherGestureHandler::OnTabletModeEnded() {
  if (IsIdle())
    return;

  // When leaving tablet mode advance to the end of the in progress scroll
  // session or animation.
  StopObservingImplicitAnimations();
  if (window_)
    window_->layer()->GetAnimator()->StopAnimating();
  if (window2_)
    window2_->layer()->GetAnimator()->StopAnimating();
  for (const auto& descendant : transient_descendants_values_)
    descendant.first->layer()->GetAnimator()->StopAnimating();
  for (const auto& descendant : transient_descendants_values2_)
    descendant.first->layer()->GetAnimator()->StopAnimating();
  UpdateWindows(IsFinalStateShow() ? 1.0 : 0.0,
                /*animate=*/false);
  OnImplicitAnimationsCompleted();
}

void HomeLauncherGestureHandler::OnImplicitAnimationsCompleted() {
  float app_list_opacity = 1.f;
  const bool is_final_state_show = IsFinalStateShow();
  if (Shell::Get()->window_selector_controller()->IsSelecting()) {
    if (is_final_state_show) {
      // Exit overview if event is released on the top half. This will also end
      // splitview if it is active as SplitViewController observes overview mode
      // ends.
      Shell::Get()->window_selector_controller()->ToggleOverview(
          WindowSelector::EnterExitOverviewType::kSwipeFromShelf);
    } else {
      app_list_opacity = 0.f;
    }
  }

  // Return the app list to its original opacity and transform without
  // animation.
  DCHECK(display_.is_valid());
  app_list_controller_->presenter()->UpdateYPositionAndOpacityForHomeLauncher(
      display_.work_area().y(), app_list_opacity, base::NullCallback());

  if (!window_) {
    RemoveObserversAndStopTracking();
    return;
  }

  // Explicitly exit split view if two windows are snapped.
  if (is_final_state_show && Shell::Get()->split_view_controller()->state() ==
                                 SplitViewController::BOTH_SNAPPED) {
    Shell::Get()->split_view_controller()->EndSplitView();
  }

  window_->SetTransform(window_values_.initial_transform);
  window_->layer()->SetOpacity(window_values_.initial_opacity);
  if (window2_) {
    window2_->SetTransform(window_values2_.initial_transform);
    window2_->layer()->SetOpacity(window_values2_.initial_opacity);
  }

  if (is_final_state_show) {
    ScopedAnimationDisabler disable(window_);
    window_->Hide();
    wm::GetWindowState(window_)->Minimize();

    if (window2_) {
      ScopedAnimationDisabler disable(window2_);
      window2_->Hide();
      wm::GetWindowState(window2_)->Minimize();
    }

    // Minimize the hidden windows so they can be used normally with alt+tab
    // and overview. Minimize in reverse order to preserve mru ordering.
    std::reverse(hidden_windows_.begin(), hidden_windows_.end());
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Hide();
      wm::GetWindowState(window)->Minimize();
    }
  } else {
    // Reshow all windows previously hidden.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Show();
    }
  }

  // Update the backdrop last as the backdrop controller listens for some state
  // changes like minimizing above which may also alter the backdrop.
  aura::Window* backdrop_window = GetBackdropWindow(window_);
  if (backdrop_window) {
    backdrop_window->SetTransform(gfx::Transform());
    backdrop_window->layer()->SetOpacity(1.f);
  }

  RemoveObserversAndStopTracking();
}

void HomeLauncherGestureHandler::AnimateToFinalState() {
  const bool is_final_state_show = IsFinalStateShow();
  UpdateWindows(is_final_state_show ? 1.0 : 0.0, /*animate=*/true);

  if (!is_final_state_show && mode_ == Mode::kSlideDownToHide) {
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeLauncherToMRUWindow"));
  } else if (is_final_state_show && mode_ == Mode::kSlideUpToShow) {
    base::RecordAction(
        base::UserMetricsAction("AppList_CurrentWindowToHomeLauncher"));
  }
}

void HomeLauncherGestureHandler::UpdateSettings(
    ui::ScopedLayerAnimationSettings* settings,
    bool observe) {
  settings->SetTransitionDuration(IsDragInProgress()
                                      ? kAnimationDurationMs
                                      : kActivationChangedAnimationDurationMs);
  settings->SetTweenType(IsDragInProgress() ? gfx::Tween::LINEAR
                                            : gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (observe)
    settings->AddObserver(this);
}

void HomeLauncherGestureHandler::UpdateWindows(double progress, bool animate) {
  // Update full screen applist.
  DCHECK(display_.is_valid());
  const gfx::Rect work_area = display_.work_area();
  const int y_position =
      gfx::Tween::IntValueBetween(progress, work_area.bottom(), work_area.y());
  const float opacity = gfx::Tween::FloatValueBetween(progress, 0.f, 1.f);
  app_list_controller_->presenter()->UpdateYPositionAndOpacityForHomeLauncher(
      y_position, opacity,
      animate ? base::BindRepeating(&HomeLauncherGestureHandler::UpdateSettings,
                                    base::Unretained(this))
              : base::NullCallback());

  // Update the overview grid if needed.
  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  if (controller->IsSelecting()) {
    DCHECK_EQ(mode_, Mode::kSlideUpToShow);
    controller->window_selector()->UpdateGridAtLocationYPositionAndOpacity(
        display_.id(), y_position - work_area.height(), 1.f - opacity,
        work_area,
        animate
            ? base::BindRepeating(&HomeLauncherGestureHandler::UpdateSettings,
                                  base::Unretained(this))
            : base::NullCallback());
  }

  if (!window_)
    return;

  // Helper to update a single windows opacity and transform based on by
  // calculating the in between values using |value| and |values|.
  auto update_windows_helper = [this](double progress, bool animate,
                                      aura::Window* window,
                                      const WindowValues& values) {
    float opacity = gfx::Tween::FloatValueBetween(
        progress, values.initial_opacity, values.target_opacity);
    gfx::Transform transform = gfx::Tween::TransformValueBetween(
        progress, values.initial_transform, values.target_transform);

    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (animate) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          window->layer()->GetAnimator());
      // There are multiple animations run on a release event (app list,
      // overview and the stored windows). We only want to act on one animation
      // end, so only observe one of the animations. If overview is active,
      // observe the shield widget of the grid, else observe |window_|.
      UpdateSettings(
          settings.get(),
          this->window_ == window &&
              !Shell::Get()->window_selector_controller()->IsSelecting());
    }
    window->layer()->SetOpacity(opacity);
    window->SetTransform(transform);
  };

  aura::Window* backdrop_window = GetBackdropWindow(window_);
  if (backdrop_window && backdrop_values_) {
    update_windows_helper(progress, animate, backdrop_window,
                          *backdrop_values_);
  }

  aura::Window* divider_window = GetDividerWindow();
  if (divider_window && divider_values_) {
    update_windows_helper(progress, animate, divider_window, *divider_values_);
  }

  for (const auto& descendant : transient_descendants_values_) {
    update_windows_helper(progress, animate, descendant.first,
                          descendant.second);
  }

  for (const auto& descendant : transient_descendants_values2_) {
    update_windows_helper(progress, animate, descendant.first,
                          descendant.second);
  }

  if (window2_)
    update_windows_helper(progress, animate, window2_, window_values2_);
  update_windows_helper(progress, animate, window_, window_values_);
}

void HomeLauncherGestureHandler::RemoveObserversAndStopTracking() {
  display_.set_id(display::kInvalidDisplayId);
  backdrop_values_ = base::nullopt;
  divider_values_ = base::nullopt;
  last_event_location_ = base::nullopt;
  last_scroll_y_ = 0.f;
  mode_ = Mode::kNone;

  for (auto* window : hidden_windows_)
    window->RemoveObserver(this);
  hidden_windows_.clear();

  for (const auto& descendant : transient_descendants_values_)
    descendant.first->RemoveObserver(this);
  transient_descendants_values_.clear();

  if (window_)
    window_->RemoveObserver(this);
  window_ = nullptr;

  for (const auto& descendant : transient_descendants_values2_)
    descendant.first->RemoveObserver(this);
  transient_descendants_values2_.clear();

  if (window2_)
    window2_->RemoveObserver(this);
  window2_ = nullptr;
}

bool HomeLauncherGestureHandler::IsIdle() {
  if (IsDragInProgress())
    return false;

  if (window_ && window_->layer()->GetAnimator()->is_animating())
    return false;

  if (window2_ && window2_->layer()->GetAnimator()->is_animating())
    return false;

  for (const auto& descendant : transient_descendants_values_) {
    if (descendant.first->layer()->GetAnimator()->is_animating())
      return false;
  }

  for (const auto& descendant : transient_descendants_values2_) {
    if (descendant.first->layer()->GetAnimator()->is_animating())
      return false;
  }

  return true;
}

bool HomeLauncherGestureHandler::IsFinalStateShow() {
  DCHECK_NE(Mode::kNone, mode_);
  DCHECK(display_.is_valid());

  // If fling velocity is greater than the threshold, show the launcher if
  // sliding up, or hide the launcher if sliding down, irregardless of
  // |last_event_location_|.
  if (mode_ == Mode::kSlideUpToShow &&
      last_scroll_y_ < -kScrollVelocityThreshold) {
    return true;
  }

  if (mode_ == Mode::kSlideDownToHide &&
      last_scroll_y_ > kScrollVelocityThreshold) {
    return false;
  }

  return last_event_location_
             ? IsLastEventInTopHalf(*last_event_location_, display_.work_area())
             : mode_ == Mode::kSlideUpToShow;
}

bool HomeLauncherGestureHandler::SetUpWindows(Mode mode, aura::Window* window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  const bool overview_active =
      Shell::Get()->window_selector_controller()->IsSelecting();
  const bool split_view_active = split_view_controller->IsSplitViewModeActive();
  auto windows = Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  if (window && (mode != Mode::kSlideDownToHide || overview_active ||
                 split_view_active)) {
    window_ = nullptr;
    return false;
  }

  if (window && !windows.empty() && windows[0] != window &&
      windows[0]->IsVisible()) {
    // Do not run slide down animation for the |window| if another active window
    // in mru list exists.
    window_ = nullptr;
    return false;
  }

  if (Shell::Get()
          ->app_list_controller()
          ->IsHomeLauncherEnabledInTabletMode() &&
      overview_active && !split_view_active) {
    DCHECK_EQ(Mode::kSlideUpToShow, mode);
    window_ = nullptr;
    return true;
  }

  // Always hide split view windows if they exist. Otherwise, hide the specified
  // window if it is not null. If non of above is true, we want the first window
  // in the mru list, if it exists and is usable.
  window_ = split_view_active
                ? split_view_controller->GetDefaultSnappedWindow()
                : (window ? window : (windows.empty() ? nullptr : windows[0]));
  if (!CanProcessWindow(window_, mode)) {
    window_ = nullptr;
    return false;
  }

  DCHECK(base::ContainsValue(windows, window_));
  DCHECK_NE(Mode::kNone, mode);
  base::RecordAction(base::UserMetricsAction(
      mode == Mode::kSlideDownToHide
          ? "AppList_HomeLauncherToMRUWindowAttempt"
          : "AppList_CurrentWindowToHomeLauncherAttempt"));
  window_->AddObserver(this);
  base::EraseIf(windows,
                [this](aura::Window* elem) { return elem == this->window_; });

  // Alter a second window if we are in split view mode with two windows
  // snapped.
  if (mode == Mode::kSlideUpToShow &&
      split_view_controller->state() == SplitViewController::BOTH_SNAPPED) {
    DCHECK_GT(windows.size(), 0u);
    window2_ = split_view_controller->default_snap_position() ==
                       SplitViewController::LEFT
                   ? split_view_controller->right_window()
                   : split_view_controller->left_window();
    DCHECK(base::ContainsValue(windows, window2_));
    window2_->AddObserver(this);
    base::EraseIf(
        windows, [this](aura::Window* elem) { return elem == this->window2_; });
  }

  // Show |window_| if we are swiping down to hide.
  if (mode == Mode::kSlideDownToHide) {
    ScopedAnimationDisabler disable(window_);
    window_->Show();

    // When |window_| has a modal dialog child, window_->Show() above would
    // cancel the current gesture and trigger OnReleaseEvent() to reset
    // |window_|.
    if (!window_)
      return false;

    wm::ActivateWindow(window_);
    window_->layer()->SetOpacity(1.f);
  }

  const gfx::RectF work_area =
      gfx::RectF(screen_util::GetDisplayWorkAreaBoundsInParent(window_));
  const gfx::RectF target_work_area = GetOffscreenWorkspaceBounds(work_area);

  // Helper function that calculates the values for |window| and its transient
  // descendants.
  auto compute_window_values =
      [this, &work_area, &target_work_area](
          aura::Window* window, WindowValues* out_window_values,
          std::map<aura::Window*, WindowValues>* out_transient_values) {
        out_transient_values->clear();
        for (auto* it : wm::GetTransientTreeIterator(window)) {
          WindowValues values;
          values.initial_opacity = it->layer()->opacity();
          values.initial_transform = it->transform();
          values.target_opacity = 0.f;
          values.target_transform = CalculateTransform(
              gfx::RectF(it->GetTargetBounds()),
              GetOffscreenWindowBounds(it, work_area, target_work_area));
          if (it == window) {
            *out_window_values = values;
            continue;
          }

          it->AddObserver(this);
          (*out_transient_values)[it] = values;
        }
      };

  compute_window_values(window_, &window_values_,
                        &transient_descendants_values_);
  if (window2_) {
    compute_window_values(window2_, &window_values2_,
                          &transient_descendants_values2_);
  }

  aura::Window* backdrop_window = GetBackdropWindow(window_);
  if (backdrop_window) {
    // Store the values needed to transform the backdrop. The backdrop actually
    // covers the area behind the shelf as well, so initially transform it to be
    // sized to the work area. Without the transform tweak, there is an extra
    // shelf sized black area under |window_|. Go to 0.01 opacity instead of 0
    // opacity otherwise animation end code will attempt to update the backdrop
    // which will try to show a 0 opacity window which causes a crash.
    backdrop_values_ = base::make_optional(WindowValues());
    backdrop_values_->initial_opacity = 1.f;
    backdrop_values_->initial_transform = gfx::Transform(
        1.f, 0.f, 0.f,
        work_area.height() /
            static_cast<float>(backdrop_window->bounds().height()),
        0.f, 0.f);
    backdrop_values_->target_opacity = 0.01f;
    backdrop_values_->target_transform = CalculateTransform(
        gfx::RectF(backdrop_window->bounds()), target_work_area);
  }

  // Stores values needed to transform the split view divider if it exists.
  aura::Window* divider_window = GetDividerWindow();
  if (divider_window) {
    divider_values_ = base::make_optional(WindowValues());
    divider_values_->initial_opacity = 1.f;
    divider_values_->initial_transform = gfx::Transform();
    divider_values_->target_opacity = 0.f;
    divider_values_->target_transform = CalculateTransform(
        gfx::RectF(divider_window->bounds()),
        GetOffscreenWindowBounds(divider_window, work_area, target_work_area));
  }

  // Hide all visible windows which are behind our window so that when we
  // scroll, the home launcher will be visible. This is only needed when swiping
  // up, and not when overview mode is active.
  hidden_windows_.clear();
  if (mode == Mode::kSlideUpToShow && !overview_active) {
    for (auto* window : windows) {
      if (window->IsVisible()) {
        hidden_windows_.push_back(window);
        window->AddObserver(this);
        ScopedAnimationDisabler disable(window);
        window->Hide();
      }
    }
  }

  return true;
}

}  // namespace ash
