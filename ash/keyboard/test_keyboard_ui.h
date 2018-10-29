// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_TEST_KEYBOARD_UI_H_
#define ASH_KEYBOARD_TEST_KEYBOARD_UI_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/keyboard/keyboard_ui.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Stub implementation of keyboard::KeyboardUI
class TestKeyboardUI : public keyboard::KeyboardUI {
 public:
  TestKeyboardUI();
  ~TestKeyboardUI() override;

  // Overridden from KeyboardUI:
  aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
  aura::Window* GetKeyboardWindow() const override;

 private:
  // Overridden from keyboard::KeyboardUI:
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override;
  void InitInsets(const gfx::Rect& keyboard_bounds) override;
  void ResetInsets() override;

  aura::test::TestWindowDelegate delegate_;
  std::unique_ptr<aura::Window> keyboard_window_;
  DISALLOW_COPY_AND_ASSIGN(TestKeyboardUI);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_TEST_KEYBOARD_UI_H_
