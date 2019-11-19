// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <cmath>
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Three fixed position ratios of the divider, which means the divider can
// always be moved to these three positions.
constexpr float kFixedPositionRatios[] = {0.f, 0.5f, 1.0f};

// Two optional position ratios of the divider. Whether the divider can be moved
// to these two positions depends on the minimum size of the snapped windows.
constexpr float kOneThirdPositionRatio = 0.33f;
constexpr float kTwoThirdPositionRatio = 0.67f;

// The black scrim starts to fade in when the divider is moved past the two
// optional positions (kOneThirdPositionRatio, kTwoThirdPositionRatio) and
// reaches to its maximum opacity (kBlackScrimOpacity) after moving
// kBlackScrimFadeInRatio of the screen width. See https://crbug.com/827730 for
// details.
constexpr float kBlackScrimFadeInRatio = 0.1f;
constexpr float kBlackScrimOpacity = 0.4f;

// Histogram names that record presentation time of resize operation with
// following conditions, a) single snapped window, empty overview, b) two
// snapped windows, c) single snapped window and non empty overview.
constexpr char kSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow";
constexpr char kSplitViewResizeMultiHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow";
constexpr char kSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview";

constexpr char kSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow";
constexpr char kSplitViewResizeMultiMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow";
constexpr char kSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview";

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(
      base::ClampToRange(location_in_screen.x(), bounds_in_screen.x(),
                         bounds_in_screen.right() - 1),
      base::ClampToRange(location_in_screen.y(), bounds_in_screen.y(),
                         bounds_in_screen.bottom() - 1));
}

WindowStateType GetStateTypeFromSnapPosition(
    SplitViewController::SnapPosition snap_position) {
  DCHECK(snap_position != SplitViewController::NONE);
  if (snap_position == SplitViewController::LEFT)
    return WindowStateType::kLeftSnapped;
  if (snap_position == SplitViewController::RIGHT)
    return WindowStateType::kRightSnapped;
  NOTREACHED();
  return WindowStateType::kDefault;
}

// Returns the minimum size of the window according to the screen orientation.
int GetMinimumWindowSize(aura::Window* window, bool is_landscape) {
  int minimum_width = 0;
  if (window && window->delegate()) {
    gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    minimum_width = is_landscape ? minimum_size.width() : minimum_size.height();
  }
  return minimum_width;
}

// Returns true if |window| is currently snapped.
bool IsSnapped(aura::Window* window) {
  if (!window)
    return false;
  return WindowState::Get(window)->IsSnapped();
}

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
OverviewSession* GetOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession()
             ? Shell::Get()->overview_controller()->overview_session()
             : nullptr;
}

}  // namespace

// The window observer that observes the current tab-dragged window. When it's
// created, it observes the dragged window, and there are two possible results
// after the user finishes dragging: 1) the dragged window stays a new window
// and SplitViewController needs to decide where to put the window; 2) the
// dragged window's tabs are attached into another browser window and thus is
// destroyed.
class SplitViewController::TabDraggedWindowObserver
    : public aura::WindowObserver {
 public:
  TabDraggedWindowObserver(
      SplitViewController* split_view_controller,
      aura::Window* dragged_window,
      SplitViewController::SnapPosition desired_snap_position,
      const gfx::Point& last_location_in_screen)
      : split_view_controller_(split_view_controller),
        dragged_window_(dragged_window),
        desired_snap_position_(desired_snap_position),
        last_location_in_screen_(last_location_in_screen) {
    DCHECK(window_util::IsDraggingTabs(dragged_window_));
    dragged_window_->AddObserver(this);
  }

  ~TabDraggedWindowObserver() override {
    if (dragged_window_)
      dragged_window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    // At this point we know the newly created dragged window is going to be
    // destroyed due to all of its tabs are attaching into another window.
    EndTabDragging(window, /*is_being_destroyed=*/true);
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, dragged_window_);
    if (key == ash::kIsDraggingTabsKey &&
        !window_util::IsDraggingTabs(window)) {
      // At this point we know the newly created dragged window just finished
      // dragging.
      EndTabDragging(window, /*is_being_destroyed=*/false);
    }
  }

 private:
  // Called after the tab dragging is ended, the dragged window is either
  // destroyed because of merging into another window, or stays as a separate
  // window.
  void EndTabDragging(aura::Window* window, bool is_being_destroyed) {
    dragged_window_->RemoveObserver(this);
    dragged_window_ = nullptr;
    split_view_controller_->EndWindowDragImpl(window, is_being_destroyed,
                                              desired_snap_position_,
                                              last_location_in_screen_);

    // Update the source window's bounds if applicable.
    UpdateSourceWindowBoundsAfterDragEnds(window);
  }

  // The source window might have been scaled down during dragging, we should
  // update its bounds to ensure it has the right bounds after the drag ends.
  void UpdateSourceWindowBoundsAfterDragEnds(aura::Window* window) {
    aura::Window* source_window =
        window->GetProperty(ash::kTabDraggingSourceWindowKey);
    if (source_window) {
      TabletModeWindowState::UpdateWindowPosition(
          WindowState::Get(source_window), /*animate=*/true);
    }
  }

  SplitViewController* split_view_controller_;
  aura::Window* dragged_window_;
  SplitViewController::SnapPosition desired_snap_position_;
  gfx::Point last_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(TabDraggedWindowObserver);
};

// Animates the divider to its closest fixed position.
// SplitViewController::is_resizing_ is assumed to be already set to false
// before this animation starts, but some resizing logic is delayed until this
// animation ends.
class SplitViewController::DividerSnapAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  DividerSnapAnimation(SplitViewController* split_view_controller,
                       int starting_position,
                       int ending_position)
      : gfx::SlideAnimation(this),
        split_view_controller_(split_view_controller),
        starting_position_(starting_position),
        ending_position_(ending_position) {
    // Before you change this value, read the comment on kIsWindowMovedTimeoutMs
    // in tablet_mode_window_drag_delegate.cc.
    SetSlideDuration(base::TimeDelta::FromMilliseconds(300));
    SetTweenType(gfx::Tween::EASE_IN);
  }

  ~DividerSnapAnimation() override = default;

  int ending_position() const { return ending_position_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->is_resizing_);
    DCHECK_EQ(ending_position_, split_view_controller_->divider_position_);

    split_view_controller_->EndResizeImpl();
    split_view_controller_->EndSplitViewAfterResizingIfAppropriate();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->is_resizing_);

    split_view_controller_->divider_position_ =
        CurrentValueBetween(starting_position_, ending_position_);
    split_view_controller_->NotifyDividerPositionChanged();
    split_view_controller_->UpdateSnappedWindowsAndDividerBounds();
    // Updating the window may stop animation.
    if (is_animating())
      split_view_controller_->SetWindowsTransformDuringResizing();
  }

  SplitViewController* split_view_controller_;
  int starting_position_;
  int ending_position_;
};

// static
SplitViewController* SplitViewController::Get(const aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  DCHECK(RootWindowController::ForWindow(window));
  DCHECK(RootWindowController::ForWindow(Shell::GetPrimaryRootWindow()));
  return RootWindowController::ForWindow(
             AreMultiDisplayOverviewAndSplitViewEnabled()
                 ? window
                 : Shell::GetPrimaryRootWindow())
      ->split_view_controller();
}

SplitViewController::SplitViewController(aura::Window* root_window)
    : root_window_(root_window) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  if (IsClamshellSplitViewModeEnabled()) {
    split_view_type_ = Shell::Get()->tablet_mode_controller()->InTabletMode()
                           ? SplitViewType::kTabletType
                           : SplitViewType::kClamshellType;
  }
}

