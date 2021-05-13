// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_event_filter.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/check.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListBubbleEventFilter::AppListBubbleEventFilter(
    views::Widget* widget,
    views::View* button,
    base::RepeatingCallback<void()> on_click_outside)
    : widget_(widget), button_(button), on_click_outside_(on_click_outside) {
  DCHECK(widget_);
  DCHECK(button_);
  DCHECK(on_click_outside_);
  Shell::Get()->AddPreTargetHandler(this);
}

AppListBubbleEventFilter::~AppListBubbleEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListBubbleEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(*event);
}

void AppListBubbleEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(*event);
}

void AppListBubbleEventFilter::ProcessPressedEvent(
    const ui::LocatedEvent& event) {
  // Check the general rules for closing bubbles.
  if (!bubble_utils::ShouldCloseBubbleForEvent(event))
    return;

  gfx::Point event_location = event.target()
                                  ? event.target()->GetScreenLocation(event)
                                  : event.root_location();
  // Ignore clicks inside the widget.
  if (widget_->GetWindowBoundsInScreen().Contains(event_location))
    return;

  // Ignore clicks inside the button (which usually spawned the widget).
  if (button_->GetBoundsInScreen().Contains(event_location))
    return;

  on_click_outside_.Run();
}

}  // namespace ash
