// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_event_handler.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d.h"

namespace keyboard {

void KeyboardEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGesturePinchBegin:
    case ui::EventType::kGesturePinchEnd:
    case ui::EventType::kGesturePinchUpdate:
      event->StopPropagation();
      break;
    default:
      auto* controller = KeyboardUIController::Get();
      if (controller->IsEnabled() && controller->HandleGestureEvent(*event))
        event->SetHandled();
      break;
  }
}

void KeyboardEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  ProcessPointerEvent(event);
}

void KeyboardEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  ProcessPointerEvent(event);
}

void KeyboardEventHandler::ProcessPointerEvent(ui::LocatedEvent* event) {
  auto* controller = KeyboardUIController::Get();
  if (controller->IsEnabled() && controller->HandlePointerEvent(*event))
    event->SetHandled();
}

}  // namespace keyboard
