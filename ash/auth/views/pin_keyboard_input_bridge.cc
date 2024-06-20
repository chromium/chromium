// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_keyboard_input_bridge.h"

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

PinKeyboardInputBridge::PinKeyboardInputBridge(AuthInputRowView* input_row,
                                               PinKeyboardView* pin_keyboard)
    : input_row_(input_row), pin_keyboard_(pin_keyboard) {
  pin_keyboard_->AddObserver(this);
}

PinKeyboardInputBridge::~PinKeyboardInputBridge() {
  pin_keyboard_->RemoveObserver(this);
}

void PinKeyboardInputBridge::OnDigitButtonPressed(int digit) {
  input_row_->InsertDigit(digit);
}

void PinKeyboardInputBridge::OnBackspacePressed() {
  input_row_->Backspace();
}

}  // namespace ash
