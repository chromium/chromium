// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_launcher_gesture_handler.h"

#include <algorithm>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/drag_window_from_shelf_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/swipe_home_to_overview_controller.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform_util.h"
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

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Checks if |window| can be hidden or shown with a gesture.
bool CanProcessWindow(aura::Window* window,
                      HomeLauncherGestureHandler::Mode mode) {
  if (!window)
    return false;

  if (!IsTabletMode())
    return false;

  if (!window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSlideUpToShow) {
    return false;
  }

  if (window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSlideDownToHide) {
    return false;
  }

  if (window->type() == aura::client::WINDOW_TYPE_POPUP)
    return false;

  // Do not process if |window| is not the root of a transient tree.
  if (::wm::GetTransientParent(window))
    return false;

  return true;
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
double GetHeightInWorkAreaAsRatio(const gfx::PointF& location_in_screen,
                                  const gfx::Rect& work_area) {
  float clamped_y = base::ClampToRange(
      location_in_screen.y(), float{work_area.y()}, float{work_area.bottom()});
  double ratio = clamped_y / double{work_area.height()};
  return 1.0 - ratio;
}

bool IsLastEventInTopHalf(const gfx::PointF& location_in_screen,
                          const gfx::Rect& work_area) {
  return GetHeightInWorkAreaAsRatio(location_in_screen, work_area) > 0.5;
}

// Returns the window of the widget which contains the workspace backdrop. May
// be nullptr if the backdrop is not shown.
aura::Window* GetBackdropWindow(aura::Window* window) {
  WorkspaceController* workspace_controller =
      GetWorkspaceControllerForContext(window);
  DCHECK(workspace_controller);

  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();

  return layout_manager
             ? layout_manager->backdrop_controller()->backdrop_window()
             : nullptr;
}

// Returns the window of the widget of the split view divider. May be nullptr if
// split view is not active.
aura::Window* GetDividerWindow() {
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (!split_view_controller->InSplitViewMode())
    return nullptr;
  return split_view_controller->split_view_divider()
      ->divider_widget()
      ->GetNativeWindow();
}

HomeScreenDelegate* GetHomeScreenDelegate() {
  return Shell::Get()->home_screen_controller()->delegate();
}

}  // namespace

// Class which allows us to make modifications to a window, and removes those
// modifications on destruction.
// TODO(sammiequon): Move to separate file and add test for
// ComputeWindowValues.
class HomeLauncherGestureHandler::ScopedWindowModifier
    : public aura::WindowObserver {
 public:
  explicit ScopedWindowModifier(aura::Window* window) : window_(window) {
    DCHECK(window_);
    event_targeting_blocker_ =
        std::make_unique<aura::ScopedWindowEventTargetingBlocker>(window_);
  }
  ~ScopedWindowModifier() override {
    for (const auto& descendant : transient_descendants_values_)
      descendant.first->RemoveObserver(this);

    ResetOpacityAndTransform();
  }

  bool IsAnimating() const {
    if (window_->layer()->GetAnimator()->is_animating())
      return true;

    for (const auto& descendant : transient_descendants_values_) {
      if (descendant.first->layer()->GetAnimator()->is_animating())
        return true;
    }

    return false;
  }

  void StopAnimating() {
    window_->layer()->GetAnimator()->StopAnimating();
    for (const auto& descendant : transient_descendants_values_)
      descendant.first->layer()->GetAnimator()->StopAnimating();
  }

  void ResetOpacityAndTransform() {
    window_->SetTransform(window_values_.initial_transform);
    window_->layer()->SetOpacity(window_values_.initial_opacity);
    for (const auto& descendant : transient_descendants_values_) {
      descendant.first->SetTransform(descendant.second.initial_transform);
      descendant.first->layer()->SetOpacity(descendant.second.initial_opacity);
    }
  }

  // Calculates the values for |window_| and its transient descendants.
  void ComputeWindowValues(const gfx::RectF& work_area,
                           const gfx::RectF& target_work_area) {
    transient_descendants_values_.clear();
    for (auto* window : GetTransientTreeIterator(window_)) {
      WindowValues values;
      values.initial_opacity = window->layer()->opacity();
      values.initial_transform = window->transform();
      values.target_opacity = 0.f;
      values.target_transform = gfx::TransformBetweenRects(
          gfx::RectF(window->GetTargetBounds()),
          GetOffscreenWindowBounds(window, work_area, target_work_area));
      if (window == window_) {
        window_values_ = values;
        continue;
      }

      window->AddObserver(this);
      transient_descendants_values_[window] = values;
    }
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    auto it = transient_descendants_values_.find(window);
    DCHECK(it != transient_descendants_values_.end());

    window->RemoveObserver(this);
    transient_descendants_values_.erase(it);
  }

  aura::Window* window() { return window_; }
  const WindowValues& window_values() const { return window_values_; }
  const std::map<aura::Window*, WindowValues>& transient_descendants_values()
      const {
    return transient_descendants_values_;
  }

 private:
  aura::Window* window_;

  // Original and target transform and opacity of |window_|.
  WindowValues window_values_;

  // Tracks the transient descendants of |window_| and their initial and
  // target opacities and transforms.
  std::map<aura::Window*, WindowValues> transient_descendants_values_;

  // For the duration of this object |window_| event targeting policy will be
  // sent to kNone. Store the original so we can change it back when destroying
  // this object.
  std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>
      event_targeting_blocker_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowModifier);
};

