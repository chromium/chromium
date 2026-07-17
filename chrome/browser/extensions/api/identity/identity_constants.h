// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::identity_constants {
inline constexpr char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
inline constexpr char kInvalidScopes[] = "Invalid OAuth2 scopes.";
inline constexpr char kAuthFailure[] = "OAuth2 request failed: ";
inline constexpr char kNoGrant[] = "OAuth2 not granted or revoked.";
inline constexpr char kUserRejected[] = "The user did not approve access.";
inline constexpr char kUserNotSignedIn[] = "The user is not signed in.";
inline constexpr char kUserNonPrimary[] =
    "Only the primary user account is allowed";
inline constexpr char kBrowserSigninNotAllowed[] =
    "The user turned off browser signin";
inline constexpr char kInteractionRequired[] =
    "User interaction required. Try setting `abortOnLoadForNonInteractive` and "
    "`timeoutMsForNonInteractive` if multiple navigations are required, or if "
    "code is used for redirects in the authorization page after it's loaded.";
inline constexpr char kGetAuthTokenInteractivityDeniedError[] =
    "User interaction blocked due to user inactivity.";
inline constexpr char kInvalidRedirect[] = "Did not redirect to the right URL.";
inline constexpr char kOffTheRecord[] =
    "Identity API is disabled in incognito windows.";
inline constexpr char kPageLoadFailure[] =
    "Authorization page could not be loaded.";
inline constexpr char kPageLoadTimedOut[] =
    "Authorization page load timed out.";
inline constexpr char kInvalidConsentResult[] =
    "Returned an invalid consent result.";
inline constexpr char kCannotSetRemoteConsentResolutionCookies[] =
    "Couldn't set up remote consent resolution cookies to display an "
    "authorization page";
inline constexpr char kCannotCreateWindow[] =
    "Couldn't create a browser window to display an authorization page.";
inline constexpr char kInvalidURLScheme[] =
    "The auth url has an invalid scheme. Only http:// and https:// schemes are "
    "allowed.";
inline constexpr char kBrowserContextShutDown[] =
    "The browser context has been shut down";
inline constexpr char kWebAuthFlowInProgress[] =
    "Only one web auth flow is allowed at a time.";

inline constexpr int kCachedRemoteConsentTTLSeconds = 1;
}  // namespace extensions::identity_constants

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_
