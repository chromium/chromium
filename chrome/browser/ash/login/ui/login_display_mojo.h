// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_

#include "chrome/browser/ash/login/ui/login_display.h"

namespace ash {

// Interface used by UI-agnostic code to send messages to views-based login
// screen.
// TODO(estade): rename to LoginDisplayAsh.
class LoginDisplayMojo : public LoginDisplay {
 public:
  LoginDisplayMojo();

  LoginDisplayMojo(const LoginDisplayMojo&) = delete;
  LoginDisplayMojo& operator=(const LoginDisplayMojo&) = delete;

  ~LoginDisplayMojo() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
