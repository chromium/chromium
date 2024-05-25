// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_splitter.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>

#include "ash/public/cpp/window_finder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"
#include "ui/events/velocity_tracker/velocity_tracker.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {

namespace {

using DragType = WindowSplitter::DragType;
using SplitRegion = WindowSplitter::SplitRegion;
using SplitWindowInfo = WindowSplitter::SplitWindowInfo;

// Squared velocity value of the dwell max velocity, for calculations.
constexpr double kDwellMaxVelocitySquaredPixelsPerSec =
    WindowSplitter::kDwellMaxVelocityPixelsPerSec *
    WindowSplitter::kDwellMaxVelocityPixelsPerSec;

// Returns true if `split_region` is a splittable region.
bool IsRegionSplittable(SplitRegion split_region) {
  return split_region != SplitRegion::kNone;
}

// Returns true if the `window`'s state type allows the window to be split.
bool IsWindowStateTypeSplittable(aura::Window* window) {
  switch (WindowState::Get(window)->GetStateType()) {
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kInactive:
    case chromeos::WindowStateType::kMaximized:
    case chromeos::WindowStateType::kPrimarySnapped:
    case chromeos::WindowStateType::kSecondarySnapped:
      return true;
    default:
      return false;
  }
}

aura::Window* GetTopmostWindow(aura::Window* dragged_window,
                               const gfx::PointF& screen_location) {
  const gfx::Point screen_point = gfx::ToFlooredPoint(screen_location);
  std::set<aura::Window*> ignore({dragged_window});
  while (true) {
    if (auto* topmost_window = GetTopmostWindowAtPoint(screen_point, ignore)) {
      // Some targeters slightly extend hit region outside window bounds, e.g.
      // `chromeos::kResizeOutsideBoundsSize`, so ignore those hits.
      if (!topmost_window->GetBoundsInScreen().Contains(screen_point)) {
        ignore.insert(topmost_window);
        continue;
      }
      if (CanIncludeWindowInMruList(topmost_window) &&
          IsWindowStateTypeSplittable(topmost_window)) {
        return topmost_window;
      }
    }
    return nullptr;
  }
}

gfx::Insets GetTriggerMargins(const gfx::Rect& bounds) {
  // TODO(b/293614784): Tune margin calculation.
  return gfx::Insets::VH(
      std::min(bounds.height() / 5, WindowSplitter::kBaseTriggerMargins.top()),
      std::min(bounds.width() / 5, WindowSplitter::kBaseTriggerMargins.left()));
}

// `screen_location` must be within `window`'s bounds.
SplitRegion GetSplitRegion(aura::Window* window,
                           const gfx::PointF& screen_location) {
  const gfx::Rect screen_bounds = window->GetBoundsInScreen();
  const gfx::Insets margins = GetTriggerMargins(screen_bounds);
  if (screen_location.x() < screen_bounds.x() + margins.left()) {
    return SplitRegion::kLeft;
  }
  if (screen_location.x() > screen_bounds.right() - margins.right()) {
    return SplitRegion::kRight;
  }
  if (screen_location.y() < screen_bounds.y() + margins.top()) {
    return SplitRegion::kTop;
  }
  if (screen_location.y() > screen_bounds.bottom() - margins.bottom()) {
    return SplitRegion::kBottom;
  }
  return SplitRegion::kNone;
}

// Gets the bounds after splitting `from_bounds` into the given region.
gfx::Rect GetBoundsForSplitRegion(const gfx::Rect& from_bounds,
                                  SplitRegion split_region) {
  gfx::Rect top_or_left = from_bounds;
  // Adjust size.
  switch (split_region) {
    case SplitRegion::kLeft:
    case SplitRegion::kRight:
      top_or_left.set_width(top_or_left.width() / 2);
      break;
    case SplitRegion::kTop:
    case SplitRegion::kBottom:
      top_or_left.set_height(top_or_left.height() / 2);
      break;
    default:
      break;
  }
  // Adjust position.
  switch (split_region) {
    case SplitRegion::kLeft:
    case SplitRegion::kTop:
      return top_or_left;
    case SplitRegion::kRight:
    case SplitRegion::kBottom: {
      gfx::Rect bottom_or_right = from_bounds;
      bottom_or_right.Subtract(top_or_left);
      return bottom_or_right;
    }
    default:
      break;
  }
  return from_bounds;
}

bool FitsMinimumSize(aura::Window* window, const gfx::Rect& new_size) {
  gfx::Size min_size;
  if (window->delegate()) {
    min_size = window->delegate()->GetMinimumSize();
  }
  if (!min_size.IsEmpty()) {
    return new_size.width() >= min_size.width() &&
           new_size.height() >= min_size.height();
  }
  return true;
}

bool ContainedInWorkArea(aura::Window* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window)
      .work_area()
      .Contains(window->GetBoundsInScreen());
}