HomeLauncherGestureHandler::HomeLauncherGestureHandler() {
  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

HomeLauncherGestureHandler::~HomeLauncherGestureHandler() {
  StopObservingImplicitAnimations();
}

bool HomeLauncherGestureHandler::OnPressEvent(Mode mode,
                                              const gfx::PointF& location) {
  // Do not start a new session if a window is currently being processed.
  if (!IsIdle())
    return false;

  display_ = display::Screen::GetScreen()->GetDisplayNearestPoint(
      gfx::ToRoundedPoint(location));
  if (!display_.is_valid())
    return false;

  if (!SetUpWindows(mode, /*window=*/nullptr))
    return false;

  mode_ = mode;
  if (mode_ != Mode::kSwipeHomeToOverview)
    last_event_location_ = base::make_optional(location);

  OnDragStarted(location);
  return true;
}

bool HomeLauncherGestureHandler::OnScrollEvent(const gfx::PointF& location,
                                               float scroll_x,
                                               float scroll_y) {
  if (IsAnimating())
    return false;

  if (!IsDragInProgress())
    return false;

  if (mode_ != Mode::kSwipeHomeToOverview) {
    last_event_location_ = base::make_optional(location);
    last_scroll_y_ = scroll_y;
  }

  DCHECK(display_.is_valid());

  OnDragContinued(location, scroll_x, scroll_y);
  return true;
}

bool HomeLauncherGestureHandler::OnReleaseEvent(
    const gfx::PointF& location,
    base::Optional<float> velocity_y) {
  if (mode_ != Mode::kSwipeHomeToOverview && IsAnimating())
    return false;

  return OnDragEnded(location, velocity_y);
}

void HomeLauncherGestureHandler::Cancel() {
  if (!IsDragInProgress())
    return;

  OnDragCancelled();
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

  PauseBackdropUpdatesForActiveWindow();
  UpdateWindowsForSlideUpOrDown(0.0 /*progress*/,
                                base::nullopt /*animation_trigger*/);
  AnimateToFinalState(AnimationTrigger::kLauncherButton);
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

  PauseBackdropUpdatesForActiveWindow();
  UpdateWindowsForSlideUpOrDown(1.0 /*progress*/,
                                base::nullopt /*animation_trigger*/);
  AnimateToFinalState(AnimationTrigger::kHideForWindow);
  return true;
}

aura::Window* HomeLauncherGestureHandler::GetActiveWindow() {
  if (!active_window_)
    return nullptr;
  return active_window_->window();
}

aura::Window* HomeLauncherGestureHandler::GetSecondaryWindow() {
  if (!secondary_window_)
    return nullptr;
  return secondary_window_->window();
}

bool HomeLauncherGestureHandler::IsDragInProgress() const {
  return mode_ != Mode::kNone;
}

void HomeLauncherGestureHandler::NotifyHomeLauncherPositionChanged(
    int percent_shown,
    int64_t display_id) {
  GetHomeScreenDelegate()->OnHomeLauncherPositionChanged(percent_shown,
                                                         display_id);
}

void HomeLauncherGestureHandler::NotifyHomeLauncherAnimationComplete(
    bool shown,
    int64_t display_id) {
  GetHomeScreenDelegate()->OnHomeLauncherAnimationComplete(shown, display_id);
}

void HomeLauncherGestureHandler::OnWindowDestroying(aura::Window* window) {
  if (window == GetActiveWindow()) {
    for (auto* hidden_window : hidden_windows_)
      hidden_window->Show();

    RemoveObserversAndStopTracking();
    return;
  }

  if (window == GetSecondaryWindow()) {
    DCHECK(active_window_);
    window->RemoveObserver(this);
    secondary_window_.reset();
    return;
  }

  DCHECK(base::Contains(hidden_windows_, window));
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

  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_->CancelDrag();
    RemoveObserversAndStopTracking();
  } else {
    if (active_window_)
      active_window_->StopAnimating();
    if (secondary_window_)
      secondary_window_->StopAnimating();

    UpdateWindowsForSlideUpOrDown(IsFinalStateShow() ? 1.0 : 0.0 /*progress*/,
                                  base::nullopt /*animation_trigger*/);
    OnImplicitAnimationsCompleted();
  }
}

