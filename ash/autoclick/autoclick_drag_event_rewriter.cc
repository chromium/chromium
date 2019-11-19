// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_drag_event_rewriter.h"

namespace ash {

void AutoclickDragEventRewriter::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool AutoclickDragEventRewriter::IsEnabled() const {
  return enabled_;
}

ui::EventDispatchDetails AutoclickDragEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  // Only rewrite mouse moved events to drag events when enabled.
  if (!enabled_)
    return SendEvent(continuation, &event);

  // On touchpads, a SCROLL_FLING_CANCEL can also indicate the start of a drag.
  // If this rewriter is enabled, a SCROLL_FLING_CANCEL should simply be
  // ignored.
  if (event.type() == ui::ET_SCROLL_FLING_CANCEL)
    return DiscardEvent(continuation);

  // Only rewrite move events, but any other type should still go through.
  if (event.type() != ui::ET_MOUSE_MOVED)
    return SendEvent(continuation, &event);

  const ui::MouseEvent* mouse_event = event.AsMouseEvent();
  // "Press" the left mouse button to make it seem like the user is holding it
  // down.
  int flags = mouse_event->flags() | ui::EF_LEFT_MOUSE_BUTTON;
  ui::MouseEvent rewritten_event(
      ui::ET_MOUSE_DRAGGED, mouse_event->location(),
      mouse_event->root_location(), mouse_event->time_stamp(), flags,
      mouse_event->changed_button_flags(), mouse_event->pointer_details());
  return SendEventFinally(continuation, &rewritten_event);
}

}  // namespace ash
