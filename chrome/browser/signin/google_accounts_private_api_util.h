// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_
#define CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_

namespace content {
class NavigationHandle;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

// Checks that the committed navigation has a Google Accounts Origin in order to
// expose the API.
// This function should be aligned with the implementation of
// `ShouldExposeGoogleAccountsJavascriptApi()` in
// chrome/renderer/google_accounts_private_api_util.h in order to add same
// safety check on both side of the Mojo bridge.
bool ShouldExposeGoogleAccountsPrivateApi(
    content::NavigationHandle* navigation_handle);

// Returns the Google Accounts (Gaia) Origin which the private API is allowed to
// be exposed to.
const url::Origin& GetAllowedGoogleAccountsOrigin();

#endif  // CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_
