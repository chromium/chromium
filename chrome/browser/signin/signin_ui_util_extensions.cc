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
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/public/base/signin_metrics.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

signin_ui_util::SigninUiDelegate* g_signin_ui_delegate_for_extensions_testing =
    nullptr;

signin_ui_util::SigninUiDelegate* GetSigninUiDelegateForExtensions() {
  if (g_signin_ui_delegate_for_extensions_testing) {
    return g_signin_ui_delegate_for_extensions_testing;
  }
  // TODO(crbug.com/403867715): Add the Android implementation here.
  static signin_ui_util::SigninUiDelegateImplDice delegate;
  return &delegate;
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  // There is no sign-in flow for guest or system profile.
  if (profile->IsGuestSession() || profile->IsSystemProfile()) {
    return;
  }
  // Locked profile should be unlocked with UserManager only.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return;
  }

  // This may be called in incognito. Redirect to the original profile.
  profile = profile->GetOriginalProfile();

  if (email_hint.empty()) {
    // Add a new account.
    GetSigninUiDelegateForExtensions()->ShowSigninUI(
        profile, enable_sync, signin_metrics::AccessPoint::kExtensions,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    return;
  }

  // Re-authenticate an existing account.
  GetSigninUiDelegateForExtensions()->ShowReauthUI(
      profile, email_hint, enable_sync,
      signin_metrics::AccessPoint::kExtensions,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
#else
  NOTIMPLEMENTED() << "Not yet implemented on Android";
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
base::AutoReset<signin_ui_util::SigninUiDelegate*>
SetSigninUiDelegateForExtensionsTesting(  // IN-TEST
    signin_ui_util::SigninUiDelegate* delegate) {
  return base::AutoReset<signin_ui_util::SigninUiDelegate*>(
      &g_signin_ui_delegate_for_extensions_testing, delegate);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