SplitViewController::~SplitViewController() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (Shell::Get()->accessibility_controller())
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  EndSplitView();
}

bool SplitViewController::InSplitViewMode() const {
  return InClamshellSplitViewMode() || InTabletSplitViewMode();
}

bool SplitViewController::InClamshellSplitViewMode() const {
  return state_ != State::kNoSnap &&
         split_view_type_ == SplitViewType::kClamshellType;
}

bool SplitViewController::InTabletSplitViewMode() const {
  return state_ != State::kNoSnap &&
         split_view_type_ == SplitViewType::kTabletType;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position,
                                     bool use_divider_spawn_animation) {
  DCHECK(window && CanSnapInSplitview(window));
  DCHECK_NE(snap_position, NONE);
  DCHECK(!is_resizing_);
  DCHECK(!IsDividerAnimating());
  DCHECK_EQ(root_window_, window->GetRootWindow());

  UpdateSnappingWindowTransformedBounds(window);
  RemoveWindowFromOverviewIfApplicable(window);

  bool do_divider_spawn_animation = false;
  if (state_ == State::kNoSnap) {
    // Add observers when the split view mode starts.
    Shell::Get()->AddShellObserver(this);
    Shell::Get()->overview_controller()->AddObserver(this);
    Shell::Get()->activation_client()->AddObserver(this);

    // If there is pre-set |divider_position_|, use it. It can happen during
    // tablet <-> clamshell transition or multi-user transition.
    divider_position_ = (divider_position_ < 0) ? GetDefaultDividerPosition()
                                                : divider_position_;
    default_snap_position_ = snap_position;

    // There is no divider bar in clamshell splitview mode.
    if (split_view_type_ == SplitViewType::kTabletType) {
      split_view_divider_ = std::make_unique<SplitViewDivider>(this);
      // The divider spawn animation adds a finishing touch to the |window|
      // animation that generally accommodates snapping by dragging, but if
      // |window| is currently minimized then it will undergo the unminimizing
      // animation instead. Therefore skip the divider spawn animation if
      // |window| is minimized.
      if (use_divider_spawn_animation &&
          !WindowState::Get(window)->IsMinimized()) {
        // For the divider spawn animation, at the end of the delay, the divider
        // shall be visually aligned with an edge of |window|. This effect will
        // be more easily achieved after |window| has been snapped and the
        // corresponding transform animation has begun. So for now, just set a
        // flag to indicate that the divider spawn animation should be done.
        do_divider_spawn_animation = true;
      }
    }
    splitview_start_time_ = base::Time::Now();
  }

  aura::Window* previous_snapped_window = nullptr;
  if (snap_position == LEFT) {
    if (left_window_ != window) {
      previous_snapped_window = left_window_;
      StopObserving(LEFT);
      left_window_ = window;
    }
    if (right_window_ == window) {
      right_window_ = nullptr;
      default_snap_position_ = LEFT;
    }
  } else if (snap_position == RIGHT) {
    if (right_window_ != window) {
      previous_snapped_window = right_window_;
      StopObserving(RIGHT);
      right_window_ = window;
    }
    if (left_window_ == window) {
      left_window_ = nullptr;
      default_snap_position_ = RIGHT;
    }
  }
  StartObserving(window);

  // Insert the previous snapped window to overview if overview is active.
  if (previous_snapped_window && GetOverviewSession()) {
    InsertWindowToOverview(previous_snapped_window);
    // Ensure that the close icon will fade in. This part is redundant for
    // dragging from overview, but necessary for dragging from the top. For
    // dragging from overview, |OverviewItem::OnSelectorItemDragEnded| will be
    // called on all overview items including the |previous_snapped_window|
    // item anyway, whereas for dragging from the top,
    // |OverviewItem::OnSelectorItemDragEnded| already was called on all
    // overview items and |previous_snapped_window| was not yet among them.
    GetOverviewSession()
        ->GetOverviewItemForWindow(previous_snapped_window)
        ->OnSelectorItemDragEnded(/*snap=*/true);
  }

  // Update the divider position and window bounds before snapping a new
  // window. Since the minimum size of |window| maybe larger than currently
  // bounds in |snap_position|.
  if (state_ != State::kNoSnap &&
      split_view_type_ == SplitViewType::kTabletType) {
    divider_position_ = GetClosestFixedDividerPosition();
    UpdateSnappedWindowsAndDividerBounds();
  }

  // Disable the bounds change animation for a to-be-snapped window if the
  // window has an un-identity transform. We'll do transform animation for the
  // window in OnWindowSnapped() function.
  std::unique_ptr<ScopedAnimationDisabler> animation_disabler;
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end())
    animation_disabler = std::make_unique<ScopedAnimationDisabler>(window);

  if (WindowState::Get(window)->GetStateType() ==
      GetStateTypeFromSnapPosition(snap_position)) {
    // Update its snapped bounds as its bounds may not be the expected snapped
    // bounds here.
    const WMEvent event((snap_position == LEFT) ? WM_EVENT_SNAP_LEFT
                                                : WM_EVENT_SNAP_RIGHT);
    WindowState::Get(window)->OnWMEvent(&event);

    OnWindowSnapped(window);
  } else {
    // Otherwise, try to snap it first. The split view state will be updated
    // after the window is snapped.
    const WMEvent event((snap_position == LEFT) ? WM_EVENT_SNAP_LEFT
                                                : WM_EVENT_SNAP_RIGHT);
    WindowState::Get(window)->OnWMEvent(&event);
  }

  if (do_divider_spawn_animation) {
    DCHECK(window->layer()->GetAnimator()->GetTargetTransform().IsIdentity());

    const gfx::Rect bounds =
        GetSnappedWindowBoundsInScreen(snap_position, window);
    // Get one of the two corners of |window| that meet the divider.
    gfx::Point p = IsPhysicalLeftOrTop(snap_position) ? bounds.bottom_right()
                                                      : bounds.origin();
    // Apply the transform that |window| will undergo when the divider spawns.
    static const double value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_SLOW_IN,
        kSplitviewDividerSpawnDelay.InMillisecondsF() /
            kSplitviewWindowTransformDuration.InMillisecondsF());
    gfx::TransformAboutPivot(bounds.origin(),
                             gfx::Tween::TransformValueBetween(
                                 value, window->transform(), gfx::Transform()))
        .TransformPoint(&p);
    // Use a coordinate of the transformed |window| corner for spawn_position.
    split_view_divider_->DoSpawningAnimation(
        IsCurrentScreenOrientationLandscape() ? p.x() : p.y());
  }

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::SwapWindows() {
  DCHECK(InSplitViewMode());

  // Ignore |is_resizing_| because it will be true in case of double tapping
  // (not double clicking) the divider without ever actually dragging it
  // anywhere. Double tapping the divider triggers StartResize(), EndResize(),
  // StartResize(), SwapWindows(), EndResize(). Double clicking the divider
  // (possible by using the emulator or chrome://flags/#force-tablet-mode)
  // triggers StartResize(), EndResize(), StartResize(), EndResize(),
  // SwapWindows(). Those two sequences of function calls are what were mainly
  // considered in writing the condition for bailing out here, to disallow
  // swapping windows when the divider is being dragged or is animating.
  if (IsDividerAnimating())
    return;

  aura::Window* new_left_window = right_window_;
  aura::Window* new_right_window = left_window_;
  left_window_ = new_left_window;
  right_window_ = new_right_window;

  // Update |default_snap_position_| if necessary.
  if (!left_window_ || !right_window_)
    default_snap_position_ = left_window_ ? LEFT : RIGHT;

  divider_position_ = GetClosestFixedDividerPosition();
  UpdateSnappedWindowsAndDividerBounds();
  UpdateStateAndNotifyObservers();

  base::RecordAction(
      base::UserMetricsAction("SplitView_DoubleTapDividerSwapWindows"));
}

