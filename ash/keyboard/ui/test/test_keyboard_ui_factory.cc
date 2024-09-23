// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/test/test_keyboard_ui_factory.h"

#include <utility>

#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace keyboard {

TestKeyboardUIFactory::TestKeyboardUIFactory(ui::InputMethod* input_method)
    : input_method_(input_method) {}

TestKeyboardUIFactory::~TestKeyboardUIFactory() = default;

std::unique_ptr<KeyboardUI> TestKeyboardUIFactory::CreateKeyboardUI() {
  return std::make_unique<TestKeyboardUI>(input_method_);
}

// TestKeyboardUIFactory::TestKeyboardUI:

TestKeyboardUIFactory::TestKeyboardUI::TestKeyboardUI(
    ui::InputMethod* input_method)
    : input_method_(input_method) {}

TestKeyboardUIFactory::TestKeyboardUI::~TestKeyboardUI() {
  // Destroy the window before the delegate.
  window_.reset();
}

aura::Window* TestKeyboardUIFactory::TestKeyboardUI::LoadKeyboardWindow(
    LoadCallback callback) {
  DCHECK(!window_);
  window_ = std::make_unique<aura::Window>(&delegate_);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->set_owned_by_parent(false);

  // Set a default size for the keyboard.
  display::Screen* screen = display::Screen::GetScreen();
  window_->SetBounds(
      test::KeyboardBoundsFromRootBounds(screen->GetPrimaryDisplay().bounds()));

  // Simulate an asynchronous load.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));

  return window_.get();
}

aura::Window* TestKeyboardUIFactory::TestKeyboardUI::GetKeyboardWindow() const {
  return window_.get();
}

ui::GestureConsumer* TestKeyboardUIFactory::TestKeyboardUI::GetGestureConsumer()
    const {
  return GetKeyboardWindow();
}

ui::InputMethod* TestKeyboardUIFactory::TestKeyboardUI::GetInputMethod() {
  return input_method_;
}

}  // namespace keyboard
