// Copyright 2017 The Chromium Authors. All rights reserved.
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
extern const char kBrowserSigninNotAllowed[];
extern const char kInteractionRequired[];
extern const char kInvalidRedirect[];
extern const char kOffTheRecord[];
extern const char kPageLoadFailure[];
extern const char kCanceled[];

extern const int kCachedIssueAdviceTTLSeconds;
}  // namespace identity_constants

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CONSTANTS_H_
