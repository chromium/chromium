// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_BRIDGE_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"

namespace ui {
class WindowAndroid;
}

// The glue for Java-side implementation of SigninBridge.
class SigninBridge {
 public:
  // Opens the account management screen.
  static void OpenAccountManagementScreen(ui::WindowAndroid* profile,
                                          signin::GAIAServiceType service_type);

  // Opens the account picker bottomsheet
  static void OpenAccountPickerBottomSheet(ui::WindowAndroid* window,
                                           const std::string& continue_url);

  SigninBridge() = delete;
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_BRIDGE_H_
