// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
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

}  // namespace

PipWindowResizer::PipWindowResizer(WindowState* window_state)
    : WindowResizer(window_state) {
  window_state->OnDragStarted(details().window_component);

  // TODO(b/301232629): `DragDetails::bounds_change` should be
  // `kBoundsChange_Resizes` during pinch resizing.
  bool is_resize = (details().bounds_change & kBoundsChange_Resizes) ||
                   Shell::Get()->toplevel_window_event_handler()->in_pinch();
  if (is_resize) {
    UMA_HISTOGRAM_ENUMERATION(kAshPipEventsHistogramName,
                              AshPipEvents::FREE_RESIZE);
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
void PipWindowResizer::Drag(const gfx::PointF& location_in_parent,
                            int event_flags) {
  last_location_in_screen_ = location_in_parent;
  last_event_was_pinch_ = false;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_location_in_screen_.value());

  gfx::Vector2dF movement_direction =
      location_in_parent - details().initial_location_in_parent;
  // If we are not sure if this is a swipe or not yet, don't modify any bounds.
  float movement_distance2 = movement_direction.x() * movement_direction.x() +
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
  if (in_screen_fraction_ == 1.f) {
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
    Shell::Get()->pip_controller()->SetDimOpacity(0.f);
    GetTarget()->layer()->SetOpacity(1.f);
    new_bounds = PipPositioner::GetBoundsForDrag(display, new_bounds,
                                                 GetTarget()->transform());
  } else {
    gfx::Rect in_screen_bounds = new_bounds;
    in_screen_bounds.Intersect(area);
    float bounds_area = new_bounds.width() * new_bounds.height();
    float in_screen_area = in_screen_bounds.width() * in_screen_bounds.height();
    if (bounds_area != 0.f) {
      in_screen_fraction_ = in_screen_area / bounds_area;
      if (may_dismiss_horizontally_ && features::IsPipTuckEnabled()) {
        if (bounds_area != 0.f) {
          Shell::Get()->pip_controller()->SetDimOpacity(
              fmin(0.5f, 1 - in_screen_fraction_));
        }
      } else {
        GetTarget()->layer()->SetOpacity(in_screen_fraction_);
      }
    }
  }

  // If the user has dragged the PIP window more than kPipDismissSlop distance
  // and no dismiss gesture has begun, make it impossible to initiate one for
  // the rest of the drag.
  if (in_screen_fraction_ == 1.f &&
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

void PipWindowResizer::Pinch(const gfx::PointF& location_in_parent,
                             const float scale) {
  accumulated_scale_ *= scale;

  last_location_in_screen_ = location_in_parent;
  last_event_was_pinch_ = true;

  gfx::Rect new_bounds = CalculateBoundsForPinch(location_in_parent);

  // We do everything in screen coordinates, so convert here.
  wm::ConvertPointToScreen(GetTarget()->parent(),
                           &last_location_in_screen_.value());
  wm::ConvertRectToScreen(GetTarget()->parent(), &new_bounds);

  // Ensure that the PiP window stays inside the PiP movement area.
  // This has to be consistent with `PipWindowResizer::Drag()`, as otherwise
  // it can cause jump during transition from pinch to drag. This could be
  // due to change (b/292768858).
  display::Display display = window_state()->GetDisplay();
  new_bounds = PipPositioner::GetBoundsForDrag(display, new_bounds,
                                               GetTarget()->transform());

  // Convert back to root window coordinates for setting bounds.
  wm::ConvertRectFromScreen(GetTarget()->parent(), &new_bounds);
  if (new_bounds != GetTarget()->bounds()) {
    moved_or_resized_ = true;
    SetBoundsDuringResize(new_bounds);
  }
}

gfx::Rect PipWindowResizer::CalculateBoundsForPinch(
    const gfx::PointF& location_in_parent) const {
  const gfx::PointF initial_location = details().initial_location_in_parent;
  const gfx::Rect initial_bounds = details().initial_bounds_in_parent;

  gfx::Size size =
      gfx::ScaleToRoundedSize(initial_bounds.size(), accumulated_scale_);

  gfx::Size max_size = GetTarget()->delegate()->GetMaximumSize();
  gfx::Size min_size = GetTarget()->delegate()->GetMinimumSize();
  size.SetToMin(max_size);
  size.SetToMax(min_size);

  gfx::SizeF* aspect_ratio_size =
      GetTarget()->GetProperty(aura::client::kAspectRatio);
  // Aspect ratio must be set for pinch-to-resize to change window bounds.
  if (!aspect_ratio_size) {
    return initial_bounds;
  }
  float aspect_ratio = aspect_ratio_size->width() / aspect_ratio_size->height();

  gfx::Rect new_bounds(GetTarget()->bounds().origin(), size);
  gfx::SizeRectToAspectRatio(gfx::ResizeEdge::kBottom, aspect_ratio, min_size,
                             max_size, &new_bounds);

  // `gfx::SizeRectToAspectRatio()` is not designed for pinch and cannot
  // calculate origin change in regards to pinch, so we calculate the origin
  // change here.
  const float left_ratio =
      (initial_location.x() - initial_bounds.x()) / initial_bounds.width();
  const float top_ratio =
      (initial_location.y() - initial_bounds.y()) / initial_bounds.height();

  new_bounds.set_x(location_in_parent.x() -
                   new_bounds.width() *
                       GetTarget()->transform().To2dScale().x() * left_ratio);
  new_bounds.set_y(location_in_parent.y() -
                   new_bounds.height() *
                       GetTarget()->transform().To2dScale().y() * top_ratio);

  return new_bounds;
}

void PipWindowResizer::CompleteDrag() {
  window_state()->OnCompleteDrag(
      last_location_in_screen_.value_or(gfx::PointF()));

  window_state()->ClearRestoreBounds();
  window_state()->SetBoundsChangedByUser(moved_or_resized_);

  int fling_amount = fling_velocity_x_ * fling_velocity_x_ +
                     fling_velocity_y_ * fling_velocity_y_;
  // Trigger a dismiss if less than |kPipDismissFraction| of the PIP window area
  // is on-screen, or, if it was flung faster than
  // |kPipSwipeToDimissFlingThresholdSquared| during a dismiss gesture.
  bool should_dismiss_or_tuck =
      in_screen_fraction_ < kPipDismissFraction ||
      (in_screen_fraction_ != 1.f &&
       fling_amount >= kPipSwipeToDismissFlingThresholdSquared);
  bool should_tuck = should_dismiss_or_tuck && may_dismiss_horizontally_ &&
                     features::IsPipTuckEnabled();
  bool should_dismiss = should_dismiss_or_tuck && !should_tuck;

  if (should_dismiss) {
    // Close the widget. This will trigger an animation dismissing the PIP
    // window.
    window_util::CloseWidgetForWindow(window_state()->window());
  } else if (should_tuck) {
    const gfx::Point display_bounds_center =
        display::Screen::GetScreen()
            ->GetDisplayMatching(GetTarget()->bounds())
            .bounds()
            .CenterPoint();
    const gfx::Point center_point = GetTarget()->bounds().CenterPoint();
    bool left = center_point.x() < display_bounds_center.x();
    Shell::Get()->pip_controller()->TuckWindow(left);
  } else {
    // Animate the PIP window to its resting position.
    gfx::Rect intended_bounds;

    // The window should be flung when the gesture is not an edge resize.
    const bool is_edge_resize = details().bounds_change & kBoundsChange_Resizes;
    if (!is_edge_resize && fling_amount > kPipMovementFlingThresholdSquared) {
      intended_bounds = ComputeFlungPosition();
    } else {
      if (last_location_in_screen_.has_value()) {
        // To calculate the resting position, we want to use the user's
        // intended bounds (bounds that are not restricted by
        // obstacles).
        gfx::PointF location_in_parent = last_location_in_screen_.value();
        wm::ConvertPointFromScreen(GetTarget()->parent(), &location_in_parent);
        intended_bounds = last_event_was_pinch_
                              ? CalculateBoundsForPinch(location_in_parent)
                              : CalculateBoundsForDrag(location_in_parent);
        wm::ConvertRectToScreen(window_state()->window()->GetRootWindow(),
                                &intended_bounds);
      } else {
        intended_bounds = GetTarget()->GetBoundsInScreen();
      }
      // Reset the dimmer for tucking.
      Shell::Get()->pip_controller()->SetDimOpacity(0.f);
    }

    // Compute resting position even if it was a fling to avoid obstacles.
    gfx::Rect resting_bounds = CollisionDetectionUtils::GetRestingPosition(
        window_state()->GetDisplay(), intended_bounds,
        CollisionDetectionUtils::RelativePriority::kPictureInPicture);
    ::wm::ConvertRectFromScreen(GetTarget()->parent(), &resting_bounds);

    base::TimeDelta duration =
        base::Milliseconds(kPipSnapToEdgeAnimationDurationMs);
    SetBoundsWMEvent event(resting_bounds, /*animate=*/true, duration);
    window_state()->OnWMEvent(&event);

    ui::Layer* layer = GetTarget()->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(duration);

    // Animate opacity back to normal opacity.
    layer->SetOpacity(1.f);

    // Animate the size back to its limit size if it has expanded or
    // shrunk beyond it.
    layer->SetTransform(gfx::Transform());

    // If the pip work area changes (e.g. message center, virtual keyboard),
    // we want to restore to the last explicitly set position.
    // TODO(edcourtney): This may not be the best place for this. Consider
    // doing this a different way or saving these bounds at a later point when
    // the work area changes.
    wm::ConvertRectToScreen(window_state()->window()->GetRootWindow(),
                                &resting_bounds);
    PipPositioner::SaveSnapFraction(window_state(), resting_bounds);
  }
}

void PipWindowResizer::RevertDrag() {
  // Handle cancel as a complete drag for pip. Having the PIP window
  // go back to where it was on cancel looks strange, so instead just
  // will just stop it where it is and animate to the edge of the screen.
  CompleteDrag();
}

void PipWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kScrollFlingStart &&
      event->type() != ui::EventType::kGestureSwipe) {
    return;
  }

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
