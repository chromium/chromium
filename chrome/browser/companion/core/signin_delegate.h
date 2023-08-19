// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_

#include "url/gurl.h"

namespace companion {

// Central class to handle user actions on various promos displayed in the
// search companion.
class SigninDelegate {
 public:
  SigninDelegate() = default;
  virtual ~SigninDelegate() = default;

  // Disallow copy/assign.
  SigninDelegate(const SigninDelegate&) = delete;
  SigninDelegate& operator=(const SigninDelegate&) = delete;

  // Returns whether the user is allowed to sign in.
  virtual bool AllowedSignin() = 0;

  // Returns whether the user is already signed in.
  virtual bool IsSignedIn() = 0;

  // Starts a signin and sync flow.
  virtual void StartSigninFlow() = 0;

  // Enable the setting for make searches and browsing better.
  virtual void EnableMsbb(bool enable_msbb) = 0;

  // Opens URL in the browser.
  virtual void OpenUrlInBrowser(const GURL& url, bool use_new_tab) = 0;

  // Returns whether region search IPH should be shown.
  virtual bool ShouldShowRegionSearchIPH() = 0;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_
