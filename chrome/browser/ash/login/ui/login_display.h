// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_

#include "base/callback.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

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

    // Returns true if sign in is in progress.
    virtual bool IsSigninInProgress() const = 0;

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

  bool is_signin_completed() const { return is_signin_completed_; }
  void set_signin_completed(bool value) { is_signin_completed_ = value; }

 protected:
  // Login UI delegate (controller).
  Delegate* delegate_ = nullptr;

  // True if signin for user has completed.
  // TODO(nkostylev): Find a better place to store this state
  // in redesigned login stack.
  // Login stack (and this object) will be recreated for next user sign in.
  bool is_signin_completed_ = false;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::LoginDisplay;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_H_
