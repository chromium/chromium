// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_event_filter.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/callback.h"
#include "base/check.h"
#include "ui/aura/window.h"
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
  DCHECK(on_click_outside_);
  Shell::Get()->AddPreTargetHandler(this);
}

AppListBubbleEventFilter::~AppListBubbleEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListBubbleEventFilter::SetButton(views::View* button) {
  button_ = button;
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

  // Ignore clicks that hit the button (which usually spawned the widget).
  // Use HitTestPoint() because the shelf home button has a custom view targeter
  // that handles clicks outside its bounds, like in the corner of the screen.
  if (button_) {
    gfx::Point point_in_button = event_location;
    views::View::ConvertPointFromScreen(button_, &point_in_button);
    if (button_->HitTestPoint(point_in_button))
      return;
  }

  // Ignore clicks in the shelf area containing app icons.
  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (target) {
    Shelf* shelf = Shelf::ForWindow(target);
    if (target == shelf->hotseat_widget()->GetNativeWindow() &&
        shelf->hotseat_widget()->EventTargetsShelfView(event)) {
      return;
    }

    // Don't dismiss the auto-hide shelf if event happened in status area. Then
    // the event can still be propagated.
    const aura::Window* status_window =
        shelf->shelf_widget()->status_area_widget()->GetNativeWindow();
    if (status_window && status_window->Contains(target))
      return;
  }

  on_click_outside_.Run();
}

}  // namespace ash