aura::Window* SplitViewController::GetDefaultSnappedWindow() {
  if (default_snap_position_ == LEFT)
    return left_window_;
  if (default_snap_position_ == RIGHT)
    return right_window_;
  return nullptr;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  gfx::Rect bounds =
      GetSnappedWindowBoundsInScreen(snap_position, window_for_minimum_size);
  ::wm::ConvertRectFromScreen(root_window_, &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (snap_position == NONE)
    return work_area_bounds_in_screen;

  const bool landscape = IsCurrentScreenOrientationLandscape();
  const bool snap_left_or_top = IsPhysicalLeftOrTop(snap_position);
  const bool in_tablet = Shell::Get()->tablet_mode_controller()->InTabletMode();
  const int work_area_size = GetDividerEndPosition();
  const int divider_position =
      divider_position_ < 0 ? GetDefaultDividerPosition() : divider_position_;

  int window_size;
  if (snap_left_or_top) {
    window_size = divider_position;
  } else {
    window_size = work_area_size - divider_position;
    // In tablet mode, there is a divider widget of which |divider_position|
    // refers to the left or top, and so we should subtract the thickness.
    if (in_tablet)
      window_size -= kSplitviewDividerShortSideLength;
  }

  const int minimum = GetMinimumWindowSize(window_for_minimum_size, landscape);
  DCHECK(window_for_minimum_size || minimum == 0);
  if (window_size < minimum) {
    if (in_tablet && !is_resizing_) {
      // If |window_for_minimum_size| really gets snapped, then the divider will
      // be adjusted to its default position. Compute |window_size| accordingly.
      window_size = work_area_size / 2 - kSplitviewDividerShortSideLength / 2;
      // If |work_area_size| is odd, then the default divider position is
      // rounded down, toward the left or top, but then if |snap_left_or_top| is
      // false, that means |window_size| should now be rounded up.
      if (!snap_left_or_top && work_area_size % 2 == 1)
        ++window_size;
    } else {
      window_size = minimum;
    }
  }

  // Get the parameter values for which |gfx::Rect::SetByBounds| would recreate
  // |work_area_bounds_in_screen|.
  int left = work_area_bounds_in_screen.x();
  int top = work_area_bounds_in_screen.y();
  int right = work_area_bounds_in_screen.right();
  int bottom = work_area_bounds_in_screen.bottom();

  // Make |snapped_window_bounds_in_screen| by modifying one of the above four
  // values: the one that represents the inner edge of the snapped bounds.
  int& left_or_top = landscape ? left : top;
  int& right_or_bottom = landscape ? right : bottom;
  if (snap_left_or_top)
    right_or_bottom = left_or_top + window_size;
  else
    left_or_top = right_or_bottom - window_size;

  gfx::Rect snapped_window_bounds_in_screen;
  snapped_window_bounds_in_screen.SetByBounds(left, top, right, bottom);
  return snapped_window_bounds_in_screen;
}

int SplitViewController::GetDefaultDividerPosition() const {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  int default_divider_position = (IsCurrentScreenOrientationLandscape()
                                      ? work_area_bounds_in_screen.width()
                                      : work_area_bounds_in_screen.height()) /
                                 2;
  if (split_view_type_ == SplitViewType::kTabletType)
    default_divider_position -= kSplitviewDividerShortSideLength / 2;
  return default_divider_position;
}

bool SplitViewController::IsDividerAnimating() const {
  return divider_snap_animation_ && divider_snap_animation_->is_animating();
}

void SplitViewController::StartResize(const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  // |is_resizing_| may be true here, because you can start dragging the divider
  // with a pointing device while already dragging it by touch, or vice versa.
  // It is possible by using the emulator or chrome://flags/#force-tablet-mode.
  // Bailing out here does not stop the user from dragging by touch and with a
  // pointing device simultaneously; it just avoids duplicate calls to
  // CreateDragDetails() and OnDragStarted(). We also bail out here if you try
  // to start dragging the divider during its snap animation.
  if (is_resizing_ || IsDividerAnimating())
    return;

  is_resizing_ = true;
  split_view_divider_->UpdateDividerBounds();
  previous_event_location_ = location_in_screen;

  for (auto* window : {left_window_, right_window_}) {
    if (window == nullptr)
      continue;
    WindowState* window_state = WindowState::Get(window);
    gfx::Point location_in_parent(location_in_screen);
    ::wm::ConvertPointFromScreen(window->parent(), &location_in_parent);
    int window_component = GetWindowComponentForResize(window);
    window_state->CreateDragDetails(location_in_parent, window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    window_state->OnDragStarted(window_component);
  }

  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
  if (state_ == State::kBothSnapped) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeMultiHistogram,
        kSplitViewResizeMultiMaxLatencyHistogram);
  } else if (GetOverviewSession() && !GetOverviewSession()->IsEmpty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeWithOverviewHistogram,
        kSplitViewResizeWithOverviewMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeSingleHistogram,
        kSplitViewResizeSingleMaxLatencyHistogram);
  }
}

void SplitViewController::Resize(const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  if (!is_resizing_)
    return;
  presentation_time_recorder_->RequestNext();
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // Update |divider_position_|.
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();

  // Update the black scrim layer's bounds and opacity.
  UpdateBlackScrim(modified_location_in_screen);

  // Update the snapped window/windows and divider's position.
  UpdateSnappedWindowsAndDividerBounds();

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewController::EndResize(const gfx::Point& location_in_screen) {
  presentation_time_recorder_.reset();
  DCHECK(InSplitViewMode());
  if (!is_resizing_)
    return;
  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();
  is_resizing_ = false;

  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();
  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsAndDividerBounds();

  const int target_divider_position = GetClosestFixedDividerPosition();
  if (divider_position_ == target_divider_position) {
    EndResizeImpl();
    EndSplitViewAfterResizingIfAppropriate();
  } else {
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, divider_position_, target_divider_position);
    divider_snap_animation_->Show();
  }
}

void SplitViewController::EndSplitView(EndReason end_reason) {
  if (!InSplitViewMode())
    return;

  end_reason_ = end_reason;

  // If we are currently in a resize but split view is ending, make sure to end
  // the resize. This can happen, for example, on the transition back to
  // clamshell mode or when a task is minimized during a resize. Likewise, if
  // split view is ending during the divider snap animation, then clean that up.
  const bool is_divider_animating = IsDividerAnimating();
  if (is_resizing_ || is_divider_animating) {
    is_resizing_ = false;
    if (is_divider_animating) {
      // Don't call StopAndShoveAnimatedDivider as it will call observers.
      divider_snap_animation_->Stop();
      divider_position_ = divider_snap_animation_->ending_position();
    }
    EndResizeImpl();
  }

  // Remove observers when the split view mode ends.
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);

  StopObserving(LEFT);
  StopObserving(RIGHT);
  black_scrim_layer_.reset();
  default_snap_position_ = NONE;
  divider_position_ = -1;
  divider_closest_ratio_ = std::numeric_limits<float>::quiet_NaN();
  snapping_window_transformed_bounds_map_.clear();

  UpdateStateAndNotifyObservers();
  // Close splitview divider widget after updating state so that
  // OnDisplayMetricsChanged triggered by the widget closing correctly
  // finds out !InSplitViewMode().
  split_view_divider_.reset();
  base::RecordAction(base::UserMetricsAction("SplitView_EndSplitView"));
  UMA_HISTOGRAM_LONG_TIMES("Ash.SplitView.TimeInSplitView",
                           base::Time::Now() - splitview_start_time_);
}