void ResizeAndActivateWindow(aura::Window* window,
                             const gfx::Rect& screen_bounds) {
  auto* window_state = WindowState::Get(window);
  if (!chromeos::IsNormalWindowStateType(window_state->GetStateType())) {
    // TODO(b/308194482): Disable animation, e.g. if this would unmaximize.
    // But having animation may be ok, so need UX input.
    const WMEvent event(WM_EVENT_NORMAL);
    window_state->OnWMEvent(&event);
  }
  window->SetBoundsInScreen(
      screen_bounds,
      display::Screen::GetScreen()->GetDisplayMatching(screen_bounds));
  window_state->Activate();
}

}  // namespace

bool SplitWindowInfo::operator==(const SplitWindowInfo&) const = default;

std::optional<SplitWindowInfo> WindowSplitter::MaybeSplitWindow(
    aura::Window* topmost_window,
    aura::Window* dragged_window,
    const gfx::PointF& screen_location) {
  // Don't split if `topmost_window` is not fully inside a display's work area.
  // This gets around some corner cases, where the split window may end up
  // entirely off screen.
  if (!ContainedInWorkArea(topmost_window)) {
    return std::nullopt;
  }

  // TODO(b/342672204): Consider filtering out windows that are too occluded.

  const auto split_region = GetSplitRegion(topmost_window, screen_location);
  if (!IsRegionSplittable(split_region)) {
    return std::nullopt;
  }

  SplitWindowInfo split_info{
      .split_region = split_region,
  };
  split_info.topmost_window_bounds = topmost_window->GetBoundsInScreen();
  split_info.dragged_window_bounds =
      GetBoundsForSplitRegion(split_info.topmost_window_bounds, split_region);

  if (!FitsMinimumSize(dragged_window, split_info.dragged_window_bounds)) {
    return std::nullopt;
  }

  split_info.topmost_window_bounds.Subtract(split_info.dragged_window_bounds);

  if (!FitsMinimumSize(topmost_window, split_info.topmost_window_bounds)) {
    return std::nullopt;
  }

  return split_info;
}

WindowSplitter::WindowSplitter(aura::Window* dragged_window)
    : drag_start_time_(base::TimeTicks::Now()),
      velocity_tracker_(ui::VelocityTracker::Strategy::STRATEGY_DEFAULT) {
  dragged_window_observation_.Observe(dragged_window);
}

WindowSplitter::~WindowSplitter() {
  RecordMetricsOnEndDrag();
}

void WindowSplitter::UpdateDrag(const gfx::PointF& location_in_screen,
                                bool can_split) {
  is_drag_updated_ = true;
  last_location_in_screen_ = location_in_screen;

  // Must update cursor location every time, so the velocity is more accurate.
  UpdateCursorLocation(location_in_screen);

  if (!can_split || !dragged_window()) {
    Disengage();
    return;
  }

  const auto* last_topmost_window = topmost_window();
  UpdateTopMostWindow(GetTopmostWindow(dragged_window(), location_in_screen));
  if (!topmost_window() || topmost_window() != last_topmost_window) {
    RestartDwellTimer();
    return;
  }

  auto last_split_window_info = last_split_window_info_;
  const std::optional<SplitWindowInfo> split_bounds =
      MaybeSplitWindow(topmost_window(), dragged_window(), location_in_screen);
  last_split_window_info_ = split_bounds;
  if (!split_bounds || split_bounds != last_split_window_info) {
    RestartDwellTimer();
    return;
  }

  if (GetCursorVelocitySquared() > kDwellMaxVelocitySquaredPixelsPerSec) {
    RestartDwellTimer();
    return;
  }

  if (!ReadyToSplit() && !dwell_activation_timer_.IsRunning()) {
    RestartDwellTimer();
  }
}

void WindowSplitter::CompleteDrag(const gfx::PointF& last_location_in_screen) {
  is_drag_completed_ = true;
  if (!ReadyToSplit() || !dragged_window()) {
    return;
  }

  if (auto* topmost_window =
          GetTopmostWindow(dragged_window(), last_location_in_screen)) {
    if (const std::optional<SplitWindowInfo> split_bounds = MaybeSplitWindow(
            topmost_window, dragged_window(), last_location_in_screen)) {
      ResizeAndActivateWindow(topmost_window,
                              split_bounds->topmost_window_bounds);
      ResizeAndActivateWindow(dragged_window(),
                              split_bounds->dragged_window_bounds);
      completed_split_region_ = split_bounds->split_region;
    }
  }
}

void WindowSplitter::Disengage() {
  RemovePhantomWindow();
  UpdateTopMostWindow(nullptr);
  last_split_window_info_ = std::nullopt;
  // Don't clear velocity_tracker_, since it needs historical cursor positions
  // to be accurate.
}

