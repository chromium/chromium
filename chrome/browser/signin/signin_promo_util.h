// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace signin {

// Enumeration of sign in promo types for the autofill bubble.
enum class SignInAutofillBubblePromoType { Passwords, Addresses, Payments };

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enumeration of possible versions of the autofill sign in promo bubble.
enum class SignInAutofillBubbleVersion {
  kNoPromo,
  kNoAccount,
  kWebSignedIn,
  kSignInPending
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Whether we should show the sync promo.
bool ShouldShowSyncPromo(Profile& profile);

// Whether we should show the sign in promo after data of the type
// |signin_promo_type| was saved.
bool ShouldShowSignInPromo(Profile& profile,
                           SignInAutofillBubblePromoType signin_promo_type);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns the version of the autofill bubble that should be displayed.
SignInAutofillBubbleVersion GetSignInPromoVersion(
    IdentityManager* identity_manager);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
