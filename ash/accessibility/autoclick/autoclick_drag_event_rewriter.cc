// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_drag_event_rewriter.h"

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
  if (event.type() == ui::EventType::kScrollFlingCancel) {
    return DiscardEvent(continuation);
  }

  // Only rewrite move events, but any other type should still go through.
  if (event.type() != ui::EventType::kMouseMoved) {
    return SendEvent(continuation, &event);
  }

  const ui::MouseEvent* mouse_event = event.AsMouseEvent();
  // "Press" the left mouse button to make it seem like the user is holding it
  // down.
  int flags = mouse_event->flags() | ui::EF_LEFT_MOUSE_BUTTON;
  ui::MouseEvent rewritten_event(
      ui::EventType::kMouseDragged, mouse_event->location(),
      mouse_event->root_location(), mouse_event->time_stamp(), flags,
      mouse_event->changed_button_flags(), mouse_event->pointer_details());
  SetEventTarget(rewritten_event, event.target());
  // SetNativeEvent must be called explicitly as native events are not copied on
  // ChromeOS by default. This is because `PlatformEvent` is a pointer by
  // default, so its lifetime can not be guaranteed in general. In this case,
  // the lifetime of  `rewritten_event` is guaranteed to be less than the
  // original `mouse_event`.
  SetNativeEvent(rewritten_event, mouse_event->native_event());
  return SendEventFinally(continuation, &rewritten_event);
}

bool AutoclickDragEventRewriter::SupportsNonRootLocation() const {
  return true;
}

}  // namespace ash
