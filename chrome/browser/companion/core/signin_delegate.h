// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

class Profile;

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

  // Returns whether the user needs to sign in.
  virtual bool AllowedSignin() = 0;

  // Starts a signin and sync flow.
  virtual void StartSigninFlow() = 0;

  // Creates the instance.
  static std::unique_ptr<SigninDelegate> Create(Profile* profile);
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_SIGNIN_DELEGATE_H_
