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

ui::EventRewriteStatus AutoclickDragEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  // Only rewrite mouse moved events to drag events when enabled.
  if (!enabled_ || event.type() != ui::ET_MOUSE_MOVED)
    return ui::EVENT_REWRITE_CONTINUE;
  // TODO(katie): Should this have an ui::EF_LEFT_MOUSE_BUTTON flag for drag?
  const ui::MouseEvent* mouse_event = event.AsMouseEvent();
  ui::MouseEvent* rewritten_event = new ui::MouseEvent(
      ui::ET_MOUSE_DRAGGED, mouse_event->location(),
      mouse_event->root_location(), mouse_event->time_stamp(),
      mouse_event->flags(), mouse_event->changed_button_flags(),
      mouse_event->pointer_details());
  new_event->reset(rewritten_event);
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus AutoclickDragEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  // Unused.
  return ui::EVENT_REWRITE_CONTINUE;
}

}  // namespace ash
