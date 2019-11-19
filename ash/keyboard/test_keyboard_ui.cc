// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/test_keyboard_ui.h"

#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/shell.h"
#include "ash/window_factory.h"
#include "ash/wm/window_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/mock_input_method.h"

namespace ash {

TestKeyboardUI::TestKeyboardUI() = default;

TestKeyboardUI::~TestKeyboardUI() = default;

aura::Window* TestKeyboardUI::LoadKeyboardWindow(LoadCallback callback) {
  DCHECK(!keyboard_window_);
  keyboard_window_ = window_factory::NewWindow(&delegate_);
  keyboard_window_->Init(ui::LAYER_NOT_DRAWN);

  // Set a default size for the keyboard.
  display::Screen* screen = display::Screen::GetScreen();
  keyboard_window_->SetBounds(keyboard::KeyboardBoundsFromRootBounds(
      screen->GetPrimaryDisplay().bounds()));

  // Simulate an asynchronous load.
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));

  return keyboard_window_.get();
}

aura::Window* TestKeyboardUI::GetKeyboardWindow() const {
  return keyboard_window_.get();
}

ui::InputMethod* TestKeyboardUI::GetInputMethod() {
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* root_window = active_window ? active_window->GetRootWindow()
                                            : Shell::GetPrimaryRootWindow();
  return root_window->GetHost()->GetInputMethod();
}

void TestKeyboardUI::ReloadKeyboardIfNeeded() {}

TestKeyboardUIFactory::TestKeyboardUIFactory() = default;
TestKeyboardUIFactory::~TestKeyboardUIFactory() = default;

std::unique_ptr<keyboard::KeyboardUI>
TestKeyboardUIFactory::CreateKeyboardUI() {
  return std::make_unique<TestKeyboardUI>();
}

}  // namespace ash
