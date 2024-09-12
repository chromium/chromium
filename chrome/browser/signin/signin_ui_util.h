// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

struct AccountInfo;
struct CoreAccountInfo;
class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

namespace signin {
class IdentityManager;
}

// Utility functions to gather status information from the various signed in
// services and construct messages suitable for showing in UI.
namespace signin_ui_util {
class SigninUiDelegate;

// Returns the username of the primary account or an empty string if there is
// no primary account or the account has not consented to browser sync.
std::u16string GetAuthenticatedUsername(Profile* profile);

// Shows a learn more page for signin errors.
void ShowSigninErrorLearnMorePage(Profile* profile);

// Shows a reauth page/dialog to reauthanticate a primary account in error
// state.
void ShowReauthForPrimaryAccountWithAuthError(
    Profile* profile,
    signin_metrics::AccessPoint access_point);

// Shows a reauth page/dialog to reauthanticate an account.
void ShowReauthForAccount(Profile* profile,
                          const std::string& email,
                          signin_metrics::AccessPoint access_point);

// Delegates to an existing sign-in tab if one exists. If not, a new sign-in tab
// is created.
void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint);

// This function is used to sign-in the user into Chrome without offering sync.
// This function does nothing if the user is already signed in to Chrome.
void ShowSigninPromptFromPromo(Profile* profile,
                               signin_metrics::AccessPoint access_point);

// This function is used to sign in a given account:
// * This function does nothing if the user is already signed in to Chrome.
// * If |account| is empty, then it presents the Chrome sign-in page.
// * If token service has an invalid refresh token for account |account|,
//   then it presents the Chrome sign-in page with |account.email| prefilled.
// * If token service has a valid refresh token for |account|, then it
//   signs in the |account|.
void SignInFromSingleAccountPromo(Profile* profile,
                                  const CoreAccountInfo& account,
                                  signin_metrics::AccessPoint access_point);

// This function is used to enable sync for a given account:
// * This function does nothing if the user is already signed in to Chrome.
// * If |account| is empty, then it presents the Chrome sign-in page.
// * If token service has an invalid refresh token for account |account|,
//   then it presents the Chrome sign-in page with |account.email| prefilled.
// * If token service has a valid refresh token for |account|, then it
//   enables sync for |account|.
void EnableSyncFromSingleAccountPromo(Profile* profile,
                                      const CoreAccountInfo& account,
                                      signin_metrics::AccessPoint access_point);

// This function is used to enable sync for a given account. It has the same
// behavior as |EnableSyncFromSingleAccountPromo()| except that it also logs
// some additional information if the action is started from a promo that
// supports selecting the account that may be used for sync.
//
// |is_default_promo_account| is true if |account| corresponds to the default
// account in the promo. It is ignored if |account| is empty.
void EnableSyncFromMultiAccountPromo(Profile* profile,
                                     const CoreAccountInfo& account,
                                     signin_metrics::AccessPoint access_point,
                                     bool is_default_promo_account);

// Returns the list of all accounts that have a token. The unconsented primary
// account will be the first account in the list. If
// |restrict_to_accounts_eligible_for_sync| is true, removes the account that
// are not suitable for sync promos.
std::vector<AccountInfo> GetOrderedAccountsForDisplay(
    signin::IdentityManager* identity_manager,
    bool restrict_to_accounts_eligible_for_sync);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns single account to use in promos.
AccountInfo GetSingleAccountForPromos(
    signin::IdentityManager* identity_manager);

#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns an existing re-usable Dice signin tab with the given access point.
content::WebContents* GetSignInTabWithAccessPoint(
    BrowserWindowInterface* browser_window_interface,
    signin_metrics::AccessPoint access_point);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Returns the short user identity to display for |profile|. It is based on the
// current unconsented primary account (if exists).
// TODO(crbug.com/40102223): Move this logic into ProfileAttributesEntry once
// AvatarToolbarButton becomes an observer of ProfileAttributesStorage and thus
// ProfileAttributesEntry is up-to-date when AvatarToolbarButton needs it.
std::u16string GetShortProfileIdentityToDisplay(
    const ProfileAttributesEntry& profile_attributes_entry,
    Profile* profile);

// Returns the domain of the policy value of RestrictSigninToPattern. Returns
// an empty string if the policy is not set or can not be parsed. The parser
// only supports the policy value that matches [^@]+@[a-zA-Z0-9\-.]+(\\E)?\$?$.
// Also, the parser does not validate the policy value.
std::string GetAllowedDomain(std::string signin_pattern);

// Returns whether Chrome should show the identity of the user (using a brief
// animation) on opening a new window. IdentityManager's refresh tokens must be
// loaded when this function gets called.
bool ShouldShowAnimatedIdentityOnOpeningWindow(
    const ProfileAttributesStorage& profile_attributes_storage,
    Profile* profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
base::AutoReset<SigninUiDelegate*> SetSigninUiDelegateForTesting(
    SigninUiDelegate* delegate);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

// Records that the animated identity was shown for the given profile. This is
// used for metrics and to decide whether/when the animation can be shown again.
void RecordAnimatedIdentityTriggered(Profile* profile);

// Called when the ProfileMenuView is opened. Used for metrics.
void RecordProfileMenuViewShown(Profile* profile);

// Called when a button/link in the profile menu was clicked.
void RecordProfileMenuClick(Profile* profile);

// Records the result of a re-auth challenge to finish a transaction (like
// unlocking the account store for passwords).
void RecordTransactionalReauthResult(
    signin_metrics::ReauthAccessPoint access_point,
    signin::ReauthResult result);

// Records user action performed in a transactional reauth dialog/tab.
void RecordTransactionalReauthUserAction(
    signin_metrics::ReauthAccessPoint access_point,
    SigninReauthViewController::UserAction user_action);

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
