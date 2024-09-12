// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_

namespace extensions {

namespace identity_constants {
extern const char kInvalidClientId[];
extern const char kInvalidScopes[];
extern const char kAuthFailure[];
extern const char kNoGrant[];
extern const char kUserRejected[];
extern const char kUserNotSignedIn[];
extern const char kUserNonPrimary[];
extern const char kBrowserSigninNotAllowed[];
extern const char kInteractionRequired[];
extern const char kGetAuthTokenInteractivityDeniedError[];
extern const char kInvalidRedirect[];
extern const char kOffTheRecord[];
extern const char kPageLoadFailure[];
extern const char kPageLoadTimedOut[];
extern const char kInvalidConsentResult[];
extern const char kCannotCreateWindow[];
extern const char kInvalidURLScheme[];
extern const char kBrowserContextShutDown[];

extern const int kCachedRemoteConsentTTLSeconds;
}  // namespace identity_constants

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_
