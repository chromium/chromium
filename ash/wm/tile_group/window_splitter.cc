// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_splitter.h"

#include <algorithm>
#include <memory>
#include <set>

#include "ash/public/cpp/window_finder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ash {

using DragType = WindowSplitter::DragType;
using SplitRegion = WindowSplitter::SplitRegion;
using SplitWindowInfo = WindowSplitter::SplitWindowInfo;

constexpr gfx::Insets kBaseTriggerMargins = gfx::Insets::VH(25, 45);

// Returns true if `split_region` is a splittable region.
bool IsSplittable(SplitRegion split_region) {
  return split_region != SplitRegion::kNone;
}

aura::Window* GetTopmostWindow(aura::Window* dragged_window,
                               const gfx::PointF& screen_location) {
  const gfx::Point screen_point = gfx::ToFlooredPoint(screen_location);
  std::set<aura::Window*> ignore({dragged_window});
  while (true) {
    if (auto* topmost_window = GetTopmostWindowAtPoint(screen_point, ignore)) {
      // Some targeters slightly extend hit region outside window bounds, e.g.
      // `chromeos::kResizeOutsideBoundsSize`, so ignore those hits.
      if (topmost_window->GetBoundsInScreen().Contains(screen_point) &&
          CanIncludeWindowInMruList(topmost_window)) {
        return topmost_window;
      }
      ignore.insert(topmost_window);
    } else {
      return nullptr;
    }
  }
}

gfx::Insets GetTriggerMargins(const gfx::Rect& bounds) {
  // TODO(b/293614784): Tune margin calculation.
  return gfx::Insets::VH(
      std::min(bounds.height() / 5, kBaseTriggerMargins.top()),
      std::min(bounds.width() / 5, kBaseTriggerMargins.left()));
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

absl::optional<SplitWindowInfo> WindowSplitter::MaybeSplitWindow(
    aura::Window* topmost_window,
    aura::Window* dragged_window,
    const gfx::PointF& screen_location) {
  // Don't split if `topmost_window` is not fully inside a display's work area.
  if (!ContainedInWorkArea(topmost_window)) {
    return absl::nullopt;
  }

  const auto split_region = GetSplitRegion(topmost_window, screen_location);
  if (!IsSplittable(split_region)) {
    return absl::nullopt;
  }

  SplitWindowInfo split_info{
      .split_region = split_region,
  };
  split_info.topmost_window_bounds = topmost_window->GetBoundsInScreen();
  split_info.dragged_window_bounds =
      GetBoundsForSplitRegion(split_info.topmost_window_bounds, split_region);

  if (!FitsMinimumSize(dragged_window, split_info.dragged_window_bounds)) {
    return absl::nullopt;
  }

  split_info.topmost_window_bounds.Subtract(split_info.dragged_window_bounds);

  if (!FitsMinimumSize(topmost_window, split_info.topmost_window_bounds)) {
    return absl::nullopt;
  }

  return split_info;
}

WindowSplitter::WindowSplitter(aura::Window* dragged_window)
    : dragged_window_(dragged_window),
      drag_start_time_(base::TimeTicks::Now()) {
  dragged_window_->AddObserver(this);
}

WindowSplitter::~WindowSplitter() {
  MaybeClearDraggedWindow();
}

void WindowSplitter::UpdateDrag(const gfx::PointF& location_in_screen,
                                bool can_split) {
  is_drag_updated_ = true;
  if (!can_split || !dragged_window_) {
    Disengage();
    return;
  }

  if (auto* topmost_window =
          GetTopmostWindow(dragged_window_, location_in_screen)) {
    if (auto split_bounds = MaybeSplitWindow(topmost_window, dragged_window_,
                                             location_in_screen)) {
      // TODO(b/252550043): Support dwell delay to not activate right away.
      can_split_window_ = true;
      ShowPhantomWindow(split_bounds->dragged_window_bounds);
      return;
    }
  }
  // TODO(b/252550043): Support cancellation after dwell delay.
  Disengage();
}

void WindowSplitter::CompleteDrag(const gfx::PointF& last_location_in_screen) {
  is_drag_completed_ = true;
  if (!can_split_window_ || !dragged_window_) {
    return;
  }

  if (auto* topmost_window =
          GetTopmostWindow(dragged_window_, last_location_in_screen)) {
    if (auto split_bounds = MaybeSplitWindow(topmost_window, dragged_window_,
                                             last_location_in_screen)) {
      // TODO(b/252550043): Change window states to normal beforehand.
      dragged_window_->SetBounds(split_bounds->dragged_window_bounds);
      topmost_window->SetBounds(split_bounds->topmost_window_bounds);
      completed_split_region_ = split_bounds->split_region;
    }
  }
}

void WindowSplitter::Disengage() {
  can_split_window_ = false;
  phantom_window_controller_.reset();
}

void WindowSplitter::OnWindowDestroying(aura::Window* window) {
  MaybeClearDraggedWindow();
}

void WindowSplitter::ShowPhantomWindow(const gfx::Rect& bounds) {
  if (!phantom_window_controller_) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(dragged_window_);
  }
  if (phantom_window_controller_->GetTargetWindowBounds() != bounds) {
    phantom_window_shown_count_++;
  }
  phantom_window_controller_->Show(bounds);
}

void WindowSplitter::MaybeClearDraggedWindow() {
  if (dragged_window_) {
    RecordMetricsOnEndDrag();
    dragged_window_->RemoveObserver(this);
    dragged_window_ = nullptr;
    Disengage();
  }
}

void WindowSplitter::RecordMetricsOnEndDrag() {
  if (!is_drag_updated_) {
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
  return IsSplittable(completed_split_region_) ? DragType::kSplit
                                               : DragType::kNoSplit;
}

}  // namespace ash