void HomeLauncherGestureHandler::OnImplicitAnimationsCompleted() {
  const bool is_final_state_show = IsFinalStateShow();
  base::ScopedClosureRunner notification_runner(base::BindOnce(
      &HomeLauncherGestureHandler::NotifyHomeLauncherAnimationComplete,
      base::Unretained(this), is_final_state_show, display_.id()));
  float home_launcher_opacity = 1.f;
  if (Shell::Get()->overview_controller()->InOverviewSession()) {
    if (overview_active_on_gesture_start_ && is_final_state_show) {
      // Exit overview if event is released on the top half. This will also
      // end splitview if it is active as SplitViewController observes
      // overview mode ends.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewSession::EnterExitOverviewType::kSwipeFromShelf);
    } else {
      home_launcher_opacity = 0.f;
    }
  }

  HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
  DCHECK(home_screen_delegate);

  // Return the app list to its original opacity and transform without
  // animation.
  DCHECK(display_.is_valid());
  home_screen_delegate->UpdateYPositionAndOpacityForHomeLauncher(
      display_.work_area().y(), home_launcher_opacity,
      base::nullopt /*metrics_reporter_info*/, base::NullCallback());

  if (!active_window_) {
    RemoveObserversAndStopTracking();
    return;
  }

  // Explicitly exit split view if two windows are snapped.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (is_final_state_show && split_view_controller->state() ==
                                 SplitViewController::State::kBothSnapped) {
    split_view_controller->EndSplitView();
  }

  if (is_final_state_show) {
    std::vector<aura::Window*> windows_to_hide_minimize;
    windows_to_hide_minimize.push_back(GetActiveWindow());

    if (secondary_window_)
      windows_to_hide_minimize.push_back(GetSecondaryWindow());

    // Minimize the hidden windows so they can be used normally with alt+tab
    // and overview. Minimize in reverse order to preserve mru ordering.
    windows_to_hide_minimize.resize(windows_to_hide_minimize.size() +
                                    hidden_windows_.size());
    std::copy(hidden_windows_.rbegin(), hidden_windows_.rend(),
              windows_to_hide_minimize.end() - hidden_windows_.size());
    window_util::HideAndMaybeMinimizeWithoutAnimation(windows_to_hide_minimize,
                                                      /*minimize=*/true);
  } else {
    // Reshow all windows previously hidden.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Show();
    }
  }

  active_window_->ResetOpacityAndTransform();
  if (secondary_window_)
    secondary_window_->ResetOpacityAndTransform();

  // Update the backdrop last as the backdrop controller listens for some
  // state changes like minimizing above which may also alter the backdrop.
  aura::Window* backdrop_window = GetBackdropWindow(GetActiveWindow());
  if (backdrop_window) {
    backdrop_window->SetTransform(gfx::Transform());
    backdrop_window->layer()->SetOpacity(1.f);
  }

  RemoveObserversAndStopTracking();
}

