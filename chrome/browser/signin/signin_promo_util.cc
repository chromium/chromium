// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "net/base/network_change_notifier.h"

namespace signin {

bool ShouldShowPromo(Profile& profile, ConsentLevel promo_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // There's no need to show the sign in promo on cros since cros users are
  // already logged in.
  return false;
#else

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Consider original profile even if an off-the-record profile was
  // passed to this method as sign-in state is only defined for the
  // primary profile.
  Profile* original_profile = profile.GetOriginalProfile();

  // Don't show for supervised child profiles.
  if (original_profile->IsChild())
    return false;

  // Don't show if sign in is not allowed.
  if (!original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed))
    return false;

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);

  // No promo if the user is already syncing.
  if (identity_manager->HasPrimaryAccount(ConsentLevel::kSync)) {
    return false;
  }

  // Sync Promos are always shown when the user is not syncing.
  if (promo_type == ConsentLevel::kSync) {
    return true;
  }

  // Signin promo is shown if the user is not signed in or needs to reauth.
  return !identity_manager->HasPrimaryAccount(ConsentLevel::kSignin) ||
         identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
             identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin));
#endif
}

bool ShouldShowSignInPromo(Profile& profile,
                           SignInAutofillBubblePromoType signin_promo_type) {
  return ShouldShowPromo(profile, ConsentLevel::kSignin) &&
         switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
             switches::ExplicitBrowserSigninPhase::kFull);
}

}  // namespace signin
