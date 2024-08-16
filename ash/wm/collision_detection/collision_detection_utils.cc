// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/collision_detection/collision_detection_utils.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/wm/work_area_insets.h"
#include "ui/base/class_property.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::CollisionDetectionUtils::RelativePriority)

namespace ash {

namespace {

// A property key to store whether the a window should be ignored for window
// collision detection. For example, StatusBubble windows.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIgnoreForWindowCollisionDetection, false)

// A property key to store a window's collision detection priority, used to
// resolve collisions between multiple windows which are both using
// CollisionDetectionUtils.
DEFINE_UI_CLASS_PROPERTY_KEY(
    CollisionDetectionUtils::RelativePriority,
    kCollisionDetectionRelativePriority,
    CollisionDetectionUtils::RelativePriority::kDefault)

bool ShouldIgnoreWindowForCollision(
    const aura::Window* window,
    CollisionDetectionUtils::RelativePriority priority) {
  if (!window->IsVisible() && !window->GetTargetBounds().IsEmpty()) {
    return true;
  }
  if (window->is_destroying()) {
    return true;
  }
  if (window->GetProperty(kIgnoreForWindowCollisionDetection))
    return true;

  DCHECK_NE(CollisionDetectionUtils::RelativePriority::kDefault, priority);

  // Only ignore a window if our priority is greater than theirs. Lower priority
  // windows need to move for higher priority windows, and windows do not need
  // to move for a window of their own priority.
  return static_cast<int>(priority) >=
         static_cast<int>(
             window->GetProperty(kCollisionDetectionRelativePriority));
}

gfx::Rect ComputeCollisionRectFromBounds(const gfx::Rect& bounds,
                                         const aura::Window* parent,
                                         bool inset = true) {
  gfx::Rect collision_rect = bounds;
  ::wm::ConvertRectToScreen(parent, &collision_rect);
  if (inset) {
    collision_rect.Inset(-kCollisionWindowWorkAreaInsetsDp);
  }
  return collision_rect;
}

std::vector<gfx::Rect> CollectCollisionRects(
    const display::Display& display,
    CollisionDetectionUtils::RelativePriority priority) {
  DCHECK_NE(CollisionDetectionUtils::RelativePriority::kDefault, priority);
  std::vector<gfx::Rect> rects;
  auto* root_window = Shell::GetRootWindowForDisplayId(display.id());
  if (root_window) {
    // Check SettingsBubbleContainer windows.
    auto* settings_bubble_container =
        root_window->GetChildById(kShellWindowId_SettingBubbleContainer);
    for (aura::Window* window : settings_bubble_container->children()) {
      if (!window->IsVisible() && !window->GetTargetBounds().IsEmpty())
        continue;
      if (ShouldIgnoreWindowForCollision(window, priority))
        continue;
      // Use the target bounds in case an animation is in progress.
      rects.push_back(ComputeCollisionRectFromBounds(window->GetTargetBounds(),
                                                     window->parent()));
    }

    // Check auto-hide shelf, which isn't included normally in the work area:
    auto* shelf = Shelf::ForWindow(root_window);
    auto* shelf_window = shelf->GetWindow();
    if (shelf->IsVisible() &&
        !ShouldIgnoreWindowForCollision(shelf_window, priority)) {
      rects.push_back(ComputeCollisionRectFromBounds(
          shelf_window->GetTargetBounds(), shelf_window->parent()));
    }

    // Explicitly add popup notifications as they are not in the notification
    // tray.
    auto* shelf_container =
        root_window->GetChildById(kShellWindowId_ShelfContainer);
    for (aura::Window* window : shelf_container->children()) {
      if (window->IsVisible() && !window->GetTargetBounds().IsEmpty() &&
          window->GetName() ==
              AshMessagePopupCollection::kMessagePopupWidgetName &&
          !ShouldIgnoreWindowForCollision(window, priority)) {
        rects.push_back(ComputeCollisionRectFromBounds(
            window->GetTargetBounds(), window->parent()));
      }
    }

    // The hotseat doesn't span the whole width of the display, but to allow
    // a PIP window to be slided horizontally along the hotseat, we extend the
    // width of the hotseat to that of the display.
    auto* hotseat_widget = shelf->hotseat_widget();
    if (hotseat_widget) {
      auto* hotseat_window = hotseat_widget->GetNativeWindow();
      gfx::Rect hotseat_rect =
          shelf->IsHorizontalAlignment()
              ? gfx::Rect(root_window->bounds().x(),
                          hotseat_window->GetTargetBounds().y(),
                          root_window->bounds().width(),
                          hotseat_window->GetTargetBounds().height())
              : gfx::Rect(hotseat_window->GetTargetBounds().x(),
                          root_window->bounds().y(),
                          hotseat_window->GetTargetBounds().width(),
                          root_window->bounds().height());
      if (hotseat_widget->state() != HotseatState::kHidden &&
          hotseat_widget->state() != HotseatState::kNone &&
          !ShouldIgnoreWindowForCollision(hotseat_window, priority)) {
        rects.push_back(ComputeCollisionRectFromBounds(
            hotseat_rect, hotseat_window->parent()));
      }
    }

    // Check the Automatic Clicks windows.
    // TODO(Katie): The PIP isn't re-triggered to check the accessibility bubble
    // windows when the autoclick window moves, just when the PIP moves or
    // another system window. Need to ensure that changing the autoclick menu
    // position triggers the PIP to re-check its bounds. crbug.com/954546.
    auto* accessibility_bubble_container =
        root_window->GetChildById(kShellWindowId_AccessibilityBubbleContainer);
    for (aura::Window* window : accessibility_bubble_container->children()) {
      if (!window->IsVisible() && !window->GetTargetBounds().IsEmpty())
        continue;
      if (ShouldIgnoreWindowForCollision(window, priority))
        continue;
      // Use the target bounds in case an animation is in progress.
      if (priority ==
              CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu &&
          window->GetProperty(kCollisionDetectionRelativePriority) ==
              CollisionDetectionUtils::RelativePriority::
                  kAutomaticClicksScrollMenu) {
        // The autoclick menu widget and autoclick scroll menu widget are 0dip
        // apart because they must be drawn at 8dips apart including drop
        // shadow. This special case means we should not add an inset if we
        // are calculating collision rects for the autoclick menu.
        rects.push_back(ComputeCollisionRectFromBounds(
            window->GetTargetBounds(), window->parent(),
            false /* do not inset */));
      } else {
        rects.push_back(ComputeCollisionRectFromBounds(
            window->GetTargetBounds(), window->parent()));
      }
    }
  }

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsEnabled() &&
      keyboard_controller->GetActiveContainerType() ==
          keyboard::ContainerType::kFloating &&
      keyboard_controller->GetRootWindow() == root_window &&
      !keyboard_controller->GetVisualBoundsInScreen().IsEmpty() &&
      !ShouldIgnoreWindowForCollision(keyboard_controller->GetKeyboardWindow(),
                                      priority)) {
    rects.push_back(ComputeCollisionRectFromBounds(
        keyboard_controller->visual_bounds_in_root(),
        /*parent=*/root_window));
  }

