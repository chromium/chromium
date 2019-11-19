// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_keyboard_test_base.h"

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/root_window_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"

namespace ash {

LoginKeyboardTestBase::LoginKeyboardTestBase() = default;

LoginKeyboardTestBase::~LoginKeyboardTestBase() = default;

void LoginKeyboardTestBase::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      keyboard::switches::kEnableVirtualKeyboard);
  LoginTestBase::SetUp();
}

void LoginKeyboardTestBase::ShowKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);
  // Set keyboard height to half of the root window - this should overlap with
  // lock/login layout.
  int height = Shell::GetPrimaryRootWindow()->bounds().height() / 2;
  keyboard_controller->GetKeyboardWindow()->SetBounds(
      keyboard::KeyboardBoundsFromRootBounds(
          Shell::GetPrimaryRootWindow()->bounds(), height));
  ASSERT_TRUE(keyboard::WaitUntilShown());
}

void LoginKeyboardTestBase::HideKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->HideKeyboardByUser();
  ASSERT_FALSE(keyboard_controller->IsKeyboardVisible());
}

gfx::Rect LoginKeyboardTestBase::GetKeyboardBoundsInScreen() const {
  return keyboard::KeyboardUIController::Get()
      ->GetKeyboardWindow()
      ->GetBoundsInScreen();
}

}  // namespace ash