bool SplitViewController::IsWindowInSplitView(
    const aura::Window* window) const {
  return window && (window == left_window_ || window == right_window_);
}

void SplitViewController::InitDividerPositionForTransition(
    int divider_position) {
  // This should only be called before the actual carry-over happens.
  DCHECK(!InSplitViewMode());
  DCHECK_EQ(divider_position_, -1);
  divider_position_ = divider_position;
}

void SplitViewController::OnWindowDragStarted(aura::Window* dragged_window) {
  DCHECK(dragged_window);
  if (IsWindowInSplitView(dragged_window))
    OnSnappedWindowDetached(dragged_window, /*window_drag=*/true);

  // OnSnappedWindowDetached() may end split view mode.
  if (split_view_divider_)
    split_view_divider_->OnWindowDragStarted(dragged_window);
}

void SplitViewController::OnWindowDragEnded(
    aura::Window* dragged_window,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (window_util::IsDraggingTabs(dragged_window)) {
    dragged_window_observer_.reset(new TabDraggedWindowObserver(
        this, dragged_window, desired_snap_position, last_location_in_screen));
  } else {
    EndWindowDragImpl(dragged_window, /*is_being_destroyed=*/false,
                      desired_snap_position, last_location_in_screen);
  }
}

void SplitViewController::AddObserver(SplitViewObserver* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(SplitViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  // If the window's resizibility property changes (must from resizable ->
  // unresizable), end the split view mode and also end overview mode if
  // overview mode is active at the moment.
  if (key == aura::client::kResizeBehaviorKey && !CanSnapInSplitview(window)) {
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview();
    ShowAppCannotSnapToast();
  }
}

void SplitViewController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(root_window_, window->GetRootWindow());

  if (split_view_type_ != SplitViewType::kClamshellType ||
      reason == ui::PropertyChangeReason::FROM_ANIMATION ||
      !InSplitViewMode()) {
    return;
  }

  WindowState* window_state = WindowState::Get(window);
  const bool is_window_moved = window_state->is_dragged() &&
                               window_state->drag_details()->bounds_change ==
                                   WindowResizer::kBoundsChange_Repositions;
  if (is_window_moved) {
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview();
    return;
  }

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (IsCurrentScreenOrientationLandscape()) {
    divider_position_ = window == GetPhysicalLeftOrTopWindow()
                            ? new_bounds.width()
                            : work_area.width() - new_bounds.width();
  } else {
    divider_position_ = window == GetPhysicalLeftOrTopWindow()
                            ? new_bounds.height()
                            : work_area.height() - new_bounds.height();
  }
  NotifyDividerPositionChanged();
}

void SplitViewController::OnWindowDestroyed(aura::Window* window) {
  DCHECK(InSplitViewMode());
  DCHECK(IsWindowInSplitView(window));
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end())
    snapping_window_transformed_bounds_map_.erase(iter);
  OnSnappedWindowDetached(window, /*window_drag=*/false);
}

void SplitViewController::OnResizeLoopStarted(aura::Window* window) {
  if (split_view_type_ != SplitViewType::kClamshellType || !InSplitViewMode()) {
    return;
  }

  // In clamshell mode, if splitview is active (which means overview is active
  // at the same time), only the resize that happens on the window edge that's
  // next to the overview grid will resize the window and overview grid at the
  // same time. For the resize that happens on the other part of the window,
  // we'll just end splitview and overview mode.
  if (WindowState::Get(window)->drag_details()->window_component !=
      GetWindowComponentForResize(window)) {
    Shell::Get()->overview_controller()->EndOverview();
  }
}

void SplitViewController::OnResizeLoopEnded(aura::Window* window) {
  if (split_view_type_ != SplitViewType::kClamshellType || !InSplitViewMode()) {
    return;
  }

  if (divider_position_ < GetDividerEndPosition() * kOneThirdPositionRatio ||
      divider_position_ > GetDividerEndPosition() * kTwoThirdPositionRatio) {
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview();
    WindowState::Get(window)->Maximize();
  }
}

void SplitViewController::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    ash::WindowStateType old_type) {
  DCHECK_EQ(
      window_state->GetDisplay().id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_).id());

  if (window_state->IsSnapped()) {
    OnWindowSnapped(window_state->window());
  } else if (window_state->IsFullscreen() || window_state->IsMaximized()) {
    // End split view mode if one of the snapped windows gets maximized /
    // full-screened. Also end overview mode if overview mode is active at the
    // moment.
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview();
  } else if (window_state->IsMinimized()) {
    OnSnappedWindowDetached(window_state->window(), /*window_drag=*/false);

    if (!InSplitViewMode()) {
      // We have different behaviors for a minimized window: in tablet splitview
      // mode, we'll insert the minimized window back to overview, as normally
      // the window is not supposed to be minmized in tablet mode. And in
      // clamshell splitview mode, we respect the minimization of the window
      // and end overview instead.
      if (split_view_type_ == SplitViewType::kTabletType)
        InsertWindowToOverview(window_state->window());
      else
        Shell::Get()->overview_controller()->EndOverview();
    }
  }
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (!gained_active || gained_active->GetRootWindow() != root_window_)
    return;

  if (features::IsVirtualDesksEnabled() &&
      DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently used
    // window, but this should not result in snapping and ending overview mode
    // now. Overview will be ended explicitly as part of the desk activation
    // animation.
    return;
  }

  // If |gained_active| was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before OnWindowDestroying() is called. We should
  // not try to snap another window in this case.
  if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED)
    return;

  // Only windows that are in the MRU list and are not already in split view can
  // be auto-snapped.
  if (IsWindowInSplitView(gained_active) ||
      !base::Contains(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
          gained_active)) {
    return;
  }

  // We do not auto snap windows in clamshell splitview mode if a new window
  // is activated when clamshell splitview mode is active. In this case we'll
  // just end overview mode which will then end splitview mode.
  // TODO(xdai): Handle this logic in OverivewSession::OnWindowActivating().
  if (InClamshellSplitViewMode()) {
    Shell::Get()->overview_controller()->EndOverview();
    return;
  }

  DCHECK(InTabletSplitViewMode());

  // Do not snap the window if the activation change is caused by dragging a
  // window, or by dragging a tab. Note the two values WindowState::is_dragged()
  // and IsDraggingTabs() might not be exactly the same under certain
  // circumstance, e.g., when a tab is dragged out from a browser window, a new
  // browser window will be created for the dragged tab and then be activated,
  // and at that time, IsDraggingTabs() is true, but WindowState::is_dragged()
  // is still false. And later after the window drag starts,
  // WindowState::is_dragged() will then be true.
  if (WindowState::Get(gained_active)->is_dragged() ||
      window_util::IsDraggingTabs(gained_active)) {
    return;
  }

  // If the divider is animating, then |gained_active| cannot be snapped (and is
  // not already snapped either, because then we would have bailed out by now).
  // Then if |gained_active| is user-positionable, we should end split view
  // mode, but the cannot snap toast would be inappropriate because the user
  // still might be able to snap |gained_active|.
  if (IsDividerAnimating()) {
    if (WindowState::Get(gained_active)->IsUserPositionable())
      EndSplitView(EndReason::kUnsnappableWindowActivated);
    return;
  }

  // If it's a user positionable window but can't be snapped, end split view
  // mode and show the cannot snap toast.
  if (!CanSnapInSplitview(gained_active)) {
    if (WindowState::Get(gained_active)->IsUserPositionable()) {
      EndSplitView(EndReason::kUnsnappableWindowActivated);
      ShowAppCannotSnapToast();
    }
    return;
  }

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  SnapWindow(gained_active, (default_snap_position_ == LEFT) ? RIGHT : LEFT);
}

