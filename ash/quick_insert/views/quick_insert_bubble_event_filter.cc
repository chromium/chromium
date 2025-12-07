// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_bubble_event_filter.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/bubble/bubble_utils.h"
#include "base/functional/callback.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {

QuickInsertBubbleEventFilter::QuickInsertBubbleEventFilter(
    views::Widget* widget)
    : BubbleEventFilter(
          widget,
          /*button=*/nullptr,
          // base::Unretained is safe here because `widget` outlives this.
          base::BindRepeating(
              &QuickInsertBubbleEventFilter::OnClickOutsideWidget,
              base::Unretained(this))),
      widget_(widget) {}

QuickInsertBubbleEventFilter::~QuickInsertBubbleEventFilter() = default;

bool QuickInsertBubbleEventFilter::ShouldRunOnClickOutsideCallback(
    const ui::LocatedEvent& event) {
  // Check the general rules for closing bubbles.
  if (!bubble_utils::ShouldCloseBubbleForEvent(event)) {
    return false;
  }

  const gfx::Point event_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();

  // Perform a hit-test for every child widget of `widget_` (which includes
  // itself).
  views::Widget::Widgets widgets =
      views::Widget::GetAllChildWidgets(widget_->GetNativeView());
  for (views::Widget* child : widgets) {
    // Ignore clicks inside the bubble widget.
    if (child->GetWindowBoundsInScreen().Contains(event_location)) {
      return false;
    }
  }

  return true;
}

void QuickInsertBubbleEventFilter::OnClickOutsideWidget(
    const ui::LocatedEvent& event) {
  widget_->Close();
}

}  // namespace ash
