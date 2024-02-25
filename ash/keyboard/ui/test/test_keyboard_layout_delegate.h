// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_
#define ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_

#include "ash/keyboard/ui/keyboard_layout_delegate.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace keyboard {

class TestKeyboardLayoutDelegate : public KeyboardLayoutDelegate {
 public:
  // |root_window| is the window that is always returned by the
  // KeyboardLayoutDelegate methods.
  explicit TestKeyboardLayoutDelegate(aura::Window* root_window);

  TestKeyboardLayoutDelegate(const TestKeyboardLayoutDelegate&) = delete;
  TestKeyboardLayoutDelegate& operator=(const TestKeyboardLayoutDelegate&) =
      delete;

  ~TestKeyboardLayoutDelegate() override = default;

  // Overridden from keyboard::KeyboardLayoutDelegate
  aura::Window* GetContainerForDefaultDisplay() override;
  aura::Window* GetContainerForDisplay(
      const display::Display& display) override;
  void TransferGestureEventToShelf(const ui::GestureEvent& e) override;

 private:
  raw_ptr<aura::Window, DanglingUntriaged> root_window_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_
