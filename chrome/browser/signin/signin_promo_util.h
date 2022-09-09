// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

class Profile;

namespace signin {

// Returns true if the sign in promo should be visible.
// |profile| is the profile of the tab the promo would be shown on.
bool ShouldShowPromo(Profile* profile);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
