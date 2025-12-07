// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_event_filter.h"

#include "ash/shell.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace ash {

SystemModalContainerEventFilter::SystemModalContainerEventFilter(
    SystemModalContainerEventFilterDelegate* delegate)
    : delegate_(delegate) {}

SystemModalContainerEventFilter::~SystemModalContainerEventFilter() = default;

void SystemModalContainerEventFilter::OnEvent(ui::Event* event) {
  // Let gesture end events through to let event handler reset their gesture
  // state. For example, widget root view uses gesture end event to reset its
  // gesture handler, so if it misses gesture end event, subsequent gesture
  // events may get sent to the gesture handler for the gesture that's ended.
  if (event->type() == ui::EventType::kGestureEnd) {
    return;
  }

  // Only filter modal events if a modal window is open.
  if (!Shell::IsSystemModalWindowOpen())
    return;
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!delegate_->CanWindowReceiveEvents(target))
    event->StopPropagation();
}

}  // namespace ash