  for (auto* window :
       CaptureModeController::Get()->GetWindowsForCollisionAvoidance()) {
    rects.push_back(ComputeCollisionRectFromBounds(window->GetTargetBounds(),
                                                   window->parent()));
  }

  // Avoid clamshell-mode launcher bubble.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (app_list_controller && !Shell::Get()->IsInTabletMode() &&
      app_list_controller->GetTargetVisibility(display.id())) {
    aura::Window* window = app_list_controller->GetWindow();
    if (window) {
      rects.push_back(ComputeCollisionRectFromBounds(window->GetTargetBounds(),
                                                     window->parent()));
    }
  }

  return rects;
}

// Finds the candidate points |center| could be moved to. One of these points
// is guaranteed to be the minimum distance to move |center| to make it not
// intersect any of the rectangles in |collision_rects|.
std::vector<gfx::Point> CollectCandidatePoints(
    const gfx::Point& center,
    const std::vector<gfx::Rect>& collision_rects) {
  std::vector<gfx::Point> candidate_points;
  candidate_points.reserve(4 * collision_rects.size() * collision_rects.size() +
                           4 * collision_rects.size() + 1);
  // There are four cases for how the window will move.
  // Case #1: Touches 0 obstacles. This corresponds to not moving.
  // Case #2: Touches 1 obstacle.
  //   The result window will be touching one edge of the obstacle.
  // Case #3: Touches 2 obstacles.
  //   The result window will be touching one horizontal and one vertical edge
  //   from two different obstacles.
  // Case #4: Touches more than 2 obstacles. This is handled in case #3.

  // Case #2.
  // Rects include the left and top edges, so subtract 1 for those.
  // Prioritize horizontal movement before vertical movement.
  bool intersects = false;
  for (const auto& rectA : collision_rects) {
    intersects = intersects || rectA.Contains(center);
    candidate_points.emplace_back(rectA.x() - 1, center.y());
    candidate_points.emplace_back(rectA.right(), center.y());
    candidate_points.emplace_back(center.x(), rectA.y() - 1);
    candidate_points.emplace_back(center.x(), rectA.bottom());
  }
  // Case #1: Touching zero obstacles, so don't move the window.
  if (!intersects)
    return {};

  // Case #3: Add candidate points corresponding to each pair of horizontal
  // and vertical edges.
  for (const auto& rectA : collision_rects) {
    for (const auto& rectB : collision_rects) {
      // Continuing early here isn't necessary but makes fewer candidate points.
      if (&rectA == &rectB)
        continue;
      candidate_points.emplace_back(rectA.x() - 1, rectB.y() - 1);
      candidate_points.emplace_back(rectA.x() - 1, rectB.bottom());
      candidate_points.emplace_back(rectA.right(), rectB.y() - 1);
      candidate_points.emplace_back(rectA.right(), rectB.bottom());
    }
  }
  return candidate_points;
}

