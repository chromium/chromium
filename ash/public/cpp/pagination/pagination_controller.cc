// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/pagination/pagination_controller.h"

#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/check.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

namespace {

// Constants for dealing with scroll events.
const int kMinScrollToSwitchPage = 10;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

}  // namespace

PaginationController::PaginationController(PaginationModel* model,
                                           ScrollAxis scroll_axis,
                                           const RecordMetrics& record_metrics)
    : pagination_model_(model),
      scroll_axis_(scroll_axis),
      record_metrics_(record_metrics) {
  DCHECK(pagination_model_);
  DCHECK(record_metrics_);
}

PaginationController::~PaginationController() = default;

bool PaginationController::OnScroll(const gfx::Vector2dF& offset,
                                    ui::EventType type) {
  float offset_magnitude;
  if (scroll_axis_ == SCROLL_AXIS_HORIZONTAL) {
    // If the view scrolls horizontally, both horizontal and vertical scroll
    // events are valid (since most mouse wheels only have vertical scrolling).
    offset_magnitude =
        abs(offset.x()) > abs(offset.y()) ? offset.x() : offset.y();
  } else {
    // If the view scrolls vertically, only vertical scroll events are valid.
    offset_magnitude = offset.y();
  }

  // Do not scroll on very small events.
  // TODO(calamity): This should only apply on touchpad scroll but touchpad
  // events are coming in as mousewheel events. See https://crbug.com/594264.
  if (abs(offset_magnitude) > kMinScrollToSwitchPage &&
      !pagination_model_->has_transition()) {
    const int delta = offset_magnitude > 0 ? -1 : 1;
    SelectPageAndRecordMetric(delta, type);
    return true;
  }

  return false;
}

bool PaginationController::OnGestureEvent(const ui::GestureEvent& event,
                                          const gfx::Rect& bounds) {
  const ui::GestureEventDetails& details = event.details();
  switch (event.type()) {
    case ui::EventType::kGestureScrollBegin: {
      float scroll = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                         ? details.scroll_x_hint()
                         : details.scroll_y_hint();
      return StartDrag(scroll);
    }
    case ui::EventType::kGestureScrollUpdate: {
      float scroll = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                         ? details.scroll_x()
                         : details.scroll_y();
      return UpdateDrag(scroll, bounds);
    }
    case ui::EventType::kGestureScrollEnd: {
      return EndDrag(event);
    }
    case ui::EventType::kScrollFlingStart: {
      float velocity = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                           ? details.velocity_x()
                           : details.velocity_y();

      if (fabs(velocity) > kMinHorizVelocityToSwitchPage) {
        pagination_model_->EndScroll(true);

        const int delta = velocity < 0 ? 1 : -1;
        SelectPageAndRecordMetric(delta, event.type());
      } else {
        // If the gesture ends in a fling below page switch velocity threshold,
        // decide whether to switch page depending on the scroll progress (if
        // gesture ends with a slow fling after the user has dragged the page
        // beyond page switch drag threshold, switch the page).
        EndDrag(event);
      }
      return true;
    }
    default:
      return false;
  }
}

void PaginationController::StartMouseDrag(const gfx::Vector2dF& offset) {
  float scroll =
      scroll_axis_ == SCROLL_AXIS_HORIZONTAL ? offset.x() : offset.y();
  StartDrag(scroll);
}

void PaginationController::UpdateMouseDrag(const gfx::Vector2dF& offset,
                                           const gfx::Rect& bounds) {
  float scroll =
      scroll_axis_ == SCROLL_AXIS_HORIZONTAL ? offset.x() : offset.y();
  UpdateDrag(scroll, bounds);
}

void PaginationController::EndMouseDrag(const ui::MouseEvent& event) {
  EndDrag(event);
}

bool PaginationController::StartDrag(float scroll) {
  if (scroll == 0)
    return false;
  pagination_model_->StartScroll();
  return true;
}

bool PaginationController::UpdateDrag(float scroll, const gfx::Rect& bounds) {
  if (!pagination_model_->IsValidPageRelative(scroll < 0 ? 1 : -1) &&
      !pagination_model_->has_transition()) {
    // scroll > 0 means moving contents right or down. That is,
    // transitioning to the previous page. If scrolling to an invalid page,
    // ignore the event until movement continues in a valid direction.
    return true;
  }
  int width =
      scroll_axis_ == SCROLL_AXIS_HORIZONTAL ? bounds.width() : bounds.height();

  pagination_model_->UpdateScroll(scroll / width);
  return true;
}

bool PaginationController::EndDrag(const ui::LocatedEvent& event) {
  const bool cancel_transition =
      pagination_model_->transition().progress < kFinishTransitionThreshold;
  pagination_model_->EndScroll(cancel_transition);

  if (!cancel_transition)
    record_metrics_.Run(event.type());

  return true;
}

void PaginationController::SelectPageAndRecordMetric(int delta,
                                                     ui::EventType type) {
  if (pagination_model_->IsValidPageRelative(delta)) {
    record_metrics_.Run(type);
  }
  pagination_model_->SelectPageRelative(delta, true);
}

}  // namespace ash
