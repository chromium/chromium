// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_PIN_KEYBOARD_INPUT_BRIDGE_H_
#define ASH_AUTH_VIEWS_PIN_KEYBOARD_INPUT_BRIDGE_H_

#include "ash/ash_export.h"
#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// A bridge between the pin keyboard and the auth input row views.
//
// The bridge is responsible for:
// - Listening to key presses on the pin keyboard.
// - Forwarding the key presses to the auth input row.
class ASH_EXPORT PinKeyboardInputBridge : public PinKeyboardView::Observer {
 public:
  PinKeyboardInputBridge(AuthInputRowView* input_row,
                         PinKeyboardView* pin_keyboard);

  PinKeyboardInputBridge(const PinKeyboardInputBridge&) = delete;
  PinKeyboardInputBridge& operator=(const PinKeyboardInputBridge&) = delete;

  ~PinKeyboardInputBridge() override;

  // PinKeyboardView::
  void OnDigitButtonPressed(int digit) override;
  void OnBackspacePressed() override;

 private:
  const raw_ptr<AuthInputRowView> input_row_;
  const raw_ptr<PinKeyboardView> pin_keyboard_;

  base::WeakPtrFactory<PinKeyboardInputBridge> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_PIN_KEYBOARD_INPUT_BRIDGE_H_
