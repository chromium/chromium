// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <algorithm>
#include <utility>

#include "ash/metrics/pip_uma.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
// Radius in which the touch can move in a non-dismiss direction before we
// no longer consider this gesture as a candidate for swipe-to-dismiss.
const int kPipDismissSlop = 8;
// How much area by proportion needs to be off-screen to consider this
// a dismissal during swipe-to-dismiss.
const float kPipDismissFraction = 0.5f;
// TODO(edcourtney): Consider varying the animation duration based on how far
// the pip window has to move.
const int kPipSnapToEdgeAnimationDurationMs = 150;
// Threshold for considering drag-moving a PIP window to fling in the
// direction of movement in GestureEvent velocity units.
const int kPipMovementFlingThresholdSquared = 1000 * 1000;
// Threshold for considering a swipe off the side of the screen a dismissal
// even if less than |kPipDismissFraction| of the PIP window is off-screen.
const int kPipSwipeToDismissFlingThresholdSquared = 800 * 800;

bool IsAtTopOrBottomEdge(const gfx::Rect& bounds, const gfx::Rect& area) {
  return (bounds.y() < area.y() + kPipDismissSlop && bounds.y() >= area.y()) ||
         (bounds.bottom() > area.bottom() - kPipDismissSlop &&
          bounds.bottom() <= area.bottom());
}

bool IsPastTopOrBottomEdge(const gfx::Rect& bounds, const gfx::Rect& area) {
  return bounds.y() < area.y() || bounds.bottom() > area.bottom();
}

bool IsAtLeftOrRightEdge(const gfx::Rect& bounds, const gfx::Rect& area) {
  return (bounds.x() < area.x() + kPipDismissSlop && bounds.x() >= area.x()) ||
         (bounds.right() > area.right() - kPipDismissSlop &&
          bounds.right() <= area.right());
}

bool IsPastLeftOrRightEdge(const gfx::Rect& bounds, const gfx::Rect& area) {
  return bounds.x() < area.x() || bounds.right() > area.right();
}

void CollectFreeResizeAreaMetric(const char* metric_name,
                                 aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  const gfx::Rect bounds = window->GetBoundsInRootWindow();
  const int root_window_area =
      root_window->bounds().width() * root_window->bounds().height();
  const int window_area = bounds.width() * bounds.height();
  if (root_window_area != 0) {
    const int percentage =
        std::round(float(window_area) / float(root_window_area) * 100.f);
    base::UmaHistogramPercentage(metric_name, percentage);
  }
}

int ComputeIntersectionArea(const gfx::Rect& ninth, const gfx::Rect& bounds) {
  gfx::Rect intersection = ninth;
  intersection.Intersect(bounds);
  return intersection.width() * intersection.height();
}

gfx::Rect ScaleRect(const gfx::Rect& rect, int scale) {
  return gfx::Rect(rect.x() * scale, rect.y() * scale, rect.width() * scale,
                   rect.height() * scale);
}

