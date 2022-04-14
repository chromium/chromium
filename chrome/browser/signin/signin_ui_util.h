// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#endif

struct AccountInfo;
class Browser;
class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace account_manager {
class AccountManagerFacade;
}
#endif

// Utility functions to gather status information from the various signed in
// services and construct messages suitable for showing in UI.
namespace signin_ui_util {

// The maximum number of times to show the welcome tutorial for an upgrade user.
const int kUpgradeWelcomeTutorialShowMax = 1;

// Returns the username of the primary account or an empty string if there is
// no primary account or the account has not consented to browser sync.
std::u16string GetAuthenticatedUsername(Profile* profile);

// Initializes signin-related preferences.
void InitializePrefsForProfile(Profile* profile);

// Shows a learn more page for signin errors.
void ShowSigninErrorLearnMorePage(Profile* profile);

// Shows a reauth page/dialog to reauthanticate a primary account in error
// state.
void ShowReauthForPrimaryAccountWithAuthError(
    Browser* browser,
    signin_metrics::AccessPoint access_point);

// Delegates to an existing sign-in tab if one exists. If not, a new sign-in tab
// is created.
void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Displays sign-in UI to the user and shows the Sync confirmation if the user
// successfully adds an account and `enable_sync` is true.
// This will display the Chrome account picker first, if the system has
// available accounts. If the user chooses to add a new account or no existing
// accounts are available, this function will display OS's add account flow.
// `browser` might be null. In that case, this function will try to re-use an
// existing or open a new browser window for a `profile` if needed.
void ShowSigninPromptAndMaybeEnableSync(
    Browser* browser,
    Profile* profile,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace internal {
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
using CreateTurnSyncOnHelperCallback = base::OnceCallback<void(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode)>;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
using OnAccountAddedCallback = base::OnceCallback<void(const CoreAccountId&)>;

// Same as `ShowReauthForPrimaryAccountWithAuthError` but with a getter function
// for AccountManagerFacade so that it can be unit tested.
void ShowReauthForPrimaryAccountWithAuthErrorLacros(
    Browser* browser,
    signin_metrics::AccessPoint access_point,
    account_manager::AccountManagerFacade* account_manager_facade);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Same as `ShowExtensionSigninPrompt()` but with an additional parameters that
// can be injected for unit testing.
// `add_account_callback` encapsulates the logic to add a new account. It
// accepts a callback parameter that is invoked when the add account flow is
// complete.
// `create_turn_sync_on_helper_callback` creates a TurnSyncOnHelper when Sync
// needs to be enabled.
void ShowExtensionSigninPrompt(
    Profile* profile,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    account_manager::AccountManagerFacade* account_manager_facade,
    base::OnceCallback<void(OnAccountAddedCallback)> add_account_callback,
    CreateTurnSyncOnHelperCallback create_turn_sync_on_helper_callback,
#endif
    bool enable_sync,
    const std::string& email_hint);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Same as `ShowSigninPromptAndMaybeEnableSync()` but with an additional
// parameters that can be injected for unit testing.
// `add_account_callback` encapsulates the logic to add a new account. It
// accepts a callback parameter that is invoked when the add account flow is
// complete.
// `create_turn_sync_on_helper_callback` creates a TurnSyncOnHelper when Sync
// needs to be enabled.
void ShowSigninPromptAndMaybeEnableSync(
    Browser* browser,
    Profile* profile,
    base::OnceCallback<void(OnAccountAddedCallback)> add_account_callback,
    CreateTurnSyncOnHelperCallback create_turn_sync_on_helper_callback,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace internal

// This function is used to enable sync for a given account:
// * This function does nothing if the user is already signed in to Chrome.
// * If |account| is empty, then it presents the Chrome sign-in page.
// * If token service has an invalid refreh token for account |account|,
//   then it presents the Chrome sign-in page with |account.emil| prefilled.
// * If token service has a valid refresh token for |account|, then it
//   enables sync for |account|.
void EnableSyncFromSingleAccountPromo(Browser* browser,
                                      const AccountInfo& account,
                                      signin_metrics::AccessPoint access_point);

// This function is used to enable sync for a given account. It has the same
// behavior as |EnableSyncFromSingleAccountPromo()| except that it also logs
// some additional information if the action is started from a promo that
// supports selecting the account that may be used for sync.
//
// |is_default_promo_account| is true if |account| corresponds to the default
// account in the promo. It is ignored if |account| is empty.
void EnableSyncFromMultiAccountPromo(Browser* browser,
                                     const AccountInfo& account,
                                     signin_metrics::AccessPoint access_point,
                                     bool is_default_promo_account);

// Returns the list of all accounts that have a token. The unconsented primary
// account will be the first account in the list. If
// |restrict_to_accounts_eligible_for_sync| is true, removes the account that
// are not suitable for sync promos.
std::vector<AccountInfo> GetOrderedAccountsForDisplay(
    Profile* profile,
    bool restrict_to_accounts_eligible_for_sync);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns single account to use in Dice promos.
AccountInfo GetSingleAccountForDicePromos(Profile* profile);

#endif

// Returns the short user identity to display for |profile|. It is based on the
// current unconsented primary account (if exists).
// TODO(crbug.com/1012179): Move this logic into ProfileAttributesEntry once
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
namespace internal {
// Same as |EnableSyncFromPromo| but with a callback that creates a
// TurnSyncOnHelper so that it can be unit tested.
void EnableSyncFromPromo(
    Browser* browser,
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account,
    CreateTurnSyncOnHelperCallback create_turn_sync_on_helper_callback);
}  // namespace internal
#endif

// Returns whether Chrome should show the identity of the user (using a brief
// animation) on opening a new window. IdentityManager's refresh tokens must be
// loaded when this function gets called.
bool ShouldShowAnimatedIdentityOnOpeningWindow(
    const ProfileAttributesStorage& profile_attributes_storage,
    Profile* profile);

// Records that the animated identity was shown for the given profile. This is
// used for metrics and to decide whether/when the animation can be shown again.
void RecordAnimatedIdentityTriggered(Profile* profile);

// Records that the avatar icon was highlighted for the given profile. This is
// used for metrics.
void RecordAvatarIconHighlighted(Profile* profile);

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
