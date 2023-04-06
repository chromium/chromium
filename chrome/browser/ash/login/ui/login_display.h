// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_

#include "chrome/browser/ash/login/signin_specifics.h"
#include "components/user_manager/user.h"

namespace ash {

// An abstract class that defines login UI implementation.
class LoginDisplay {
 public:
  LoginDisplay();

  LoginDisplay(const LoginDisplay&) = delete;
  LoginDisplay& operator=(const LoginDisplay&) = delete;

  virtual ~LoginDisplay();

  // Initializes login UI with the user pods based on list of known users and
  // guest, new user pods if those are enabled.
  virtual void Init(const user_manager::UserList& users) = 0;

  // Changes enabled state of the UI.
  virtual void SetUIEnabled(bool is_enabled) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
