// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_metrics.h"

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

// The glue for Java-side implementation of SigninBridge.
class SigninBridge {
 public:
  // Opens a signin flow with the specified |access_point| for metrics.
  static void LaunchSigninActivity(ui::WindowAndroid* window,
                                   signin_metrics::AccessPoint access_point);

  // Opens the account management screen.
  static void OpenAccountManagementScreen(ui::WindowAndroid* window,
                                          signin::GAIAServiceType service_type);

  // Opens the account picker bottomsheet
  static void OpenAccountPickerBottomSheet(content::WebContents* web_contents,
                                           const std::string& continue_url);

  SigninBridge() = delete;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_
