// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include "base/cxx17_backports.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

MenuEntryView::MenuEntryView(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {}

MenuEntryView::~MenuEntryView() = default;

bool MenuEntryView::OnMousePressed(const ui::MouseEvent& event) {
  if (beta_)
    OnDragStart(event);
  return views::Button::OnMousePressed(event);
}

bool MenuEntryView::OnMouseDragged(const ui::MouseEvent& event) {
  if (beta_)
    OnDragUpdate(event);
  return views::Button::OnMouseDragged(event);
}

void MenuEntryView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!beta_ || !is_dragging_) {
    views::Button::OnMouseReleased(event);
  } else {
    OnDragEnd();
  }
}

void MenuEntryView::OnGestureEvent(ui::GestureEvent* event) {
  if (!beta_)
    return views::Button::OnGestureEvent(event);
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
      break;
    default:
      views::Button::OnGestureEvent(event);
      break;
  }
}

void MenuEntryView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_pos_ = event.location();
}

void MenuEntryView::OnDragUpdate(const ui::LocatedEvent& event) {
  is_dragging_ = true;
  auto new_location = event.location();
  auto target_location = origin() + (new_location - start_drag_pos_);
  target_location.set_x(base::clamp(target_location.x(), /*lo=*/0,
                                    /*hi=*/parent()->width() - width()));
  target_location.set_y(base::clamp(target_location.y(), /*lo=*/0,
                                    /*hi=*/parent()->height() - height()));
  SetPosition(target_location);
}

void MenuEntryView::OnDragEnd() {
  is_dragging_ = false;
}

}  // namespace arc::input_overlay
