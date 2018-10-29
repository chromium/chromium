// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_constants.h"

namespace extensions {

namespace identity_constants {
const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kAuthFailure[] = "OAuth2 request failed: ";
const char kNoGrant[] = "OAuth2 not granted or revoked.";
const char kUserRejected[] = "The user did not approve access.";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kBrowserSigninNotAllowed[] = "The user turned off browser signin";
const char kInteractionRequired[] = "User interaction required.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
const char kOffTheRecord[] = "Identity API is disabled in incognito windows.";
const char kPageLoadFailure[] = "Authorization page could not be loaded.";
const char kCanceled[] = "canceled";

const int kCachedIssueAdviceTTLSeconds = 1;
}  // namespace identity_constants

}  // namespace extensions
