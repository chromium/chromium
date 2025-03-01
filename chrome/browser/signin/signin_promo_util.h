// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "components/signin/public/base/signin_buildflags.h"

class Profile;

namespace signin_metrics {
enum class AccessPoint;
}

namespace autofill {
class AutofillProfile;
}

namespace signin {

enum class SignInPromoType;

// Whether we should show the sync promo.
bool ShouldShowSyncPromo(Profile& profile);

// Whether we should show the sign in promo after a password was saved.
bool ShouldShowPasswordSignInPromo(Profile& profile);

// Whether we should show the sign in promo after `address` was saved.
bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address);

// Whether we should show the sign in promo after a bookmark was saved.
bool ShouldShowBookmarkSignInPromo(Profile& profile);

// Returns whether `access_point` has an equivalent autofill signin promo.
bool IsAutofillSigninPromo(signin_metrics::AccessPoint access_point);

// Returns whether `access_point` has an equivalent signin promo.
bool IsSignInPromo(signin_metrics::AccessPoint access_point);

SignInPromoType GetSignInPromoTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Records that the sign in promo was shown, either for the account used for the
// promo, or for the profile if there is no account available.
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
