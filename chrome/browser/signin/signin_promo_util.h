// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

class Profile;

namespace signin_metrics {
enum class AccessPoint;
}

namespace autofill {
class AutofillProfile;
}

namespace signin {

// Whether we should show the sync promo.
bool ShouldShowSyncPromo(Profile& profile);

// Whether we should show the sign in promo after a password was saved.
bool ShouldShowPasswordSignInPromo(Profile& profile);

// Whether we should show the sign in promo after `address` was saved.
bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address);

// Returns whether `access_point` has an equivalent autofill signin promo.
bool IsAutofillSigninPromo(signin_metrics::AccessPoint access_point);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
