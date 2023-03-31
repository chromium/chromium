// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_

#include <set>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
class MouseEvent;
}  // namespace ui

namespace ash {

// Allows a client to receive keyboard and mouse events from Chrome OS
// in order to implement input handling in Select to Speak.
// Chrome OS is the source of truth of keyboard and mouse state; Select
// to Speak should defer to state sent by these methods in case of dropped
// events.
class ASH_PUBLIC_EXPORT SelectToSpeakEventHandlerDelegate {
 public:
  // Sends the currently pressed keys to the Select-to-Speak extension in
  // Chrome.
  virtual void DispatchKeysCurrentlyDown(
      const std::set<ui::KeyboardCode>& pressed_keys) = 0;

  // Sends a MouseEvent to the Select-to-Speak extension in Chrome.
  virtual void DispatchMouseEvent(const ui::MouseEvent& event) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_