bool HomeLauncherGestureHandler::IsAnimating() {
  if (active_window_ && active_window_->IsAnimating())
    return true;

  if (secondary_window_ && secondary_window_->IsAnimating())
    return true;

  if (overview_active_on_gesture_start_ &&
      Shell::Get()->overview_controller()->InOverviewSession() &&
      (Shell::Get()->overview_controller()->IsInStartAnimation() ||
       animating_to_close_overview_)) {
    return true;
  }

  return false;
}

void HomeLauncherGestureHandler::AnimateToFinalState(AnimationTrigger trigger) {
  const bool is_final_state_show = IsFinalStateShow();
  UpdateWindowsForSlideUpOrDown(is_final_state_show ? 1.0 : 0.0,
                                trigger /**animation_trigger*/);

  if (!is_final_state_show && mode_ == Mode::kSlideDownToHide) {
    if (GetActiveWindow())
      wm::ActivateWindow(GetActiveWindow());
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeLauncherToMRUWindow"));
  } else if (is_final_state_show && mode_ == Mode::kSlideUpToShow) {
    base::RecordAction(
        base::UserMetricsAction("AppList_CurrentWindowToHomeLauncher"));
  }
}

void HomeLauncherGestureHandler::UpdateSettings(
    ui::ScopedLayerAnimationSettings* settings) {
  auto duration_ms = kActivationChangedAnimationDurationMs;
  if (IsDragInProgress())
    duration_ms = kAnimationDurationMs;

  HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
  duration_ms = home_screen_delegate->GetOptionalAnimationDuration().value_or(
      duration_ms);

  settings->SetTransitionDuration(duration_ms);
  settings->SetTweenType(IsDragInProgress() ? gfx::Tween::LINEAR
                                            : gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

void HomeLauncherGestureHandler::UpdateWindowsForSlideUpOrDown(
    double progress,
    base::Optional<AnimationTrigger> animation_trigger) {
  // Update full screen applist.
  DCHECK(display_.is_valid());
  const gfx::Rect work_area = display_.work_area();
  const int y_position =
      gfx::Tween::IntValueBetween(progress, work_area.bottom(), work_area.y());
  const float opacity = gfx::Tween::FloatValueBetween(progress, 0.f, 1.f);
  HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
  DCHECK(home_screen_delegate);

  // Before updating position and opacity for home launcher, make sure its
  // window is visible. This handles the case that
  // HomeScreenController::Show() put launcher window hidden when it is called
  // within an overview session. See https://crbug.com/996384
  aura::Window* home_screen_window =
      home_screen_delegate->GetHomeScreenWindow();
  DCHECK(home_screen_window);
  if (!home_screen_window->TargetVisibility())
    home_screen_delegate->GetHomeScreenWindow()->Show();

  const bool animate = animation_trigger.has_value();
  if (animate) {
    home_screen_delegate->UpdateYPositionAndOpacityForHomeLauncher(
        y_position, opacity,
        HomeScreenDelegate::AnimationInfo(*animation_trigger,
                                          progress == 1.0 /*showing*/),
        base::BindRepeating(&HomeLauncherGestureHandler::UpdateSettings,
                            base::Unretained(this)));
  } else {
    home_screen_delegate->UpdateYPositionAndOpacityForHomeLauncher(
        y_position, opacity, base::nullopt /*animation_info*/,
        base::NullCallback());
  }

  // Update the overview grid if needed. If |active_window_| is null, then
  // observe the animation of a window in overview.
  OverviewController* controller = Shell::Get()->overview_controller();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> overview_settings;
  if (overview_active_on_gesture_start_ && controller->InOverviewSession()) {
    DCHECK_EQ(mode_, Mode::kSlideUpToShow);
    const int inverted_y_position = gfx::Tween::IntValueBetween(
        progress, work_area.y(), work_area.bottom());
    overview_settings =
        controller->overview_session()->UpdateGridAtLocationYPositionAndOpacity(
            display_.id(), inverted_y_position, 1.f - opacity,
            animate ? base::BindRepeating(
                          &HomeLauncherGestureHandler::UpdateSettings,
                          base::Unretained(this))
                    : base::NullCallback());
    if (animate && progress == 1.0)
      animating_to_close_overview_ = true;
  }

  if (!active_window_) {
    if (overview_settings)
      overview_settings->AddObserver(this);
    return;
  }

  // Helper to update a single window's opacity and transform based on by
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
      // overview and the stored windows). We only want to act on one
      // animation end, so only observe one of the animations, which is
      // |active_window_|.
      UpdateSettings(settings.get());
      if (this->GetActiveWindow() == window)
        settings->AddObserver(this);
    }
    window->layer()->SetOpacity(opacity);
    window->SetTransform(transform);
  };

  aura::Window* backdrop_window = GetBackdropWindow(GetActiveWindow());
  if (backdrop_window && backdrop_values_) {
    update_windows_helper(progress, animate, backdrop_window,
                          *backdrop_values_);
  }

  aura::Window* divider_window = GetDividerWindow();
  if (divider_window && divider_values_) {
    update_windows_helper(progress, animate, divider_window, *divider_values_);
  }

  if (secondary_window_) {
    for (const auto& descendant :
         secondary_window_->transient_descendants_values()) {
      update_windows_helper(progress, animate, descendant.first,
                            descendant.second);
    }
    update_windows_helper(progress, animate, GetSecondaryWindow(),
                          secondary_window_->window_values());
  }

  if (active_window_) {
    for (const auto& descendant :
         active_window_->transient_descendants_values()) {
      update_windows_helper(progress, animate, descendant.first,
                            descendant.second);
    }
    update_windows_helper(progress, animate, GetActiveWindow(),
                          active_window_->window_values());
  }
}

