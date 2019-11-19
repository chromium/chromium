// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ui {
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace ash {

// Allows a client to implement Select-to-Speak.
// TODO(katie): Consider combining DispatchMouseEvent and DispatchKeyEvent
// into a single DispatchEvent function.
class ASH_PUBLIC_EXPORT SelectToSpeakEventHandlerDelegate {
 public:
  // Sends a KeyEvent to the Select-to-Speak extension in Chrome.
  virtual void DispatchKeyEvent(const ui::KeyEvent& event) = 0;

  // Sends a MouseEvent to the Select-to-Speak extension in Chrome.
  virtual void DispatchMouseEvent(const ui::MouseEvent& event) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_