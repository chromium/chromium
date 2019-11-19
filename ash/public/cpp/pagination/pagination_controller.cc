// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/pagination/pagination_controller.h"

#include "ash/public/cpp/pagination/pagination_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

namespace {

// Constants for dealing with scroll events.
const int kMinScrollToSwitchPage = 20;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

}  // namespace

PaginationController::PaginationController(PaginationModel* model,
                                           ScrollAxis scroll_axis,
                                           const RecordMetrics& record_metrics,
                                           bool is_tablet_mode)
    : pagination_model_(model),
      scroll_axis_(scroll_axis),
      record_metrics_(record_metrics),
      is_tablet_mode_(is_tablet_mode) {
  DCHECK(record_metrics_);
}

PaginationController::~PaginationController() = default;

bool PaginationController::OnScroll(const gfx::Vector2d& offset,
                                    ui::EventType type) {
  int offset_magnitude;
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
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      float scroll = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                         ? details.scroll_x_hint()
                         : details.scroll_y_hint();
      return StartDrag(scroll);
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      float scroll = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                         ? details.scroll_x()
                         : details.scroll_y();
      return UpdateDrag(scroll, bounds);
    }
    case ui::ET_GESTURE_SCROLL_END: {
      return EndDrag(event);
    }
    case ui::ET_SCROLL_FLING_START: {
      float velocity = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                           ? details.velocity_x()
                           : details.velocity_y();
      pagination_model_->EndScroll(true);

      if (fabs(velocity) > kMinHorizVelocityToSwitchPage) {
        const int delta = velocity < 0 ? 1 : -1;
        SelectPageAndRecordMetric(delta, event.type());
      }
      return true;
    }
    default:
      return false;
  }
}

void PaginationController::StartMouseDrag(const gfx::Vector2d& offset) {
  float scroll =
      scroll_axis_ == SCROLL_AXIS_HORIZONTAL ? offset.x() : offset.y();
  StartDrag(scroll);
}

void PaginationController::UpdateMouseDrag(const gfx::Vector2d& offset,
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
    record_metrics_.Run(event.type(), is_tablet_mode_);

  return true;
}

void PaginationController::SelectPageAndRecordMetric(int delta,
                                                     ui::EventType type) {
  if (pagination_model_->IsValidPageRelative(delta)) {
    record_metrics_.Run(type, is_tablet_mode_);
  }
  pagination_model_->SelectPageRelative(delta, true);
}

}  // namespace ash
