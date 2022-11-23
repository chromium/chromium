// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_SPECIFICS_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_SPECIFICS_H_

#include <string>

namespace ash {

// This structure encapsulates some specific parameters of signin flows that are
// not general enough to be put to UserContext.
struct SigninSpecifics {
  SigninSpecifics() {}

  // Specifies url that should be shown during Guest signin.
  std::string guest_mode_url;

  // Specifies if locale should be passed to guest mode url.
  bool guest_mode_url_append_locale = false;

  // Whether it is an automatic login.
  bool is_auto_login = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_SPECIFICS_H_
