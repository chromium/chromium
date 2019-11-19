// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

struct AccountInfo;
class Browser;
class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

// Utility functions to gather status information from the various signed in
// services and construct messages suitable for showing in UI.
namespace signin_ui_util {

// The maximum number of times to show the welcome tutorial for an upgrade user.
const int kUpgradeWelcomeTutorialShowMax = 1;

// Returns the username of the authenticated user or an empty string if there is
// no authenticated user.
base::string16 GetAuthenticatedUsername(Profile* profile);

// Initializes signin-related preferences.
void InitializePrefsForProfile(Profile* profile);

// Shows a learn more page for signin errors.
void ShowSigninErrorLearnMorePage(Profile* profile);

// This function is used to enable sync for a given account:
// * This function does nothing if the user is already signed in to Chrome.
// * If |account| is empty, then it presents the Chrome sign-in page.
// * If token service has an invalid refreh token for account |account|,
//   then it presents the Chrome sign-in page with |account.emil| prefilled.
// * If token service has a valid refresh token for |account|, then it
//   enables sync for |account|.
// |is_default_promo_account| is true if |account| corresponds to the default
// account in the promo. It is ignored if |account| is empty.
void EnableSyncFromPromo(Browser* browser,
                         const AccountInfo& account,
                         signin_metrics::AccessPoint access_point,
                         bool is_default_promo_account);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns the list of all accounts that have a token. The default account in
// the Gaia cookies will be the first account in the list.
std::vector<AccountInfo> GetAccountsForDicePromos(Profile* profile);

#endif

// Returns the short user identity to display for |profile|. It is based on the
// current unconsented primary account (if exists).
// TODO(crbug.com/1012179): Move this logic into ProfileAttributesEntry once
// AvatarToolbarButton becomes an observer of ProfileAttributesStorage and thus
// ProfileAttributesEntry is up-to-date when AvatarToolbarButton needs it.
base::string16 GetShortProfileIdentityToDisplay(
    const ProfileAttributesEntry& profile_attributes_entry,
    Profile* profile);

// Returns the domain of the policy value of RestrictSigninToPattern. Returns
// an empty string if the policy is not set or can not be parsed. The parser
// only supports the policy value that matches [^@]+@[a-zA-Z0-9\-.]+(\\E)?\$?$.
// Also, the parser does not validate the policy value.
std::string GetAllowedDomain(std::string signin_pattern);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace internal {
// Same as |EnableSyncFromPromo| but with a callback that creates a
// DiceTurnSyncOnHelper so that it can be unit tested.
void EnableSyncFromPromo(
    Browser* browser,
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account,
    base::OnceCallback<
        void(Profile* profile,
             Browser* browser,
             signin_metrics::AccessPoint signin_access_point,
             signin_metrics::PromoAction signin_promo_action,
             signin_metrics::Reason signin_reason,
             const CoreAccountId& account_id,
             DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode)>
        create_dice_turn_sync_on_helper_callback);
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

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
