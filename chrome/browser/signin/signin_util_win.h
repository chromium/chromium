// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_

#include <memory>

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

class Profile;

namespace signin_util {

// Attempt to sign in with a credentials from a system installed credential
// provider if available.
void SigninWithCredentialProviderIfPossible(Profile* profile);

// Attempt to reauthenticate with a credentials from a system installed
// credential provider if available.  If a new authentication token was
// installed returns true.
bool ReauthWithCredentialProviderIfPossible(Profile* profile);

// Sets the TurnSyncOnHelper delegate for browser tests.
void SetTurnSyncOnHelperDelegateForTesting(
    std::unique_ptr<TurnSyncOnHelper::Delegate> delegate);

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_