void HomeLauncherGestureHandler::RemoveObserversAndStopTracking() {
  display_.set_id(display::kInvalidDisplayId);
  backdrop_values_ = base::nullopt;
  scoped_backdrop_update_pause_ = base::nullopt;
  divider_values_ = base::nullopt;
  last_event_location_ = base::nullopt;
  last_scroll_y_ = 0.f;
  mode_ = Mode::kNone;
  animating_to_close_overview_ = false;

  for (auto* window : hidden_windows_)
    window->RemoveObserver(this);
  hidden_windows_.clear();

  if (active_window_)
    GetActiveWindow()->RemoveObserver(this);
  active_window_.reset();

  if (secondary_window_)
    GetSecondaryWindow()->RemoveObserver(this);
  secondary_window_.reset();
}

bool HomeLauncherGestureHandler::IsIdle() {
  return !IsDragInProgress() && !IsAnimating();
}

bool HomeLauncherGestureHandler::IsFinalStateShow() {
  DCHECK_NE(Mode::kNone, mode_);
  DCHECK(display_.is_valid());

  // If fling velocity is greater than the threshold, show the launcher if
  // sliding up, or hide the launcher if sliding down, regardless of
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
  if (mode == Mode::kSwipeHomeToOverview) {
    active_window_.reset();
    return Shell::Get()->home_screen_controller()->IsHomeScreenVisible();
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  overview_active_on_gesture_start_ =
      Shell::Get()->overview_controller()->InOverviewSession();
  const bool split_view_active = split_view_controller->InSplitViewMode();
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  if (window && (mode != Mode::kSlideDownToHide ||
                 overview_active_on_gesture_start_ || split_view_active)) {
    active_window_.reset();
    return false;
  }

  if (window && !windows.empty() && windows[0] != window &&
      windows[0]->IsVisible()) {
    // Do not run slide down animation for the |window| if another active
    // window in mru list exists. Windows minimized in clamshell mode may
    // have opacity of 0, so set them to 1 to ensure visibility.
    if (WindowState::Get(window)->IsMinimized())
      window->layer()->SetOpacity(1.f);
    active_window_.reset();
    return false;
  }

  if (IsTabletMode() && overview_active_on_gesture_start_ &&
      !split_view_active && mode == Mode::kSlideUpToShow) {
    active_window_.reset();
    return true;
  }

  // Always hide split view windows if they exist. Otherwise, hide the
  // specified window if it is not null. If none of above is true, we want
  // the first window in the mru list, if it exists and is usable.
  aura::Window* first_window =
      split_view_active
          ? split_view_controller->GetDefaultSnappedWindow()
          : (window ? window : (windows.empty() ? nullptr : windows[0]));
  if (!CanProcessWindow(first_window, mode)) {
    active_window_.reset();
    return false;
  }

  DCHECK(base::Contains(windows, first_window));
  DCHECK_NE(Mode::kNone, mode);
  base::RecordAction(base::UserMetricsAction(
      mode == Mode::kSlideDownToHide
          ? "AppList_HomeLauncherToMRUWindowAttempt"
          : "AppList_CurrentWindowToHomeLauncherAttempt"));
  active_window_ = std::make_unique<ScopedWindowModifier>(first_window);
  GetActiveWindow()->AddObserver(this);
  base::EraseIf(windows, [this](aura::Window* elem) {
    return elem == this->GetActiveWindow();
  });

  // Alter a second window if we are in split view mode with two windows
  // snapped.
  if (mode == Mode::kSlideUpToShow &&
      split_view_controller->state() ==
          SplitViewController::State::kBothSnapped) {
    DCHECK_GT(windows.size(), 0u);
    aura::Window* second_window =
        split_view_controller->default_snap_position() ==
                SplitViewController::LEFT
            ? split_view_controller->right_window()
            : split_view_controller->left_window();
    DCHECK(base::Contains(windows, second_window));
    secondary_window_ = std::make_unique<ScopedWindowModifier>(second_window);
    GetSecondaryWindow()->AddObserver(this);
    base::EraseIf(windows, [this](aura::Window* elem) {
      return elem == this->GetSecondaryWindow();
    });
  }

  // Hide all visible windows which are behind our window so that when we
  // scroll, the home launcher will be visible in kSlideUpToShow case. This is
  // only needed when swiping up, and not when overview mode is active.
  hidden_windows_.clear();
  if (mode == Mode::kSlideUpToShow && !overview_active_on_gesture_start_) {
    for (auto* window : windows) {
      if (window->IsVisible()) {
        hidden_windows_.push_back(window);
        window->AddObserver(this);
      }
    }
    window_util::HideAndMaybeMinimizeWithoutAnimation(hidden_windows_,
                                                      /*minimize=*/false);
  }

  // Show |active_window_| if we are swiping down to hide.
  if (mode == Mode::kSlideDownToHide) {
    ScopedAnimationDisabler disable(GetActiveWindow());
    GetActiveWindow()->Show();

    // When |active_window_| has a modal dialog child, active_window_->Show()
    // above would cancel the current gesture and trigger OnReleaseEvent() to
    // reset |active_window_|.
    if (!active_window_ || !GetActiveWindow())
      return false;

    GetActiveWindow()->layer()->SetOpacity(1.f);
  }

  const gfx::RectF work_area = gfx::RectF(
      screen_util::GetDisplayWorkAreaBoundsInParent(GetActiveWindow()));
  const gfx::RectF target_work_area = GetOffscreenWorkspaceBounds(work_area);

  active_window_->ComputeWindowValues(work_area, target_work_area);
  if (secondary_window_)
    secondary_window_->ComputeWindowValues(work_area, target_work_area);

  aura::Window* backdrop_window = GetBackdropWindow(GetActiveWindow());
  if (backdrop_window) {
    // Store the values needed to transform the backdrop. The backdrop
    // actually covers the area behind the shelf as well, so initially
    // transform it to be sized to the work area. Without the transform
    // tweak, there is an extra shelf sized black area under |active_window_|.
    // Go to 0.01 opacity instead of 0 opacity otherwise animation end code
    // will attempt to update the backdrop which will try to show a 0 opacity
    // window which causes a crash.
    backdrop_values_ = base::make_optional(WindowValues());
    backdrop_values_->initial_opacity = 1.f;
    backdrop_values_->initial_transform = gfx::Transform(
        1.f, 0.f, 0.f,
        work_area.height() /
            static_cast<float>(backdrop_window->bounds().height()),
        0.f, 0.f);
    backdrop_values_->target_opacity = 0.01f;
    backdrop_values_->target_transform = gfx::TransformBetweenRects(
        gfx::RectF(backdrop_window->bounds()), target_work_area);
  }

  // Stores values needed to transform the split view divider if it exists.
  aura::Window* divider_window = GetDividerWindow();
  if (divider_window) {
    divider_values_ = base::make_optional(WindowValues());
    divider_values_->initial_opacity = 1.f;
    divider_values_->initial_transform = gfx::Transform();
    divider_values_->target_opacity = 0.f;
    divider_values_->target_transform = gfx::TransformBetweenRects(
        gfx::RectF(divider_window->bounds()),
        GetOffscreenWindowBounds(divider_window, work_area, target_work_area));
  }

  return true;
}

void HomeLauncherGestureHandler::OnDragStarted(const gfx::PointF& location) {
  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_ =
        std::make_unique<SwipeHomeToOverviewController>(display_.id());
  } else {
    const double progress = mode_ == Mode::kSlideUpToShow ? 0 : 1;
    NotifyHomeLauncherPositionChanged(100 * progress, display_.id());
    HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
    DCHECK(home_screen_delegate);
    home_screen_delegate->OnHomeLauncherDragStart();

    PauseBackdropUpdatesForActiveWindow();
    UpdateWindowsForSlideUpOrDown(progress,
                                  base::nullopt /*animation_trigger*/);
  }
}