void SplitViewController::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Disable split view for pinned windows.
  if (WindowState::Get(pinned_window)->IsPinned() && InSplitViewMode())
    EndSplitView(EndReason::kUnsnappableWindowActivated);
}

void SplitViewController::OnOverviewModeStarting() {
  DCHECK(InSplitViewMode());

  // If split view mode is active, reset |state_| to make it be able to select
  // another window from overview window grid.
  if (default_snap_position_ == LEFT) {
    StopObserving(RIGHT);
  } else if (default_snap_position_ == RIGHT) {
    StopObserving(LEFT);
  }
  UpdateStateAndNotifyObservers();
}

void SplitViewController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  DCHECK(InSplitViewMode());

  // Early exit if overview is ended while swiping up on the shelf to avoid
  // snapping a window or showing a toast.
  if (overview_session->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    EndSplitView();
    return;
  }

  // If overview is ended because of a window getting snapped, suppress the
  // overview exiting animation.
  if (state_ == State::kBothSnapped)
    overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);

  // If clamshell split view mode is active, end it and bail out.
  if (split_view_type_ == SplitViewType::kClamshellType) {
    EndSplitView();
    return;
  }

  // Tablet split view mode is active. If it still only has one snapped window,
  // snap the first snappable window in the overview grid on the other side.
  if (state_ == State::kBothSnapped)
    return;
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(root_window_);
  if (!current_grid || current_grid->empty())
    return;
  for (const auto& overview_item : current_grid->window_list()) {
    aura::Window* window = overview_item->GetWindow();
    if (CanSnapInSplitview(window) && window != GetDefaultSnappedWindow()) {
      // Remove the overview item before snapping because the overview session
      // is unavailable to retrieve outside this function after OnOverviewEnding
      // is notified.
      overview_item->RestoreWindow(/*reset_transform=*/false);
      overview_session->RemoveItem(overview_item.get());
      SnapWindow(window, (default_snap_position_ == LEFT) ? RIGHT : LEFT);
      // If ending overview causes a window to snap, also do not do exiting
      // overview animation.
      overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);
      return;
    }
  }

  // The overview grid has at least one window, but has none that can be snapped
  // in split view. If overview is ending because of switching between virtual
  // desks, then there is no need to do anything here. Otherwise, end split view
  // and show the cannot snap toast.
  if (DesksController::Get()->AreDesksBeingModified())
    return;
  EndSplitView();
  ShowAppCannotSnapToast();
}

void SplitViewController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // Avoid |ScreenAsh::GetDisplayNearestWindow|, which has a |DCHECK| that fails
  // if the display is being deleted. Use |GetRootWindowSettings| directly, and
  // if the display is being deleted, we will get |display::kInvalidDisplayId|.
  if (GetRootWindowSettings(root_window_)->display_id != display.id())
    return;

  // We need to update |is_previous_screen_orientation_primary_| even if split
  // view mode is not active.
  const bool is_previous_screen_orientation_primary =
      is_previous_screen_orientation_primary_;
  is_previous_screen_orientation_primary_ = IsCurrentScreenOrientationPrimary();

  if (!InSplitViewMode())
    return;

  // If one of the snapped windows becomes unsnappable, end the split view mode
  // directly.
  if ((left_window_ && !CanSnapInSplitview(left_window_)) ||
      (right_window_ && !CanSnapInSplitview(right_window_))) {
    if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
      EndSplitView();
    return;
  }

  // Before adjusting the divider position for the new display metrics, if the
  // divider is animating to a snap position, then stop it and shove it there.
  // Postpone EndSplitViewAfterResizingIfAppropriate() until after the
  // adjustment, because the new display metrics will be used to compare the
  // divider position against the edges of the screen.
  if (IsDividerAnimating()) {
    StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }

  if ((metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) ||
      (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    // Set default |divider_closest_ratio_| to kFixedPositionRatios[1].
    if (std::isnan(divider_closest_ratio_))
      divider_closest_ratio_ = kFixedPositionRatios[1];

    // Reverse the position ratio if top/left window changes.
    if (is_previous_screen_orientation_primary !=
        IsCurrentScreenOrientationPrimary()) {
      divider_closest_ratio_ = 1.f - divider_closest_ratio_;
    }
    divider_position_ =
        static_cast<int>(divider_closest_ratio_ * GetDividerEndPosition()) -
        kSplitviewDividerShortSideLength / 2;
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!is_resizing_)
    divider_position_ = GetClosestFixedDividerPosition();

  EndSplitViewAfterResizingIfAppropriate();
  if (!InSplitViewMode())
    return;

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnTabletModeStarting() {
  split_view_type_ = SplitViewType::kTabletType;
}

void SplitViewController::OnTabletModeStarted() {
  // If splitview is active when tablet mode is starting, do the clamshell mode
  // splitview to tablet mode splitview transition by adding the split view
  // divider bar and also adjust the |divider_position_| so that it's on one of
  // the three fixed positions.
  if (InSplitViewMode()) {
    divider_position_ = GetClosestFixedDividerPosition();
    split_view_divider_ = std::make_unique<SplitViewDivider>(this);
    UpdateSnappedWindowsAndDividerBounds();
    NotifyDividerPositionChanged();
  }
}

void SplitViewController::OnTabletModeEnding() {
  if (IsClamshellSplitViewModeEnabled()) {
    split_view_type_ = SplitViewType::kClamshellType;

    // If splitview is active when tablet mode is ending, simply destroy the
    // split view divider bar as we don't have the bar in clamshell split view
    // mode.
    if (InSplitViewMode())
      split_view_divider_.reset();
  } else if (InSplitViewMode()) {
    // If clamshell splitview mode is not enabled, fall back to the old
    // behavior: end splitview and overivew and all windows will return to its
    // old window state before entering tablet mode.
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview();
  }
}

void SplitViewController::OnTabletControllerDestroyed() {
  tablet_mode_observer_.RemoveAll();
}

void SplitViewController::OnAccessibilityStatusChanged() {
  // TODO(crubg.com/853588): Exit split screen if ChromeVox is turned on until
  // they are compatible.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    EndSplitView();
}

void SplitViewController::OnAccessibilityControllerShutdown() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

SplitViewController::SnapPosition
SplitViewController::GetPositionOfSnappedWindow(
    const aura::Window* window) const {
  DCHECK(IsWindowInSplitView(window));
  return window == left_window_ ? LEFT : RIGHT;
}

aura::Window* SplitViewController::GetSnappedWindow(SnapPosition position) {
  DCHECK_NE(NONE, position);
  return position == LEFT ? left_window_ : right_window_;
}

aura::Window* SplitViewController::GetPhysicalLeftOrTopWindow() {
  return IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
}

aura::Window* SplitViewController::GetPhysicalRightOrBottomWindow() {
  return IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
    if (split_view_divider_)
      split_view_divider_->AddObservedWindow(window);
  }
}

void SplitViewController::StopObserving(SnapPosition snap_position) {
  aura::Window* window = GetSnappedWindow(snap_position);
  if (window == left_window_)
    left_window_ = nullptr;
  else
    right_window_ = nullptr;

  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    WindowState::Get(window)->RemoveObserver(this);
    if (split_view_divider_)
      split_view_divider_->RemoveObservedWindow(window);
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);

    // It's possible that when we try to snap an ARC app window, while we are
    // waiting for its state/bounds to the expected state/bounds, another window
    // snap request comes in and causing the previous to-be-snapped window to
    // be un-observed, in this case we should restore the previous to-be-snapped
    // window's transform if it's unidentity.
    RestoreTransformIfApplicable(window);
  }
}