void CollectPositionMetric(const gfx::Rect& bounds_in_screen,
                           const gfx::Rect& area_in_screen) {
  const int width = area_in_screen.width();
  const int height = area_in_screen.height();
  // Scale by three to avoid truncation.
  const gfx::Rect area = ScaleRect(area_in_screen, 3);
  const gfx::Rect bounds = ScaleRect(bounds_in_screen, 3);

  // Choose corners first, then edges, and finally middle in the case of a tie.
  // This is based on the enum integer values.
  // For this to work, all of the 9 buckets need to have the same area.
  std::pair<int, AshPipPosition> area_ninths[9] = {
      {ComputeIntersectionArea(
           gfx::Rect(area.x() + width, area.y() + height, width, height),
           bounds),
       AshPipPosition::MIDDLE},
      {ComputeIntersectionArea(
           gfx::Rect(area.x() + width, area.y(), width, height), bounds),
       AshPipPosition::TOP_MIDDLE},
      {ComputeIntersectionArea(
           gfx::Rect(area.x(), area.y() + height, width, height), bounds),
       AshPipPosition::MIDDLE_LEFT},
      {ComputeIntersectionArea(
           gfx::Rect(area.x() + 2 * width, area.y() + height, width, height),
           bounds),
       AshPipPosition::MIDDLE_RIGHT},
      {ComputeIntersectionArea(
           gfx::Rect(area.x() + width, area.y() + 2 * height, width, height),
           bounds),
       AshPipPosition::BOTTOM_MIDDLE},
      {ComputeIntersectionArea(gfx::Rect(area.x(), area.y(), width, height),
                               bounds),
       AshPipPosition::TOP_LEFT},
      {ComputeIntersectionArea(
           gfx::Rect(area.x() + 2 * width, area.y(), width, height), bounds),
       AshPipPosition::TOP_RIGHT},
      {ComputeIntersectionArea(
           gfx::Rect(area.x(), area.y() + 2 * height, width, height), bounds),
       AshPipPosition::BOTTOM_LEFT},
      {ComputeIntersectionArea(gfx::Rect(area.x() + 2 * width,
                                         area.y() + 2 * height, width, height),
                               bounds),
       AshPipPosition::BOTTOM_RIGHT}};

  std::sort(area_ninths, area_ninths + base::size(area_ninths));
  UMA_HISTOGRAM_ENUMERATION(kAshPipPositionHistogramName,
                            area_ninths[8].second);
}

}  // namespace

PipWindowResizer::PipWindowResizer(WindowState* window_state)
    : WindowResizer(window_state) {
  window_state->OnDragStarted(details().window_component);

  bool is_resize = details().bounds_change & kBoundsChange_Resizes;
  if (is_resize) {
    UMA_HISTOGRAM_ENUMERATION(kAshPipEventsHistogramName,
                              AshPipEvents::FREE_RESIZE);
    CollectFreeResizeAreaMetric(kAshPipFreeResizeInitialAreaHistogramName,
                                GetTarget());
  } else {
    // Don't allow swipe-to-dismiss for resizes.
    gfx::Rect area =
        CollisionDetectionUtils::GetMovementArea(window_state->GetDisplay());
    // Check in which directions we can dismiss. Usually this is only in one
    // direction, except when the PIP window is in the corner. In that case,
    // we initially mark both directions as viable, and later choose one based
    // on the direction of drag.
    may_dismiss_horizontally_ =
        IsAtLeftOrRightEdge(GetTarget()->GetBoundsInScreen(), area);
    may_dismiss_vertically_ =
        IsAtTopOrBottomEdge(GetTarget()->GetBoundsInScreen(), area);
  }
}

PipWindowResizer::~PipWindowResizer() {
  // Drag details should be deleted upon destruction of the resizer.
  if (window_state())
    window_state()->DeleteDragDetails();
}

