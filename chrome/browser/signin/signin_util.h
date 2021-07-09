// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

class Profile;

namespace signin_util {

// This class calls ResetForceSigninForTesting when destroyed, so that
// ForcedSigning doesn't leak across tests.
class ScopedForceSigninSetterForTesting {
 public:
  explicit ScopedForceSigninSetterForTesting(bool enable);
  ~ScopedForceSigninSetterForTesting();
};

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing. Please use
// ScopedForceSigninSetterForTesting instead, if possible. If not, make sure
// ResetForceSigninForTesting is called before the test finishes.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Returns true if clearing the primary profile is allowed.
bool IsUserSignoutAllowedForProfile(Profile* profile);

// Sign-out is allowed by default, but some Chrome profiles (e.g. for cloud-
// managed enterprise accounts) may wish to disallow user-initiated sign-out.
// Note that this exempts sign-outs that are not user-initiated (e.g. sign-out
// triggered when cloud policy no longer allows current email pattern). See
// ChromeSigninClient::PreSignOut().
void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed);

// Updates the user sign-out state to |true| if is was never initialized.
// This should be called at the end of the flow to initialize a profile to
// ensure that the signout allowed flag is updated.
void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile);

// Ensures that the primary account for |profile| is allowed:
// * If profile does not have any primary account, then this is a no-op.
// * If |IsUserSignoutAllowedForProfile| is allowed and the primary account
//   is no longer allowed, then this clears the primary account.
// * If |IsUserSignoutAllowedForProfile| is not allowed and the primary account
//   is not longer allowed, then this removes the profile.
void EnsurePrimaryAccountAllowedForProfile(Profile* profile);

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