void HomeLauncherGestureHandler::OnDragContinued(const gfx::PointF& location,
                                                 float scroll_x,
                                                 float scroll_y) {
  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_->Drag(location, scroll_x, scroll_y);
  } else {
    const double progress =
        GetHeightInWorkAreaAsRatio(location, display_.work_area());
    HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
    DCHECK(home_screen_delegate);
    home_screen_delegate->OnHomeLauncherDragInProgress();

    UpdateWindowsForSlideUpOrDown(progress,
                                  base::nullopt /*animation_trigger*/);
    const int percent_shown = static_cast<int>(progress * 100);
    NotifyHomeLauncherPositionChanged(percent_shown, display_.id());
  }
}

bool HomeLauncherGestureHandler::OnDragEnded(const gfx::PointF& location,
                                             base::Optional<float> velocity_y) {
  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_->EndDrag(location, velocity_y);
    RemoveObserversAndStopTracking();
  } else {
    // In clamshell mode, AppListView::SetIsInDrag is called explicitly so it
    // does not need the notification from HomeLauncherGestureHandler.
    if (IsTabletMode()) {
      HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
      DCHECK(home_screen_delegate);
      home_screen_delegate->OnHomeLauncherDragEnd();
    }

    if (!IsDragInProgress()) {
      if (GetActiveWindow()) {
        // |active_window_| may not be nullptr when this release event is
        // triggered by opening |active_window_| with modal dialog in
        // OnPressEvent(). In that case, just leave the |active_window_| in
        // show state and stop tracking.
        AnimateToFinalState(AnimationTrigger::kDragRelease);
        RemoveObserversAndStopTracking();
        return true;
      }
      return false;
    }

    last_event_location_ = base::make_optional(location);
    AnimateToFinalState(AnimationTrigger::kDragRelease);
  }

  return true;
}

void HomeLauncherGestureHandler::OnDragCancelled() {
  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_->CancelDrag();
    RemoveObserversAndStopTracking();
  } else {
    HomeScreenDelegate* home_screen_delegate = GetHomeScreenDelegate();
    DCHECK(home_screen_delegate);
    home_screen_delegate->OnHomeLauncherDragEnd();

    AnimateToFinalState(AnimationTrigger::kDragRelease);
  }
}

void HomeLauncherGestureHandler::PauseBackdropUpdatesForActiveWindow() {
  if (scoped_backdrop_update_pause_.has_value())
    return;
  aura::Window* active_window = GetActiveWindow();
  if (!active_window)
    return;
  scoped_backdrop_update_pause_ =
      GetWorkspaceControllerForContext(active_window)
          ->layout_manager()
          ->backdrop_controller()
          ->PauseUpdates();
}

}  // namespace ash
