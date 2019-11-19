// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_UTILS_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_UTILS_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/signin_header_helper.h"

namespace ui {
class WindowAndroid;
}

// The glue for Java-side implementation of SigninUtils.
class SigninUtils {
 public:
  // Opens the account management screen.
  static void OpenAccountManagementScreen(ui::WindowAndroid* profile,
                                          signin::GAIAServiceType service_type,
                                          const std::string& email);

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninUtils);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_UTILS_H_
