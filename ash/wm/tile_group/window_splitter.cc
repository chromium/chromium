// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_splitter.h"

#include <algorithm>
#include <memory>
#include <set>

#include "ash/public/cpp/window_finder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ash {

using SplitPosition = WindowSplitter::SplitPosition;
using SplitWindowBounds = WindowSplitter::SplitWindowBounds;

constexpr gfx::Insets kBaseTriggerMargins = gfx::Insets::VH(25, 45);

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
SplitPosition GetSplitPosition(aura::Window* window,
                               const gfx::PointF& screen_location) {
  const gfx::Rect screen_bounds = window->GetBoundsInScreen();
  const gfx::Insets margins = GetTriggerMargins(screen_bounds);
  if (screen_location.x() < screen_bounds.x() + margins.left()) {
    return SplitPosition::kLeft;
  }
  if (screen_location.x() > screen_bounds.right() - margins.right()) {
    return SplitPosition::kRight;
  }
  if (screen_location.y() < screen_bounds.y() + margins.top()) {
    return SplitPosition::kTop;
  }
  if (screen_location.y() > screen_bounds.bottom() - margins.bottom()) {
    return SplitPosition::kBottom;
  }
  return SplitPosition::kNone;
}

// Gets the bounds after splitting `from_bounds` into the given position.
gfx::Rect GetBoundsForSplitPosition(const gfx::Rect& from_bounds,
                                    SplitPosition split_position) {
  gfx::Rect top_or_left = from_bounds;
  // Adjust size.
  switch (split_position) {
    case SplitPosition::kLeft:
    case SplitPosition::kRight:
      top_or_left.set_width(top_or_left.width() / 2);
      break;
    case SplitPosition::kTop:
    case SplitPosition::kBottom:
      top_or_left.set_height(top_or_left.height() / 2);
      break;
    default:
      break;
  }
  // Adjust position.
  switch (split_position) {
    case SplitPosition::kLeft:
    case SplitPosition::kTop:
      return top_or_left;
    case SplitPosition::kRight:
    case SplitPosition::kBottom: {
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

absl::optional<SplitWindowBounds> WindowSplitter::MaybeSplitWindow(
    aura::Window* topmost_window,
    aura::Window* dragged_window,
    const gfx::PointF& screen_location) {
  // Don't split if `topmost_window` is not fully inside a display's work area.
  if (!ContainedInWorkArea(topmost_window)) {
    return absl::nullopt;
  }

  const auto split_position = GetSplitPosition(topmost_window, screen_location);
  if (split_position == SplitPosition::kNone) {
    return absl::nullopt;
  }

  SplitWindowBounds split_bounds;
  split_bounds.topmost_window_bounds = topmost_window->GetBoundsInScreen();
  split_bounds.dragged_window_bounds = GetBoundsForSplitPosition(
      split_bounds.topmost_window_bounds, split_position);

  if (!FitsMinimumSize(dragged_window, split_bounds.dragged_window_bounds)) {
    return absl::nullopt;
  }

  split_bounds.topmost_window_bounds.Subtract(
      split_bounds.dragged_window_bounds);

  if (!FitsMinimumSize(topmost_window, split_bounds.topmost_window_bounds)) {
    return absl::nullopt;
  }

  return split_bounds;
}

WindowSplitter::WindowSplitter(aura::Window* dragged_window)
    : dragged_window_(dragged_window) {
  dragged_window_->AddObserver(this);
}

WindowSplitter::~WindowSplitter() {
  MaybeClearDraggedWindow();
}

void WindowSplitter::UpdateDrag(const gfx::PointF& location_in_screen,
                                bool can_split) {
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
  phantom_window_controller_->Show(bounds);
}

void WindowSplitter::MaybeClearDraggedWindow() {
  if (dragged_window_) {
    dragged_window_->RemoveObserver(this);
    dragged_window_ = nullptr;
    Disengage();
  }
}

}  // namespace ash
