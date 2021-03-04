// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace user_manager {
class User;
}

namespace chromeos {

// Ensure a set of "Login keyboard layouts" is used at UserAddingScreen.
class UserAddingScreenInputMethodsController
    : public UserAddingScreen::Observer {
 public:
  explicit UserAddingScreenInputMethodsController(UserAddingScreen* screen);
  ~UserAddingScreenInputMethodsController() override;

  // UserAddingScreen::Observer implementation:
  void OnBeforeUserAddingScreenStarted() override;
  void OnUserAddingFinished() override;

 private:
  UserAddingScreen* screen_;

  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state_;
  user_manager::User* active_user_on_show_;

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreenInputMethodsController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