void SplitViewController::UpdateStateAndNotifyObservers() {
  State previous_state = state_;
  if (IsSnapped(left_window_) && IsSnapped(right_window_))
    state_ = State::kBothSnapped;
  else if (IsSnapped(left_window_))
    state_ = State::kLeftSnapped;
  else if (IsSnapped(right_window_))
    state_ = State::kRightSnapped;
  else
    state_ = State::kNoSnap;

  // We still notify observers even if |state_| doesn't change as it's possible
  // to snap a window to a position that already has a snapped window. However,
  // |previous_state| and |state_| cannot both be |State::kNoSnap|.
  // When |previous_state| is |State::kNoSnap|, it indicates to
  // observers that split view mode started. Likewise, when |state_| is
  // |State::kNoSnap|, it indicates to observers that split view mode
  // ended.
  DCHECK(previous_state != State::kNoSnap || state_ != State::kNoSnap);
  for (auto& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state_);
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (auto& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

void SplitViewController::UpdateBlackScrim(
    const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  if (!black_scrim_layer_) {
    // Create an invisible black scrim layer.
    black_scrim_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    black_scrim_layer_->SetColor(SK_ColorBLACK);
    root_window_->layer()->Add(black_scrim_layer_.get());
    root_window_->layer()->StackAtTop(black_scrim_layer_.get());
  }

  // Decide where the black scrim should show and update its bounds.
  SnapPosition position = GetBlackScrimPosition(location_in_screen);
  if (position == NONE) {
    black_scrim_layer_.reset();
    return;
  }
  black_scrim_layer_->SetBounds(
      GetSnappedWindowBoundsInScreen(position, GetSnappedWindow(position)));

  // Update its opacity. The opacity increases as it gets closer to the edge of
  // the screen.
  const int location = IsCurrentScreenOrientationLandscape()
                           ? location_in_screen.x()
                           : location_in_screen.y();
  gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (!IsCurrentScreenOrientationLandscape())
    work_area_bounds.Transpose();
  float opacity = kBlackScrimOpacity;
  const float ratio = kOneThirdPositionRatio - kBlackScrimFadeInRatio;
  const int distance = std::min(std::abs(location - work_area_bounds.x()),
                                std::abs(work_area_bounds.right() - location));
  if (distance > work_area_bounds.width() * ratio) {
    opacity -= kBlackScrimOpacity *
               (distance - work_area_bounds.width() * ratio) /
               (work_area_bounds.width() * kBlackScrimFadeInRatio);
    opacity = std::max(opacity, 0.f);
  }
  black_scrim_layer_->SetOpacity(opacity);
}

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  DCHECK(InSplitViewMode());

  // Update the snapped windows' bounds.
  if (IsSnapped(left_window_)) {
    const WMEvent left_window_event(WM_EVENT_SNAP_LEFT);
    WindowState::Get(left_window_)->OnWMEvent(&left_window_event);
  }
  if (IsSnapped(right_window_)) {
    const WMEvent right_window_event(WM_EVENT_SNAP_RIGHT);
    WindowState::Get(right_window_)->OnWMEvent(&right_window_event);
  }

  // Update divider's bounds.
  if (split_view_divider_)
    split_view_divider_->UpdateDividerBounds();
}

SplitViewController::SnapPosition SplitViewController::GetBlackScrimPosition(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (!work_area_bounds.Contains(location_in_screen))
    return NONE;

  gfx::Size left_window_min_size, right_window_min_size;
  if (left_window_ && left_window_->delegate())
    left_window_min_size = left_window_->delegate()->GetMinimumSize();
  if (right_window_ && right_window_->delegate())
    right_window_min_size = right_window_->delegate()->GetMinimumSize();

  bool is_primary = IsCurrentScreenOrientationPrimary();
  int divider_end_position = GetDividerEndPosition();
  // The distance from the current resizing position to the left or right side
  // of the screen. Note: left or right side here means the side of the
  // |left_window_| or |right_window_|.
  int left_window_distance = 0, right_window_distance = 0;
  int min_left_length = 0, min_right_length = 0;

  if (IsCurrentScreenOrientationLandscape()) {
    int left_distance = location_in_screen.x() - work_area_bounds.x();
    int right_distance = work_area_bounds.right() - location_in_screen.x();
    left_window_distance = is_primary ? left_distance : right_distance;
    right_window_distance = is_primary ? right_distance : left_distance;

    min_left_length = left_window_min_size.width();
    min_right_length = right_window_min_size.width();
  } else {
    int top_distance = location_in_screen.y() - work_area_bounds.y();
    int bottom_distance = work_area_bounds.bottom() - location_in_screen.y();
    left_window_distance = is_primary ? top_distance : bottom_distance;
    right_window_distance = is_primary ? bottom_distance : top_distance;

    min_left_length = left_window_min_size.height();
    min_right_length = right_window_min_size.height();
  }

  if (left_window_distance < divider_end_position * kOneThirdPositionRatio ||
      left_window_distance < min_left_length) {
    return LEFT;
  }
  if (right_window_distance < divider_end_position * kOneThirdPositionRatio ||
      right_window_distance < min_right_length) {
    return RIGHT;
  }

  return NONE;
}

void SplitViewController::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  if (IsCurrentScreenOrientationLandscape()) {
    divider_position_ += location_in_screen.x() - previous_event_location_.x();
  } else {
    divider_position_ += location_in_screen.y() - previous_event_location_.y();
  }
  divider_position_ = std::max(0, divider_position_);
}

int SplitViewController::GetClosestFixedDividerPosition() {
  DCHECK(InSplitViewMode());

  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |divider_position_| represent the origin of the
  // divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |divider_position_|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int divider_end_position = GetDividerEndPosition();
  divider_closest_ratio_ = FindClosestPositionRatio(
      divider_position_ + kSplitviewDividerShortSideLength / 2,
      divider_end_position);
  int fix_position = divider_end_position * divider_closest_ratio_;
  if (divider_closest_ratio_ > 0.f && divider_closest_ratio_ < 1.f)
    fix_position -= kSplitviewDividerShortSideLength / 2;
  return fix_position;
}

