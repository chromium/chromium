// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_KEYBOARD_TEST_BASE_H_
#define ASH_LOGIN_UI_LOGIN_KEYBOARD_TEST_BASE_H_

#include <memory>

#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class LoginScreenController;

// Base test fixture for testing the views-based login and lock screens with
// virtual keyboard.
class LoginKeyboardTestBase : public AshTestBase {
 public:
  LoginKeyboardTestBase();
  ~LoginKeyboardTestBase() override;

  // Shows keyboard with the height of half of the window height. Asserts that
  // keyboard is visible. To stop execution of the test on failed assertion
  // use ASSERT_NO_FATAL_FAILURE macro.
  void ShowKeyboard();

  // Hides keyboard. Asserts that keyboard is not visible. To stop execution of
  // the test on failed assertion use ASSERT_NO_FATAL_FAILURE macro.
  void HideKeyboard();

  // Returns bounds of the keyboard in screen coordinate space.
  gfx::Rect GetKeyboardBoundsInScreen() const;

  // Shows lock screen. Asserts that lock screen is shown. To stop execution of
  // the test on failed assertion use ASSERT_NO_FATAL_FAILURE macro.
  void ShowLockScreen();

  // Shows login screen. Asserts that login screen is shown. To stop execution
  // of the test on failed assertion use ASSERT_NO_FATAL_FAILURE macro.
  void ShowLoginScreen();

  // Loads the number of test users specified by |count|.
  void LoadUsers(int count);

  // Loads the number of test public account users specified by |count|.
  void LoadPublicAccountUsers(int count);

  // Loads user with the specified |email|.
  void LoadUser(const std::string& email);

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  LoginScreenController* login_controller_ = nullptr;
  std::vector<mojom::LoginUserInfoPtr> users_;

  DISALLOW_COPY_AND_ASSIGN(LoginKeyboardTestBase);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_KEYBOARD_TEST_BASE_H_
