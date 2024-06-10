// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

class Profile;

namespace signin {

// Enumeration of sign in promo types for the autofill bubble.
enum class SignInAutofillBubblePromoType { Passwords, Addresses, Payments };

// Whether we should show the sync promo.
bool ShouldShowSyncPromo(Profile& profile);

// Whether we should show the sign in promo after data of the type
// |signin_promo_type| was saved.
bool ShouldShowSignInPromo(Profile& profile,
                           SignInAutofillBubblePromoType signin_promo_type);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
