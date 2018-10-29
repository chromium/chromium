// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

class Profile;

namespace signin_util {

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Sign-out is allowed by default, but some Chrome profiles (e.g. for cloud-
// managed enterprise accounts) may wish to disallow user-initiated sign-out.
// Note that this exempts sign-outs that are not user-initiated (e.g. sign-out
// triggered when cloud policy no longer allows current email pattern). See
// ChromeSigninClient::PreSignOut().
void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed);

bool IsUserSignoutAllowedForProfile(Profile* profile);

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
