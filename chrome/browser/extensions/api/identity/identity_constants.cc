// Copyright 2017 The Chromium Authors
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
const char kUserNonPrimary[] = "Only the primary user account is allowed";
const char kBrowserSigninNotAllowed[] = "The user turned off browser signin";
const char kInteractionRequired[] =
    "User interaction required. Try setting `abortOnLoadForNonInteractive` and "
    "`timeoutMsForNonInteractive` if multiple navigations are required, or if "
    "code is used for redirects in the authorization page after it's loaded.";
const char kGetAuthTokenInteractivityDeniedError[] =
    "User interaction blocked due to user inactivity.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
const char kOffTheRecord[] = "Identity API is disabled in incognito windows.";
const char kPageLoadFailure[] = "Authorization page could not be loaded.";
const char kPageLoadTimedOut[] = "Authorization page load timed out.";
const char kInvalidConsentResult[] = "Returned an invalid consent result.";
const char kCannotCreateWindow[] =
    "Couldn't create a browser window to display an authorization page.";
const char kInvalidURLScheme[] =
    "The auth url has an invalid scheme. Only http:// and https:// schemes are "
    "allowed.";
const char kBrowserContextShutDown[] = "The browser context has been shut down";

const int kCachedRemoteConsentTTLSeconds = 1;
}  // namespace identity_constants

}  // namespace extensions
