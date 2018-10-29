// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_keyboard_test_base.h"

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/root_window_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

LoginKeyboardTestBase::LoginKeyboardTestBase() = default;

LoginKeyboardTestBase::~LoginKeyboardTestBase() = default;

void LoginKeyboardTestBase::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      keyboard::switches::kEnableVirtualKeyboard);
  AshTestBase::SetUp();

  login_controller_ = Shell::Get()->login_screen_controller();
  ASSERT_NE(nullptr, login_controller_);

  Shell::Get()->ash_keyboard_controller()->ActivateKeyboard();
}

void LoginKeyboardTestBase::TearDown() {
  Shell::Get()->ash_keyboard_controller()->DeactivateKeyboard();
  if (ash::LockScreen::HasInstance())
    ash::LockScreen::Get()->Destroy();
  AshTestBase::TearDown();
}

void LoginKeyboardTestBase::ShowKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(false);
  // Set keyboard height to half of the root window - this should overlap with
  // lock/login layout.
  if (keyboard_controller->GetKeyboardWindow()->bounds().height() == 0) {
    int height = Shell::GetPrimaryRootWindow()->bounds().height() / 2;
    keyboard_controller->GetKeyboardWindow()->SetBounds(
        keyboard::KeyboardBoundsFromRootBounds(
            Shell::GetPrimaryRootWindow()->bounds(), height));
    keyboard_controller->NotifyKeyboardWindowLoaded();
  }
  ASSERT_TRUE(keyboard_controller->IsKeyboardVisible());
}

void LoginKeyboardTestBase::HideKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->HideKeyboardByUser();
  ASSERT_FALSE(keyboard_controller->IsKeyboardVisible());
}

gfx::Rect LoginKeyboardTestBase::GetKeyboardBoundsInScreen() const {
  return keyboard::KeyboardController::Get()
      ->GetKeyboardWindow()
      ->GetBoundsInScreen();
}

void LoginKeyboardTestBase::ShowLockScreen() {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  // The lock screen can't be shown without a wallpaper.
  Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();

  base::Optional<bool> result;
  login_controller_->ShowLockScreen(base::BindOnce(
      [](base::Optional<bool>* result, bool did_show) { *result = did_show; },
      &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(*result, true);
}

void LoginKeyboardTestBase::ShowLoginScreen() {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // The login screen can't be shown without a wallpaper.
  Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();

  base::Optional<bool> result;
  login_controller_->ShowLoginScreen(base::BindOnce(
      [](base::Optional<bool>* result, bool did_show) { *result = did_show; },
      &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(*result, true);
}

void LoginKeyboardTestBase::LoadUsers(int count) {
  for (int i = 0; i < count; ++i) {
    std::string email =
        base::StrCat({"user", std::to_string(i), "@domain.com "});
    users_.push_back(CreateUser(email));
  }
  ash::LockScreen::Get()->data_dispatcher()->NotifyUsers(users_);
}

void LoginKeyboardTestBase::LoadPublicAccountUsers(int count) {
  for (int i = 0; i < count; ++i) {
    std::string email =
        base::StrCat({"publicuser", std::to_string(i), "@domain.com"});
    users_.push_back(CreatePublicAccountUser(email));
  }
  ash::LockScreen::Get()->data_dispatcher()->NotifyUsers(users_);
}

void LoginKeyboardTestBase::LoadUser(const std::string& email) {
  users_.push_back(CreateUser(email));
  ash::LockScreen::Get()->data_dispatcher()->NotifyUsers(users_);
}

}  // namespace ash