// TODO(edcourtney): Implement swipe-to-dismiss on fling.
void PipWindowResizer::Drag(const gfx::Point& location_in_parent,
                            int event_flags) {
  last_location_in_screen_ = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &last_location_in_screen_);

  gfx::Vector2d movement_direction =
      location_in_parent - details().initial_location_in_parent;
  // If we are not sure if this is a swipe or not yet, don't modify any bounds.
  int movement_distance2 = movement_direction.x() * movement_direction.x() +
                           movement_direction.y() * movement_direction.y();
  if ((may_dismiss_horizontally_ || may_dismiss_vertically_) &&
      movement_distance2 <= kPipDismissSlop * kPipDismissSlop) {
    return;
  }

  gfx::Rect new_bounds = CalculateBoundsForDrag(location_in_parent);
  // We do everything in Screen coordinates, so convert here.
  ::wm::ConvertRectToScreen(GetTarget()->parent(), &new_bounds);

  display::Display display = window_state()->GetDisplay();
  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(display);

  // If the PIP window is at a corner, lock swipe to dismiss to the axis
  // of movement. Require that the direction of movement is mainly in the
  // direction of dismissing to start a swipe-to-dismiss gesture.
  if (dismiss_fraction_ == 1.f) {
    bool swipe_is_horizontal =
        std::abs(movement_direction.x()) > std::abs(movement_direction.y());
    may_dismiss_horizontally_ =
        may_dismiss_horizontally_ && swipe_is_horizontal;
    may_dismiss_vertically_ = may_dismiss_vertically_ && !swipe_is_horizontal;
  }

  // Lock to the axis if we've started the swipe-to-dismiss, or, if the PIP
  // window is no longer poking outside of the movement area, disable any
  // further swipe-to-dismiss gesture for this drag. Use the initial bounds
  // to decide the locked axis position.
  gfx::Rect initial_bounds_in_screen = details().initial_bounds_in_parent;
  ::wm::ConvertRectToScreen(GetTarget()->parent(), &initial_bounds_in_screen);
  if (may_dismiss_horizontally_) {
    if (IsPastLeftOrRightEdge(new_bounds, area))
      new_bounds.set_y(initial_bounds_in_screen.y());
    else if (!IsAtLeftOrRightEdge(new_bounds, area))
      may_dismiss_horizontally_ = false;
  } else if (may_dismiss_vertically_) {
    if (IsPastTopOrBottomEdge(new_bounds, area))
      new_bounds.set_x(initial_bounds_in_screen.x());
    else if (!IsAtTopOrBottomEdge(new_bounds, area))
      may_dismiss_vertically_ = false;
  }

  // If we aren't dismissing, make sure to collide with objects.
  if (!may_dismiss_horizontally_ && !may_dismiss_vertically_) {
    // Reset opacity if it's not a dismiss gesture.
    GetTarget()->layer()->SetOpacity(1.f);
    new_bounds = PipPositioner::GetBoundsForDrag(display, new_bounds);
  } else {
    gfx::Rect dismiss_bounds = new_bounds;
    dismiss_bounds.Intersect(area);
    float bounds_area = new_bounds.width() * new_bounds.height();
    float dismiss_area = dismiss_bounds.width() * dismiss_bounds.height();
    if (bounds_area != 0.f) {
      dismiss_fraction_ = dismiss_area / bounds_area;
      GetTarget()->layer()->SetOpacity(dismiss_fraction_);
    }
  }

  // If the user has dragged the PIP window more than kPipDismissSlop distance
  // and no dismiss gesture has begun, make it impossible to initiate one for
  // the rest of the drag.
  if (dismiss_fraction_ == 1.f &&
      movement_distance2 > kPipDismissSlop * kPipDismissSlop) {
    may_dismiss_horizontally_ = false;
    may_dismiss_vertically_ = false;
  }

  // Convert back to root window coordinates for setting bounds.
  ::wm::ConvertRectFromScreen(GetTarget()->parent(), &new_bounds);
  if (new_bounds != GetTarget()->bounds()) {
    moved_or_resized_ = true;
    SetBoundsDuringResize(new_bounds);
  }
}

