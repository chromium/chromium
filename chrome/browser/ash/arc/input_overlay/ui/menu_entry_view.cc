// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include "ash/app_list/app_list_util.h"
#include "base/cxx17_backports.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

MenuEntryView::MenuEntryView(
    PressedCallback pressed_callback,
    OnPositionChangedCallback on_position_changed_callback)
    : views::ImageButton(std::move(pressed_callback)),
      on_position_changed_callback_(on_position_changed_callback) {}

MenuEntryView::~MenuEntryView() = default;

bool MenuEntryView::OnMousePressed(const ui::MouseEvent& event) {
  if (allow_reposition_)
    OnDragStart(event);
  return views::Button::OnMousePressed(event);
}

bool MenuEntryView::OnMouseDragged(const ui::MouseEvent& event) {
  if (allow_reposition_)
    OnDragUpdate(event);
  return views::Button::OnMouseDragged(event);
}

void MenuEntryView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!allow_reposition_ || !is_dragging_) {
    views::Button::OnMouseReleased(event);
    MayCancelLocatedEvent(event);
  } else {
    OnDragEnd();
    RecordInputOverlayMenuEntryReposition(RepositionType::kMouseDragRepostion);
  }
}

void MenuEntryView::OnGestureEvent(ui::GestureEvent* event) {
  if (!allow_reposition_) {
    views::Button::OnGestureEvent(event);
    MayCancelLocatedEvent(*event);
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnDragUpdate(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      OnDragEnd();
      event->SetHandled();
      RecordInputOverlayMenuEntryReposition(
          RepositionType::kTouchscreenDragRepostion);
      break;
    default:
      views::Button::OnGestureEvent(event);
      break;
  }
}

bool MenuEntryView::OnKeyPressed(const ui::KeyEvent& event) {
  auto current_pos = origin();
  if (!allow_reposition_ ||
      !UpdatePositionByArrowKey(event.key_code(), current_pos)) {
    return views::ImageButton::OnKeyPressed(event);
  }

  SetPosition(current_pos);
  return true;
}

bool MenuEntryView::OnKeyReleased(const ui::KeyEvent& event) {
  if (!allow_reposition_ || !ash::IsArrowKeyEvent(event))
    return views::ImageButton::OnKeyReleased(event);

  on_position_changed_callback_.Run(/*leave_focus=*/false,
                                    absl::make_optional(origin()));
  RecordInputOverlayMenuEntryReposition(
      RepositionType::kKeyboardArrowKeyReposition);
  return true;
}

void MenuEntryView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  start_drag_view_pos_ = origin();
}

void MenuEntryView::OnDragUpdate(const ui::LocatedEvent& event) {
  is_dragging_ = true;
  auto new_location = event.location();
  auto target_location = origin() + (new_location - start_drag_event_pos_);
  target_location.set_x(base::clamp(target_location.x(), /*lo=*/0,
                                    /*hi=*/parent()->width() - width()));
  target_location.set_y(base::clamp(target_location.y(), /*lo=*/0,
                                    /*hi=*/parent()->height() - height()));
  SetPosition(target_location);
}

void MenuEntryView::OnDragEnd() {
  is_dragging_ = false;
  // When menu entry is in dragging, input events target at overlay layer. When
  // finishing drag, input events should target on the app content layer
  // underneath the overlay. So it needs to leave focus to make event target
  // leave from the overlay layer.
  on_position_changed_callback_.Run(/*leave_focus=*/true,
                                    origin() != start_drag_view_pos_
                                        ? absl::make_optional(origin())
                                        : absl::nullopt);
}

void MenuEntryView::MayCancelLocatedEvent(const ui::LocatedEvent& event) {
  if ((event.IsMouseEvent() && !HitTestPoint(event.location())) ||
      (event.IsGestureEvent() && event.type() == ui::ET_GESTURE_TAP_CANCEL)) {
    on_position_changed_callback_.Run(/*leave_focus=*/true,
                                      /*location=*/absl::nullopt);
  }
}

}  // namespace arc::input_overlay
