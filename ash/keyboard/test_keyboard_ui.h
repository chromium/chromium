// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_TEST_KEYBOARD_UI_H_
#define ASH_KEYBOARD_TEST_KEYBOARD_UI_H_

#include <memory>

#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ui/aura/test/test_window_delegate.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Stub implementation of keyboard::KeyboardUI
class TestKeyboardUI : public keyboard::KeyboardUI {
 public:
  TestKeyboardUI();

  TestKeyboardUI(const TestKeyboardUI&) = delete;
  TestKeyboardUI& operator=(const TestKeyboardUI&) = delete;

  ~TestKeyboardUI() override;

  // Overridden from KeyboardUI:
  aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
  aura::Window* GetKeyboardWindow() const override;
  ui::GestureConsumer* GetGestureConsumer() const override;

 private:
  // Overridden from keyboard::KeyboardUI:
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override;

  aura::test::TestWindowDelegate delegate_;
  std::unique_ptr<aura::Window> keyboard_window_;
};

class TestKeyboardUIFactory : public keyboard::KeyboardUIFactory {
 public:
  TestKeyboardUIFactory();

  TestKeyboardUIFactory(const TestKeyboardUIFactory&) = delete;
  TestKeyboardUIFactory& operator=(const TestKeyboardUIFactory&) = delete;

  ~TestKeyboardUIFactory() override;

 private:
  // keyboard::KeyboardUIFactory:
  std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() override;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_TEST_KEYBOARD_UI_H_
