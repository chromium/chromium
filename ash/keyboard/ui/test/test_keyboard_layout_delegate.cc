// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/test/test_keyboard_layout_delegate.h"

#include "ui/aura/window.h"

namespace keyboard {

TestKeyboardLayoutDelegate::TestKeyboardLayoutDelegate(
    aura::Window* root_window)
    : root_window_(root_window) {}

aura::Window* TestKeyboardLayoutDelegate::GetContainerForDefaultDisplay() {
  return root_window_;
}

aura::Window* TestKeyboardLayoutDelegate::GetContainerForDisplay(
    const display::Display& display) {
  return root_window_;
}

void TestKeyboardLayoutDelegate::TransferGestureEventToShelf(
    const ui::GestureEvent& e) {}

}  // namespace keyboard