// Finds the candidate point with the shortest distance to |center| that is
// inside |work_area| and does not intersect any gfx::Rect in |rects|.
gfx::Point ComputeBestCandidatePoint(const gfx::Point& center,
                                     const gfx::Rect& work_area,
                                     const std::vector<gfx::Rect>& rects) {
  auto candidate_points = CollectCandidatePoints(center, rects);
  int64_t best_dist = -1;
  gfx::Point best_point = center;
  for (const auto& point : candidate_points) {
    if (!work_area.Contains(point))
      continue;
    bool viable = true;
    for (const auto& rect : rects) {
      if (rect.Contains(point)) {
        viable = false;
        break;
      }
    }
    if (!viable)
      continue;
    int64_t dist = (point - center).LengthSquared();
    if (dist < best_dist || best_dist == -1) {
      best_dist = dist;
      best_point = point;
    }
  }
  return best_point;
}

}  // namespace

gfx::Rect CollisionDetectionUtils::GetMovementArea(
    const display::Display& display) {
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(Shell::GetRootWindowForDisplayId(display.id()))
          ->user_work_area_bounds();

  work_area.Inset(kCollisionWindowWorkAreaInsetsDp);
  return work_area;
}

gfx::Rect CollisionDetectionUtils::AdjustToFitMovementAreaByGravity(
    const display::Display& display,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect resting_bounds = bounds_in_screen;
  gfx::Rect area = GetMovementArea(display);
  resting_bounds.AdjustToFit(area);
  const CollisionDetectionUtils::Gravity gravity =
      GetGravityToClosestEdge(resting_bounds, area);
  return GetAdjustedBoundsByGravity(resting_bounds, area, gravity);
}

gfx::Rect CollisionDetectionUtils::GetRestingPosition(
    const display::Display& display,
    const gfx::Rect& bounds_in_screen,
    CollisionDetectionUtils::RelativePriority priority) {
  gfx::Rect resting_bounds =
      AdjustToFitMovementAreaByGravity(display, bounds_in_screen);
  return AvoidObstacles(display, resting_bounds, priority);
}

void CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
    aura::Window* window) {
  window->SetProperty(kIgnoreForWindowCollisionDetection, true);
}

void CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
    aura::Window* window,
    RelativePriority priority) {
  window->SetProperty(kCollisionDetectionRelativePriority, priority);
}