void SplitViewController::StopAndShoveAnimatedDivider() {
  DCHECK(IsDividerAnimating());

  divider_snap_animation_->Stop();
  divider_position_ = divider_snap_animation_->ending_position();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

bool SplitViewController::ShouldEndSplitViewAfterResizing() {
  DCHECK(InSplitViewMode());

  return divider_position_ == 0 || divider_position_ == GetDividerEndPosition();
}

void SplitViewController::EndSplitViewAfterResizingIfAppropriate() {
  if (!ShouldEndSplitViewAfterResizing())
    return;

  aura::Window* active_window = GetActiveWindowAfterResizingUponExit();

  // Track the window that needs to be put back into the overview list if we
  // remain in overview mode.
  aura::Window* insert_overview_window = nullptr;
  if (Shell::Get()->overview_controller()->InOverviewSession())
    insert_overview_window = GetDefaultSnappedWindow();
  EndSplitView();
  if (active_window) {
    Shell::Get()->overview_controller()->EndOverview();
    wm::ActivateWindow(active_window);
  } else if (insert_overview_window) {
    // The dimensions of |window| will be very slim because of dragging the
    // divider to the edge. Change the window dimensions to its tablet mode
    // dimensions. Note: if split view is no longer constrained to tablet mode
    // this will be need to updated.
    TabletModeWindowState::UpdateWindowPosition(
        WindowState::Get(insert_overview_window), /*animate=*/false);
    InsertWindowToOverview(insert_overview_window, /*animate=*/false);
  }
}

aura::Window* SplitViewController::GetActiveWindowAfterResizingUponExit() {
  DCHECK(InSplitViewMode());

  if (!ShouldEndSplitViewAfterResizing())
    return nullptr;

  return divider_position_ == 0 ? GetPhysicalRightOrBottomWindow()
                                : GetPhysicalLeftOrTopWindow();
}

int SplitViewController::GetDividerEndPosition() {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  return IsCurrentScreenOrientationLandscape() ? work_area_bounds.width()
                                               : work_area_bounds.height();
}

void SplitViewController::OnWindowSnapped(aura::Window* window) {
  RestoreTransformIfApplicable(window);
  UpdateStateAndNotifyObservers();
  UpdateWindowStackingAfterSnap(window);
}

void SplitViewController::OnSnappedWindowDetached(aura::Window* window,
                                                  bool window_drag) {
  DCHECK(IsWindowInSplitView(window));
  if (left_window_ == window) {
    StopObserving(LEFT);
  } else {
    StopObserving(RIGHT);
  }

  // Resizing (or the divider snap animation) may continue, but |window| will no
  // longer have anything to do with it.
  if (is_resizing_ || IsDividerAnimating())
    FinishWindowResizing(window);

  if (!left_window_ && !right_window_) {
    // If there is no snapped window at this moment, ends split view mode. Note
    // this will update overview window grid bounds if the overview mode is
    // active at the moment.
    EndSplitView(window_drag ? EndReason::kWindowDragStarted
                             : EndReason::kNormal);
  } else {
    DCHECK_EQ(split_view_type_, SplitViewType::kTabletType);
    // If there is still one snapped window after minimizing/closing one snapped
    // window, update its snap state and open overview window grid.
    default_snap_position_ = left_window_ ? LEFT : RIGHT;
    UpdateStateAndNotifyObservers();
    Shell::Get()->overview_controller()->StartOverview(
        window_drag ? OverviewSession::EnterExitOverviewType::kImmediateEnter
                    : OverviewSession::EnterExitOverviewType::kNormal);
  }
}

float SplitViewController::FindClosestPositionRatio(float distance,
                                                    float length) {
  float current_ratio = distance / length;
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + sizeof(kFixedPositionRatios) / sizeof(float));
  GetDividerOptionalPositionRatios(&position_ratios);
  for (float ratio : position_ratios) {
    if (std::abs(current_ratio - ratio) <
        std::abs(current_ratio - closest_ratio)) {
      closest_ratio = ratio;
    }
  }
  return closest_ratio;
}

void SplitViewController::GetDividerOptionalPositionRatios(
    std::vector<float>* out_position_ratios) {
  aura::Window* left_or_top_window = GetPhysicalLeftOrTopWindow();
  aura::Window* right_or_bottom_window = GetPhysicalRightOrBottomWindow();
  bool is_landscape = IsCurrentScreenOrientationLandscape();

  float min_size_left_ratio = 0.f, min_size_right_ratio = 0.f;
  int min_left_size = 0, min_right_size = 0;
  if (left_or_top_window && left_or_top_window->delegate()) {
    gfx::Size min_size = left_or_top_window->delegate()->GetMinimumSize();
    min_left_size = is_landscape ? min_size.width() : min_size.height();
  }
  if (right_or_bottom_window && right_or_bottom_window->delegate()) {
    gfx::Size min_size = right_or_bottom_window->delegate()->GetMinimumSize();
    min_right_size = is_landscape ? min_size.width() : min_size.height();
  }

  int divider_end_position = GetDividerEndPosition();
  min_size_left_ratio =
      static_cast<float>(min_left_size) / divider_end_position;
  min_size_right_ratio =
      static_cast<float>(min_right_size) / divider_end_position;
  if (min_size_left_ratio <= kOneThirdPositionRatio)
    out_position_ratios->push_back(kOneThirdPositionRatio);

  if (min_size_right_ratio <= kOneThirdPositionRatio)
    out_position_ratios->push_back(kTwoThirdPositionRatio);
}

int SplitViewController::GetWindowComponentForResize(aura::Window* window) {
  DCHECK(IsWindowInSplitView(window));
  return window == GetPhysicalLeftOrTopWindow()
             ? (IsCurrentScreenOrientationLandscape() ? HTRIGHT : HTBOTTOM)
             : (IsCurrentScreenOrientationLandscape() ? HTLEFT : HTTOP);
}

gfx::Point SplitViewController::GetEndDragLocationInScreen(
    aura::Window* window,
    const gfx::Point& location_in_screen) {
  gfx::Point end_location(location_in_screen);
  if (!IsWindowInSplitView(window))
    return end_location;

  const gfx::Rect bounds = GetSnappedWindowBoundsInScreen(
      GetPositionOfSnappedWindow(window), window);
  if (IsCurrentScreenOrientationLandscape()) {
    end_location.set_x(window == GetPhysicalLeftOrTopWindow() ? bounds.right()
                                                              : bounds.x());
  } else {
    end_location.set_y(window == GetPhysicalLeftOrTopWindow() ? bounds.bottom()
                                                              : bounds.y());
  }
  return end_location;
}

void SplitViewController::RestoreTransformIfApplicable(aura::Window* window) {
  // If the transform of the window has been changed, calculate a good starting
  // transform based on its transformed bounds before to be snapped.
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter == snapping_window_transformed_bounds_map_.end())
    return;

  const gfx::Rect item_bounds = iter->second;
  snapping_window_transformed_bounds_map_.erase(iter);

  // Restore the window's transform first if it's not identity.
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    // Calculate the starting transform based on the window's expected snapped
    // bounds and its transformed bounds before to be snapped.
    const gfx::Rect snapped_bounds = GetSnappedWindowBoundsInScreen(
        GetPositionOfSnappedWindow(window), window);
    const gfx::Transform starting_transform = gfx::TransformBetweenRects(
        gfx::RectF(snapped_bounds), gfx::RectF(item_bounds));
    SetTransformWithAnimation(window, starting_transform, gfx::Transform());
  }
}

void SplitViewController::UpdateWindowStackingAfterSnap(
    aura::Window* newly_snapped) {
  if (split_view_divider_)
    split_view_divider_->SetAlwaysOnTop(true);

  aura::Window* other_snapped =
      newly_snapped == left_window_ ? right_window_ : left_window_;
  if (other_snapped) {
    DCHECK(newly_snapped == left_window_ || newly_snapped == right_window_);
    other_snapped->parent()->StackChildAtTop(other_snapped);
  }

  newly_snapped->parent()->StackChildAtTop(newly_snapped);
}

void SplitViewController::SetWindowsTransformDuringResizing() {
  DCHECK(InTabletSplitViewMode());
  DCHECK_GE(divider_position_, 0);
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  aura::Window* left_or_top_window = GetPhysicalLeftOrTopWindow();
  aura::Window* right_or_bottom_window = GetPhysicalRightOrBottomWindow();

  gfx::Transform left_or_top_transform;
  if (left_or_top_window) {
    const int left_size = divider_position_;
    const int left_minimum_size =
        GetMinimumWindowSize(left_or_top_window, is_landscape);
    const int distance = left_size - left_minimum_size;
    if (distance < 0) {
      left_or_top_transform.Translate(is_landscape ? distance : 0,
                                      is_landscape ? 0 : distance);
    }
    SetTransform(left_or_top_window, left_or_top_transform);
  }

  gfx::Transform right_or_bottom_transform;
  if (right_or_bottom_window) {
    const int right_size = GetDividerEndPosition() - divider_position_ -
                           kSplitviewDividerShortSideLength;
    const int right_minimum_size =
        GetMinimumWindowSize(right_or_bottom_window, is_landscape);
    const int distance = right_size - right_minimum_size;
    if (distance < 0) {
      right_or_bottom_transform.Translate(is_landscape ? -distance : 0,
                                          is_landscape ? 0 : -distance);
    }
    SetTransform(right_or_bottom_window, right_or_bottom_transform);
  }

  if (black_scrim_layer_.get()) {
    black_scrim_layer_->SetTransform(left_or_top_transform.IsIdentity()
                                         ? right_or_bottom_transform
                                         : left_or_top_transform);
  }
}

