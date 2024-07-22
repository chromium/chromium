// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_event_filter.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "base/functional/callback.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

BubbleEventFilter::BubbleEventFilter(views::Widget* bubble_widget,
                                     views::View* button,
                                     OnClickedOutsideCallback on_click_outside)
    : bubble_widget_(bubble_widget),
      button_(button),
      on_click_outside_(on_click_outside) {
  Shell::Get()->AddPreTargetHandler(this);
}

BubbleEventFilter::~BubbleEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void BubbleEventFilter::SetButton(views::View* button) {
  button_ = button;
}

void BubbleEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed) {
    ProcessPressedEvent(*event);
  }
}

void BubbleEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchPressed) {
    ProcessPressedEvent(*event);
  }
}

bool BubbleEventFilter::ShouldRunOnClickOutsideCallback(
    const ui::LocatedEvent& event) {
  if (!bubble_widget_) {
    return false;
  }

  // Check the general rules for closing bubbles.
  if (!bubble_utils::ShouldCloseBubbleForEvent(event)) {
    return false;
  }

  gfx::Point event_location = event.target()
                                  ? event.target()->GetScreenLocation(event)
                                  : event.root_location();
  // Ignore clicks inside the bubble widget.
  if (bubble_widget_->GetWindowBoundsInScreen().Contains(event_location)) {
    return false;
  }

  // Ignore clicks that hit the button (which usually spawned the widget).
  // Note that we need to use `HitTestPoint()` because certain button (i.e. the
  // shelf home button) have a custom view targeter that extends its hit test
  // bounds beyond the button bounds, so when deciding whether or not to close
  // the bubble we need to do a real hit test against the button, not just check
  // if the click point is inside its bounds.
  if (button_) {
    gfx::Point point_in_button = event_location;
    views::View::ConvertPointFromScreen(button_, &point_in_button);
    if (button_->HitTestPoint(point_in_button)) {
      return false;
    }
  }

  return true;
}

void BubbleEventFilter::ProcessPressedEvent(const ui::LocatedEvent& event) {
  if (ShouldRunOnClickOutsideCallback(event)) {
    on_click_outside_.Run(event);
  }
}

}  // namespace ash