void PipWindowResizer::CompleteDrag() {
  const bool is_resize = details().bounds_change & kBoundsChange_Resizes;
  if (is_resize) {
    CollectFreeResizeAreaMetric(kAshPipFreeResizeFinishAreaHistogramName,
                                GetTarget());
  } else {
    // Collect final position on drag-move.
    display::Display display = window_state()->GetDisplay();
    gfx::Rect area = CollisionDetectionUtils::GetMovementArea(display);
    CollectPositionMetric(GetTarget()->GetBoundsInScreen(), area);
  }

  window_state()->OnCompleteDrag(last_location_in_screen_);

  window_state()->ClearRestoreBounds();
  window_state()->set_bounds_changed_by_user(moved_or_resized_);

  int fling_amount = fling_velocity_x_ * fling_velocity_x_ +
                     fling_velocity_y_ * fling_velocity_y_;
  // Trigger a dismiss if less than |kPipDismissFraction| of the PIP window area
  // is on-screen, or, if it was flung faster than
  // |kPipSwipeToDimissFlingThresholdSquared| during a dismiss gesture.
  bool should_dismiss =
      dismiss_fraction_ < kPipDismissFraction ||
      (dismiss_fraction_ != 1.f &&
       fling_amount >= kPipSwipeToDismissFlingThresholdSquared);
  if (should_dismiss) {
    // Close the widget. This will trigger an animation dismissing the PIP
    // window.
    window_util::CloseWidgetForWindow(window_state()->window());
  } else {
    // Animate the PIP window to its resting position.
    gfx::Rect bounds;
    if (!is_resize && fling_amount > kPipMovementFlingThresholdSquared) {
      bounds = ComputeFlungPosition();
    } else {
      bounds = GetTarget()->GetBoundsInScreen();
    }

    // Compute resting position even if it was a fling to avoid obstacles.
    bounds = CollisionDetectionUtils::GetRestingPosition(
        window_state()->GetDisplay(), bounds,
        CollisionDetectionUtils::RelativePriority::kPictureInPicture);

    base::TimeDelta duration =
        base::TimeDelta::FromMilliseconds(kPipSnapToEdgeAnimationDurationMs);
    ::wm::ConvertRectFromScreen(GetTarget()->parent(), &bounds);
    SetBoundsWMEvent event(bounds, /*animate=*/true, duration);
    window_state()->OnWMEvent(&event);

    // Animate opacity back to normal opacity:
    ui::Layer* layer = GetTarget()->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(duration);
    layer->SetOpacity(1.f);

    // If the pip work area changes (e.g. message center, virtual keyboard),
    // we want to restore to the last explicitly set position.
    // TODO(edcourtney): This may not be the best place for this. Consider
    // doing this a different way or saving these bounds at a later point when
    // the work area changes.
    window_state()->SetRestoreBoundsInParent(bounds);
  }
}

void PipWindowResizer::RevertDrag() {
  // Handle cancel as a complete drag for pip. Having the PIP window
  // go back to where it was on cancel looks strange, so instead just
  // will just stop it where it is and animate to the edge of the screen.
  CompleteDrag();
}

void PipWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  fling_velocity_x_ = event->details().velocity_x();
  fling_velocity_y_ = event->details().velocity_y();
  CompleteDrag();
}

gfx::Rect PipWindowResizer::ComputeFlungPosition() {
  gfx::Rect bounds = GetTarget()->GetBoundsInScreen();

  // Undefined fling direction, don't move.
  if (fling_velocity_x_ == 0 && fling_velocity_y_ == 0)
    return bounds;

  gfx::Rect area =
      CollisionDetectionUtils::GetMovementArea(window_state()->GetDisplay());

  // Compute signed distance to travel in x and y axes.
  int x_dist = 0;
  if (fling_velocity_x_ < 0)
    x_dist = area.x() - bounds.x();
  else if (fling_velocity_x_ > 0)
    x_dist = area.right() - bounds.x() - bounds.width();

  int y_dist = 0;
  if (fling_velocity_y_ < 0)
    y_dist = area.y() - bounds.y();
  else if (fling_velocity_y_ > 0)
    y_dist = area.bottom() - bounds.y() - bounds.height();

  // Compute which axis is limiting the movement, then offset.
  if (fling_velocity_x_ == 0 || std::abs(x_dist * fling_velocity_y_) >
                                    std::abs(y_dist * fling_velocity_x_)) {
    bounds.Offset((y_dist * fling_velocity_x_) / fling_velocity_y_, y_dist);
  } else {
    bounds.Offset(x_dist, (x_dist * fling_velocity_y_) / fling_velocity_x_);
  }

  return bounds;
}

}  // namespace ash
