// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_UI_FACTORY_H_
#define ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_UI_FACTORY_H_

#include <memory>

#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/test/test_window_delegate.h"

namespace aura {
class Window;
}

namespace ui {
class InputMethod;
}

namespace keyboard {

class TestKeyboardUIFactory : public KeyboardUIFactory {
 public:
  class TestKeyboardUI : public KeyboardUI {
   public:
    explicit TestKeyboardUI(ui::InputMethod* input_method);
    ~TestKeyboardUI() override;

    // Overridden from KeyboardUI:
    aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
    aura::Window* GetKeyboardWindow() const override;
    ui::GestureConsumer* GetGestureConsumer() const override;
    ui::InputMethod* GetInputMethod() override;
    void ReloadKeyboardIfNeeded() override {}

   private:
    std::unique_ptr<aura::Window> window_;
    aura::test::TestWindowDelegate delegate_;
    raw_ptr<ui::InputMethod> input_method_;
  };

  explicit TestKeyboardUIFactory(ui::InputMethod* input_method);

  TestKeyboardUIFactory(const TestKeyboardUIFactory&) = delete;
  TestKeyboardUIFactory& operator=(const TestKeyboardUIFactory&) = delete;

  ~TestKeyboardUIFactory() override;

  // Overridden from KeyboardUIFactory:
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;

 private:
  raw_ptr<ui::InputMethod, DanglingUntriaged> input_method_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_UI_FACTORY_H_