gfx::Rect CollisionDetectionUtils::AvoidObstacles(
    const display::Display& display,
    const gfx::Rect& bounds_in_screen,
    RelativePriority priority) {
  gfx::Rect work_area = GetMovementArea(display);
  auto rects = CollectCollisionRects(display, priority);
  // The worst case for this should be: floating keyboard + one system tray +
  // the volume shifter + autoclick bubble menu, which is 4 windows.
  DCHECK(rects.size() <= 15)
      << "CollisionDetectionUtils::AvoidObstacles is N^3 and "
         "should be optimized if there are a lot of "
         "windows. Please see crrev.com/c/1221427 for a "
         "description of an N^2 algorithm.";
  return AvoidObstaclesInternal(work_area, rects, bounds_in_screen, priority);
}

// Returns the result of adjusting |bounds| according to |gravity| inside
// |region|.
gfx::Rect CollisionDetectionUtils::GetAdjustedBoundsByGravity(
    const gfx::Rect& bounds,
    const gfx::Rect& region,
    CollisionDetectionUtils::Gravity gravity) {
  switch (gravity) {
    case CollisionDetectionUtils::Gravity::kGravityLeft:
      return gfx::Rect(region.x(), bounds.y(), bounds.width(), bounds.height());
    case CollisionDetectionUtils::Gravity::kGravityRight:
      return gfx::Rect(region.right() - bounds.width(), bounds.y(),
                       bounds.width(), bounds.height());
    case CollisionDetectionUtils::Gravity::kGravityTop:
      return gfx::Rect(bounds.x(), region.y(), bounds.width(), bounds.height());
    case CollisionDetectionUtils::Gravity::kGravityBottom:
      return gfx::Rect(bounds.x(), region.bottom() - bounds.height(),
                       bounds.width(), bounds.height());
    default:
      NOTREACHED();
  }
}

CollisionDetectionUtils::Gravity
CollisionDetectionUtils::GetGravityToClosestEdge(const gfx::Rect& bounds,
                                                 const gfx::Rect& region) {
  const gfx::Insets insets = region.InsetsFrom(bounds);
  int minimum_edge_dist = std::min(insets.left(), insets.right());
  minimum_edge_dist = std::min(minimum_edge_dist, insets.top());
  minimum_edge_dist = std::min(minimum_edge_dist, insets.bottom());

  if (insets.left() == minimum_edge_dist) {
    return CollisionDetectionUtils::Gravity::kGravityLeft;
  } else if (insets.right() == minimum_edge_dist) {
    return CollisionDetectionUtils::Gravity::kGravityRight;
  } else if (insets.top() == minimum_edge_dist) {
    return CollisionDetectionUtils::Gravity::kGravityTop;
  } else {
    return CollisionDetectionUtils::Gravity::kGravityBottom;
  }
}

gfx::Rect CollisionDetectionUtils::AvoidObstaclesInternal(
    const gfx::Rect& work_area,
    const std::vector<gfx::Rect>& rects,
    const gfx::Rect& bounds_in_screen,
    RelativePriority priority) {
  gfx::Rect inset_work_area = work_area;

  // For even sized bounds, there is no 'center' integer point, so we need
  // to adjust the obstacles and work area to account for this.
  inset_work_area.Inset(gfx::Insets::TLBR(
      bounds_in_screen.height() / 2, bounds_in_screen.width() / 2,
      (bounds_in_screen.height() - 1) / 2, (bounds_in_screen.width() - 1) / 2));
  std::vector<gfx::Rect> inset_rects(rects);
  for (auto& rect : inset_rects) {
    // Reduce the collision resolution problem from rectangles-rectangle
    // resolution to rectangles-point resolution, by expanding each obstacle
    // by |bounds_in_screen| size.
    rect.Inset(gfx::Insets::TLBR(-(bounds_in_screen.height() - 1) / 2,
                                 -(bounds_in_screen.width() - 1) / 2,
                                 -bounds_in_screen.height() / 2,
                                 -bounds_in_screen.width() / 2));
  }

  gfx::Point moved_center = ComputeBestCandidatePoint(
      bounds_in_screen.CenterPoint(), inset_work_area, inset_rects);
  gfx::Rect moved_bounds = bounds_in_screen;
  moved_bounds.Offset(moved_center - bounds_in_screen.CenterPoint());
  return moved_bounds;
}

}  // namespace ash
