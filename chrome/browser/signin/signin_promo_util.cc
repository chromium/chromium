// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/signin_ui_util.h"

namespace {
constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;
}  // namespace
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !BUILDFLAG(IS_ANDROID)
namespace {

// Performs base checks for whether the sync/sign in promos should be shown.
// Needs additional checks depending on the type of the promo (see
// ShouldShowSyncPromo and ShouldShowSignInPromo). |profile| is the profile of
// the tab the promo would be shown on.
bool ShouldShowPromoCommon(Profile& profile) {
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

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);

  // No promo if the user is already syncing.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  // Verified the base checks. Depending on whether the promo should be for sync
  // or signin, additional checks are necessary.
  return true;
#endif
}

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace signin {

#if !BUILDFLAG(IS_ANDROID)
bool ShouldShowSyncPromo(Profile& profile) {
  // Don't show the promo if it does not pass the base checks.
  if (!ShouldShowPromoCommon(profile)) {
    return false;
  }

  syncer::SyncPrefs prefs(profile.GetPrefs());
  // Don't show if sync is not allowed to start or is running in local mode.
  if (!SyncServiceFactory::IsSyncAllowed(&profile) ||
      prefs.IsLocalSyncEnabled()) {
    return false;
  }

  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ShouldShowSignInPromo(Profile& profile,
                           SignInAutofillBubblePromoType promo_type) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Don't show the promo if it does not pass the base checks.
  if (!ShouldShowPromoCommon(profile)) {
    return false;
  }

  // Don't show the promo if the user is off-the-record.
  if (profile.IsOffTheRecord()) {
    return false;
  }

  SignInAutofillBubbleVersion sign_in_status =
      GetSignInPromoVersion(IdentityManagerFactory::GetForProfile(&profile));

  // Don't show the promo if the user is already signed in.
  if (sign_in_status == SignInAutofillBubbleVersion::kNoPromo) {
    return false;
  }

  // Always show the promo in sign in pending state.
  if (sign_in_status == SignInAutofillBubbleVersion::kSignInPending &&
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    return true;
  }

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);

  // Don't show the promo again after it was dismissed twice, regardless of
  // autofill bubble promo type.
  int dismiss_count =
      account.gaia.empty()
          ? profile.GetPrefs()->GetInteger(
                prefs::kAutofillSignInPromoDismissCountPerProfile)
          : SigninPrefs(*profile.GetPrefs())
                .GetAutofillSigninPromoDismissCount(account.gaia);

  if (dismiss_count >= kSigninPromoDismissedThreshold) {
    return false;
  }

  // Don't show the promo again if it has already been shown 5 times.
  int show_count = 0;
  switch (promo_type) {
    case SignInAutofillBubblePromoType::Addresses:
    case SignInAutofillBubblePromoType::Payments:
      break;
    case SignInAutofillBubblePromoType::Passwords:
      show_count =
          account.gaia.empty()
              ? profile.GetPrefs()->GetInteger(
                    prefs::kPasswordSignInPromoShownCountPerProfile)
              : SigninPrefs(*profile.GetPrefs())
                    .GetPasswordSigninPromoImpressionCount(account.gaia);
  }
  if (show_count >= kSigninPromoShownThreshold) {
    return false;
  }

  // Only show the promo if explicit browser signin is enabled.
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
SignInAutofillBubbleVersion GetSignInPromoVersion(
    IdentityManager* identity_manager) {
  if (identity_manager->HasPrimaryAccount(ConsentLevel::kSync)) {
    return SignInAutofillBubbleVersion::kNoPromo;
  }

  if (identity_manager->HasPrimaryAccount(ConsentLevel::kSignin) &&
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin))) {
    return SignInAutofillBubbleVersion::kSignInPending;
  }

  if (identity_manager->HasPrimaryAccount(ConsentLevel::kSignin)) {
    return SignInAutofillBubbleVersion::kNoPromo;
  }

  if (!signin_ui_util::GetSingleAccountForPromos(identity_manager).IsEmpty()) {
    return SignInAutofillBubbleVersion::kWebSignedIn;
  }

  return SignInAutofillBubbleVersion::kNoAccount;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
