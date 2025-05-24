// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_

class Profile;

// These methods are used by ash-chrome.
namespace crosapi {
namespace browser_util {

// Checks for the given profile if the user is affiliated or belongs to the
// sign-in profile.
bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile);

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_
