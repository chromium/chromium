// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_COOKIE_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_COOKIE_HELPER_H_

class Profile;

namespace cookie_helper {

// Adds new signin cookie (aka SAPISID cookie) directly to the profile's
// CookieManager. |profile| must not be nullptr and network must be already
// initialized.
void AddSigninCookie(Profile* profile);

// Removes all signin cookie (aka SAPISID cookie) directly from the profile's
// CookieManager. |profile| must not be nullptr and network must be already
// initialized.
void DeleteSigninCookies(Profile* profile);

}  // namespace cookie_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_COOKIE_HELPER_H_
