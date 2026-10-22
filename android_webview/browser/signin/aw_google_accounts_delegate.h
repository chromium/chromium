// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SIGNIN_AW_GOOGLE_ACCOUNTS_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_SIGNIN_AW_GOOGLE_ACCOUNTS_DELEGATE_H_

#include <string>

namespace signin {
struct ManageAccountsParams;
}

namespace android_webview {

// Used to intercept interaction with GAIA accounts.
class AwGoogleAccountsDelegate {
 public:
  virtual ~AwGoogleAccountsDelegate();

  // Called when a user wants to change the state of their GAIA account. This
  // could be a signin, signout, or any other action. See
  // signin::GAIAServiceType for all the possible actions.
  //
  // `is_outermost_main_frame` indicates whether the request happened in the
  // outermost frame. `is_navigation_request` is true if the request was a
  // navigation, whether this was in the outermost or an inner frame.
  virtual void OnGoogleAccountsRequest(
      const signin::ManageAccountsParams& params,
      bool is_outermost_main_frame,
      bool is_navigation_request) = 0;

  // The current GAIA ID the user is signed in with, or empty if the user is
  // signed out.
  virtual std::string GetGaiaId() = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SIGNIN_AW_GOOGLE_ACCOUNTS_DELEGATE_H_
