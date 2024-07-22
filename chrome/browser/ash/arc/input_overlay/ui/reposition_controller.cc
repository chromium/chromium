// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/reposition_controller.h"

#include "ash/app_list/app_list_util.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

namespace {

// Clamp position `position` inside of the `parent_size` with padding of
// `parent_padding`
void ClampPosition(gfx::Point& position,
                   const gfx::Size& ui_size,
                   const gfx::Size& parent_size,
                   int parent_padding) {
  int lo = parent_padding;
  int hi = parent_size.width() - ui_size.width() - parent_padding;
  if (lo >= hi) {
    // Ignore `parent_padding` if there is not enough space.
    lo = 0;
    hi += parent_padding;
  }
  position.set_x(std::clamp(position.x(), lo, hi));

  lo = parent_padding;
  hi = parent_size.height() - ui_size.height() - parent_padding;
  if (lo >= hi) {
    // Ignore `parent_padding` if there is not enough space.
    lo = 0;
    hi += parent_padding;
  }
  position.set_y(std::clamp(position.y(), lo, hi));
}

}  // namespace

RepositionController::RepositionController(views::View* host_view,
                                           int parent_padding)
    : host_view_(host_view), parent_padding_(parent_padding) {}

RepositionController::~RepositionController() = default;

void RepositionController::OnMousePressed(const ui::MouseEvent& event) {
  OnDragStart(event);
}

void RepositionController::OnMouseDragged(const ui::MouseEvent& event) {
  OnDragUpdate(event);
}

bool RepositionController::OnMouseReleased(const ui::MouseEvent& event) {
  if (is_dragging_) {
    OnDragEnd(event);
    return true;
  }
  return false;
}

bool RepositionController::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollUpdate:
      OnDragUpdate(*event);
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      if (!is_dragging_) {
        return false;
      }
      OnDragEnd(*event);
      event->SetHandled();
      break;
    default:
      return false;
  }
  return true;
}

bool RepositionController::OnKeyPressed(const ui::KeyEvent& event) {
  if (auto target_position = host_view_->origin();
      OffsetPositionByArrowKey(event.key_code(), target_position)) {
    ClampPosition(target_position, host_view_->size(),
                  host_view_->parent()->size(), parent_padding_);
    host_view_->SetPosition(target_position);
    return true;
  }
  return false;
}

bool RepositionController::OnKeyReleased(const ui::KeyEvent& event) {
  if (!ash::IsArrowKeyEvent(event)) {
    return false;
  }

  if (!key_released_callback_.is_null()) {
    key_released_callback_.Run();
  }

  return true;
}

void RepositionController::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  ResetFocusTo(host_view_->parent());
}

void RepositionController::OnDragUpdate(const ui::LocatedEvent& event) {
  if (!is_dragging_) {
    is_dragging_ = true;
    if (!first_dragging_callback_.is_null()) {
      first_dragging_callback_.Run();
    }
  }

  auto target_position =
      host_view_->origin() + (event.location() - start_drag_event_pos_);
  ClampPosition(target_position, host_view_->size(),
                host_view_->parent()->size(), parent_padding_);
  host_view_->SetPosition(target_position);
  if (!dragging_callback_.is_null()) {
    dragging_callback_.Run();
  }
}

void RepositionController::OnDragEnd(const ui::LocatedEvent& event) {
  is_dragging_ = false;
  if (event.IsMouseEvent() && !mouse_drag_end_callback_.is_null()) {
    mouse_drag_end_callback_.Run();
  } else if (event.IsGestureEvent() && !gesture_drag_end_callback_.is_null()) {
    gesture_drag_end_callback_.Run();
  }
}

}  // namespace arc::input_overlay
