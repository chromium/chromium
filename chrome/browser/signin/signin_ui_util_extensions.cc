// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util_extensions.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/signin_ui_delegate_impl_android.h"
#endif

namespace {

signin_ui_util::SigninUiDelegate* g_signin_ui_delegate_for_extensions_testing =
    nullptr;

signin_ui_util::SigninUiDelegate* GetSigninUiDelegateForExtensions() {
  if (g_signin_ui_delegate_for_extensions_testing) {
    return g_signin_ui_delegate_for_extensions_testing;
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  static signin_ui_util::SigninUiDelegateImplDice delegate;
  return &delegate;
#else
  static signin_ui_util::SigninUiDelegateImplAndroid delegate;
  return &delegate;
#endif
}

bool CanShowSigninPrompt(Profile* profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // There is no sign-in flow for guest or system profile.
  if (profile->IsGuestSession() || profile->IsSystemProfile()) {
    return false;
  }
  // Locked profile should be unlocked with UserManager only.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  return true;
}

bool ShouldShowReauthFlowForEmail(Profile* profile, const std::string& email) {
  if (email.empty()) {
    return false;
  }

  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByEmailAddress(email);
  if (account_info.IsEmpty()) {
    return false;
  }
  return identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
      account_info.account_id);
}

}  // namespace

void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint,
                               const std::string& extension_name) {
  if (!CanShowSigninPrompt(profile)) {
    return;
  }

  // This may be called in incognito. Redirect to the original profile.
  profile = profile->GetOriginalProfile();

  if (ShouldShowReauthFlowForEmail(profile, email_hint)) {
    // Re-authenticate an existing account.
    GetSigninUiDelegateForExtensions()->ShowReauthUI(
        profile, email_hint, enable_sync,
        signin_metrics::AccessPoint::kExtensions,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    return;
  }

  // Add a new account.
  GetSigninUiDelegateForExtensions()->ShowSigninUI(
      profile, enable_sync, signin_metrics::AccessPoint::kExtensions,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      extension_name);
}

base::AutoReset<signin_ui_util::SigninUiDelegate*>
SetSigninUiDelegateForExtensionsTesting(  // IN-TEST
    signin_ui_util::SigninUiDelegate* delegate) {
  return base::AutoReset<signin_ui_util::SigninUiDelegate*>(
      &g_signin_ui_delegate_for_extensions_testing, delegate);
}
