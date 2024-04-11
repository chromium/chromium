// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "components/signin/public/base/consent_level.h"

class Profile;

namespace signin {

// Enumeration of sign in promo types for the autofill bubble.
enum class SignInAutofillBubblePromoType { Passwords, Addresses, Payments };

// Returns true if the sync/sign in promo should be visible.
// |profile| is the profile of the tab the promo would be shown on.
// |promo_type| specifies whether the promo would be for sync or sign in.
bool ShouldShowPromo(Profile& profile, ConsentLevel promo_type);

// Whether we should show the sign in promo after data of the type
// |signin_promo_type| was saved.
bool ShouldShowSignInPromo(Profile& profile,
                           SignInAutofillBubblePromoType signin_promo_type);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