void WindowSplitter::OnWindowDestroying(aura::Window* window) {
  if (window == topmost_window()) {
    UpdateTopMostWindow(nullptr);
    return;
  }
  // Dragged window is destroying.
  Disengage();
  RecordMetricsOnEndDrag();
  dragged_window_observation_.Reset();
}

void WindowSplitter::RestartDwellTimer() {
  if (dwell_activation_timer_.IsRunning()) {
    dwell_activation_timer_.Reset();
    return;
  }
  RemovePhantomWindow();
  dwell_activation_timer_.Start(
      FROM_HERE, kDwellActivationDuration,
      base::BindOnce(&WindowSplitter::ShowPhantomWindowCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WindowSplitter::RemovePhantomWindow() {
  phantom_window_controller_.reset();
  dwell_activation_timer_.Stop();
  dwell_cancellation_timer_.Stop();
}

void WindowSplitter::ShowPhantomWindowCallback() {
  if (!dragged_window()) {
    return;
  }

  // Make sure the cursor is still over the expected topmost window, since the
  // initial topmost window may have been moved/resized, closed, or occluded.
  if (auto* current_topmost_window =
          GetTopmostWindow(dragged_window(), last_location_in_screen_)) {
    if (current_topmost_window != topmost_window()) {
      return;
    }
    // Recalculate phantom window bounds, since topmost window may have resized.
    if (const std::optional<SplitWindowInfo> split_bounds =
            MaybeSplitWindow(current_topmost_window, dragged_window(),
                             last_location_in_screen_)) {
      ShowPhantomWindow(split_bounds->dragged_window_bounds);
      dwell_cancellation_timer_.Start(
          FROM_HERE, kDwellCancellationDuration,
          base::BindOnce(&WindowSplitter::Disengage,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void WindowSplitter::ShowPhantomWindow(const gfx::Rect& bounds) {
  if (!phantom_window_controller_) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(dragged_window());
  }
  if (phantom_window_controller_->GetTargetWindowBounds() != bounds) {
    phantom_window_shown_count_++;
  }
  phantom_window_controller_->Show(bounds);
}

void WindowSplitter::RecordMetricsOnEndDrag() {
  if (!dragged_window() || !is_drag_updated_) {
    return;
  }

  const DragType drag_type = GetDragType();
  base::UmaHistogramEnumeration(kWindowSplittingDragTypeHistogramName,
                                drag_type);

  if (drag_type == DragType::kIncomplete) {
    return;
  }

  base::UmaHistogramMediumTimes(
      drag_type == DragType::kNoSplit
          ? kWindowSplittingDragDurationPerNoSplitHistogramName
          : kWindowSplittingDragDurationPerSplitHistogramName,
      base::TimeTicks::Now() - drag_start_time_);
  base::UmaHistogramCounts100(
      drag_type == DragType::kNoSplit
          ? kWindowSplittingPreviewsShownCountPerNoSplitDragHistogramName
          : kWindowSplittingPreviewsShownCountPerSplitDragHistogramName,
      phantom_window_shown_count_);

  if (drag_type == DragType::kSplit) {
    base::UmaHistogramEnumeration(kWindowSplittingSplitRegionHistogramName,
                                  completed_split_region_);
  }
}

DragType WindowSplitter::GetDragType() const {
  if (!is_drag_completed_) {
    return DragType::kIncomplete;
  }
  return IsRegionSplittable(completed_split_region_) ? DragType::kSplit
                                                     : DragType::kNoSplit;
}

void WindowSplitter::UpdateCursorLocation(
    const gfx::PointF& location_in_screen) {
  const ui::MotionEventGeneric event(
      ui::MotionEvent::Action::MOVE, base::TimeTicks::Now(),
      ui::PointerProperties(location_in_screen.x(), location_in_screen.y(),
                            /*touch_major=*/0));
  velocity_tracker_.AddMovement(event);
}

double WindowSplitter::GetCursorVelocitySquared() const {
  gfx::Vector2dF velocity_vector;
  float dx, dy;
  if (velocity_tracker_.GetVelocity(/*id=*/0, &dx, &dy)) {
    velocity_vector.set_x(dx);
    velocity_vector.set_y(dy);
  }
  return velocity_vector.LengthSquared();
}

void WindowSplitter::UpdateTopMostWindow(aura::Window* topmost_window) {
  if (!topmost_window) {
    topmost_window_observation_.Reset();
    return;
  }
  if (topmost_window_observation_.IsObservingSource(topmost_window)) {
    return;
  }
  topmost_window_observation_.Reset();
  topmost_window_observation_.Observe(topmost_window);
}

}  // namespace ash
