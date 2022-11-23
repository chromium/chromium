// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_

#include "chrome/browser/ash/login/signin_specifics.h"
#include "components/user_manager/user.h"

namespace ash {

class UserContext;

// TODO(nkostylev): Extract interface, create a BaseLoginDisplay class.
// An abstract class that defines login UI implementation.
class LoginDisplay {
 public:
  class Delegate {
   public:
    // Sign in using `username` and `password` specified.
    // Used for known users only.
    virtual void Login(const UserContext& user_context,
                       const SigninSpecifics& specifics) = 0;

    // Called when the user requests kiosk enable screen.
    virtual void OnStartKioskEnableScreen() = 0;

    // Restarts the auto-login timer if it is running.
    virtual void ResetAutoLoginTimer() = 0;

   protected:
    virtual ~Delegate();
  };

  LoginDisplay();

  LoginDisplay(const LoginDisplay&) = delete;
  LoginDisplay& operator=(const LoginDisplay&) = delete;

  virtual ~LoginDisplay();

  // Initializes login UI with the user pods based on list of known users and
  // guest, new user pods if those are enabled.
  virtual void Init(const user_manager::UserList& users, bool show_guest) = 0;

  // Changes enabled state of the UI.
  virtual void SetUIEnabled(bool is_enabled) = 0;

  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  // Login UI delegate (controller).
  Delegate* delegate_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
