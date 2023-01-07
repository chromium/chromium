// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_EVENT_HANDLER_H_
#define ASH_KEYBOARD_UI_KEYBOARD_EVENT_HANDLER_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace keyboard {

// EventHandler for the keyboard window, which intercepts events before they are
// processed by the keyboard window.
class KEYBOARD_EXPORT KeyboardEventHandler : public ui::EventHandler {
 public:
  KeyboardEventHandler() = default;

  KeyboardEventHandler(const KeyboardEventHandler&) = delete;
  KeyboardEventHandler& operator=(const KeyboardEventHandler&) = delete;

  ~KeyboardEventHandler() override = default;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  void ProcessPointerEvent(ui::LocatedEvent* event);
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_EVENT_HANDLER_H_