void SplitViewController::RestoreWindowsTransformAfterResizing() {
  DCHECK(InSplitViewMode());
  if (left_window_)
    SetTransform(left_window_, gfx::Transform());
  if (right_window_)
    SetTransform(right_window_, gfx::Transform());
  if (black_scrim_layer_.get())
    black_scrim_layer_->SetTransform(gfx::Transform());
}

void SplitViewController::SetTransformWithAnimation(
    aura::Window* window,
    const gfx::Transform& start_transform,
    const gfx::Transform& target_transform) {
  gfx::Point target_origin =
      gfx::ToRoundedPoint(GetTargetBoundsInScreen(window).origin());
  for (auto* window_iter : GetTransientTreeIterator(window)) {
    // Adjust |start_transform| and |target_transform| for the transient child.
    aura::Window* parent_window = window_iter->parent();
    gfx::Rect original_bounds(window_iter->GetTargetBounds());
    ::wm::ConvertRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_start_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            start_transform);
    gfx::Transform new_target_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            target_transform);
    if (new_start_transform != window_iter->layer()->GetTargetTransform())
      window_iter->SetTransform(new_start_transform);

    DoSplitviewTransformAnimation(window_iter->layer(),
                                  SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM,
                                  new_target_transform);
  }
}

void SplitViewController::RemoveWindowFromOverviewIfApplicable(
    aura::Window* window) {
  DCHECK_EQ(root_window_, window->GetRootWindow());

  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  OverviewSession* overview_session = GetOverviewSession();
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(root_window_);
  if (!current_grid)
    return;

  OverviewItem* item = current_grid->GetOverviewItemContaining(window);
  if (!item)
    return;

  // Remove it from the grid. The transform will be reset later after the
  // window is snapped. Note the remaining windows in overview don't need to be
  // repositioned in this case as they have been positioned to the right place
  // during dragging.
  item->RestoreWindow(/*reset_transform=*/false);
  overview_session->RemoveItem(item);
}

void SplitViewController::UpdateSnappingWindowTransformedBounds(
    aura::Window* window) {
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    snapping_window_transformed_bounds_map_[window] =
        gfx::ToEnclosedRect(GetTransformedBounds(window, /*top_inset=*/0));
  }
}

void SplitViewController::InsertWindowToOverview(aura::Window* window,
                                                 bool animate) {
  if (!window || !GetOverviewSession())
    return;
  GetOverviewSession()->AddItem(window, /*reposition=*/true, animate);
}

void SplitViewController::FinishWindowResizing(aura::Window* window) {
  if (window != nullptr) {
    WindowState* window_state = WindowState::Get(window);
    window_state->OnCompleteDrag(
        GetEndDragLocationInScreen(window, previous_event_location_));
    window_state->DeleteDragDetails();
  }
}

void SplitViewController::EndResizeImpl() {
  DCHECK(InSplitViewMode());
  DCHECK(!is_resizing_);
  // Resize may not end with |EndResize()|, so make sure to clear here too.
  presentation_time_recorder_.reset();
  RestoreWindowsTransformAfterResizing();
  FinishWindowResizing(left_window_);
  FinishWindowResizing(right_window_);
}

void SplitViewController::EndWindowDragImpl(
    aura::Window* window,
    bool is_being_destroyed,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (split_view_divider_)
    split_view_divider_->OnWindowDragEnded();

  // If the dragged window is to be destroyed, do not try to snap it.
  if (is_being_destroyed)
    return;

  // If dragged window was in overview before or it has been added to overview
  // window by dropping on the new selector item, do nothing.
  if (GetOverviewSession() && GetOverviewSession()->IsWindowInOverview(window))
    return;

  DCHECK_EQ(root_window_, window->GetRootWindow());

  const bool was_splitview_active = InSplitViewMode();
  if (desired_snap_position == SplitViewController::NONE) {
    if (was_splitview_active) {
      // Even though |snap_position| equals |NONE|, the dragged window still
      // needs to be snapped if splitview mode is active at the momemnt.
      // Calculate the expected snap position based on the last event
      // location. Note if there is already a window at |desired_snap_postion|,
      // SnapWindow() will put the previous snapped window in overview.
      SnapWindow(window, ComputeSnapPosition(last_location_in_screen));
      wm::ActivateWindow(window);
    } else {
      // Restore the dragged window's transform first if it's not identity. It
      // needs to be called before the transformed window's bounds change so
      // that its transient children are layout'ed properly (the layout happens
      // when window's bounds change).
      SetTransformWithAnimation(window, window->layer()->GetTargetTransform(),
                                gfx::Transform());

      OverviewSession* overview_session = GetOverviewSession();
      if (overview_session) {
        overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);
        // Set the overview exit type to kImmediateExit to avoid update bounds
        // animation of the windows in overview grid.
        overview_session->set_enter_exit_overview_type(
            OverviewSession::EnterExitOverviewType::kImmediateExit);
      }
      // Activate the dragged window and end the overview. The dragged window
      // will be restored back to its previous state before dragging.
      wm::ActivateWindow(window);
      Shell::Get()->overview_controller()->EndOverview();

      // Update the dragged window's bounds. It's possible that the dragged
      // window's bounds was changed during dragging. Update its bounds after
      // the drag ends to ensure it has the right bounds.
      TabletModeWindowState::UpdateWindowPosition(WindowState::Get(window),
                                                  /*animate=*/true);
    }
  } else {
    aura::Window* initiator_window =
        window->GetProperty(ash::kTabDraggingSourceWindowKey);
    // Note SnapWindow() might put the previous window that was snapped at the
    // |desired_snap_position| in overview.
    SnapWindow(window, desired_snap_position,
               /*use_divider_spawn_animation=*/!initiator_window);
    wm::ActivateWindow(window);

    if (!was_splitview_active) {
      // If splitview mode was not active before snapping the dragged
      // window, snap the initiator window to the other side of the screen
      // if it's not the same window as the dragged window.
      if (initiator_window && initiator_window != window) {
        SnapWindow(initiator_window,
                   (desired_snap_position == SplitViewController::LEFT)
                       ? SplitViewController::RIGHT
                       : SplitViewController::LEFT);
      } else {
        // If overview is not active, open overview.
        Shell::Get()->overview_controller()->StartOverview();
      }
    }
  }
}

SplitViewController::SnapPosition SplitViewController::ComputeSnapPosition(
    const gfx::Point& last_location_in_screen) {
  const int divider_position = InSplitViewMode() ? this->divider_position()
                                                 : GetDefaultDividerPosition();
  const int position = IsCurrentScreenOrientationLandscape()
                           ? last_location_in_screen.x()
                           : last_location_in_screen.y();
  return (position <= divider_position) == IsCurrentScreenOrientationPrimary()
             ? SplitViewController::LEFT
             : SplitViewController::RIGHT;
}

}  // namespace ash
