// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/user_selectable_type.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/interaction/element_identifier.h"

class GaiaId;
class Profile;
class Browser;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace signin_util {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogOkButtonId);

enum class ProfileSeparationPolicyState {
  kEnforcedByExistingProfile,
  kEnforcedByInterceptedAccount,
  kEnforcedOnMachineLevel,
  kKeepsBrowsingData,
  kMaxValue = kKeepsBrowsingData
};

// Enum used to share the sign in state with the WebUI.
enum class SignedInState {
  kSignedOut = 0,
  kSignedIn = 1,
  kSyncing = 2,
  kSignInPending = 3,
  kWebOnlySignedIn = 4,
  kSyncPaused = 5,
};

// Enum used to indicate if the history sync optin screen
// should be shown or skipped.
enum class ShouldShowHistorySyncOptinResult : int {
  // The screen needs to be shown.
  kShow = 0,
  // The screen is skipped because there is no primary account in
  // error-free state (this includes any SignedInState that is different
  // from SignedInState::kSignedIn).
  kSkipUserNotSignedIn = 1,
  // The screen is skipped because syncing is disabled: this
  // includes a null Sync service, a service disabled due to policies
  // or the history sync setting being managed by policies.
  kSkipSyncForbidden = 2,
  // The screen is skipped because the user is already opted in.
  kSkipUserAlreadyOptedIn = 3,
};

using ProfileSeparationPolicyStateSet =
    base::EnumSet<ProfileSeparationPolicyState,
                  ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                  ProfileSeparationPolicyState::kMaxValue>;

using PrimaryAccountError = signin::PrimaryAccountMutator::PrimaryAccountError;

// This class calls ResetForceSigninForTesting when destroyed, so that
// ForcedSigning doesn't leak across tests.
class ScopedForceSigninSetterForTesting {
 public:
  explicit ScopedForceSigninSetterForTesting(bool enable);
  ~ScopedForceSigninSetterForTesting();
  ScopedForceSigninSetterForTesting(const ScopedForceSigninSetterForTesting&) =
      delete;
  ScopedForceSigninSetterForTesting& operator=(
      const ScopedForceSigninSetterForTesting&) = delete;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// Utility class that moves cookies linked to a URL from one profile to the
// other. This will be mostly used when a new profile is created after a
// signin interception of an account linked a SAML signin.
class CookiesMover {
 public:
  // Moves cookies related to `url` from `source_profile` to
  // `destination_profile` and calls `callback` when it is done.
  CookiesMover(base::WeakPtr<Profile> source_profile,
               base::WeakPtr<Profile> destination_profile,
               base::OnceCallback<void()> callback);

  CookiesMover(const CookiesMover& copy) = delete;
  CookiesMover& operator=(const CookiesMover&) = delete;
  ~CookiesMover();

  void StartMovingCookies();

 private:
  void OnCookiesReceived(
      const std::vector<net::CookieWithAccessResult>& included,
      const std::vector<net::CookieWithAccessResult>& excluded);

  // Called when all the cookies have been moved.
  void OnCookiesMoved();

  GURL url_;
  base::WeakPtr<Profile> source_profile_;
  base::WeakPtr<Profile> destination_profile_;
  base::OnceCallback<void()> callback_;
  base::WeakPtrFactory<CookiesMover> weak_pointer_factory_{this};
};
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing. Please use
// ScopedForceSigninSetterForTesting instead, if possible. If not, make sure
// ResetForceSigninForTesting is called before the test finishes.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Returns true if profile deletion is allowed.
bool IsProfileDeletionAllowed(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)

// Returns true if managed accounts signin are required to create a new profile
// by policies set in `profile`. This will check the by default check the
// ManagedAccountsSigninRestriction policy.
// The optional `intercepted_account_email` will trigger a check to the
// ProfileSeparationDomainExceptionList policy. Unless
// `intercepted_account_email` is not available, it should always be passed.
bool IsProfileSeparationEnforcedByProfile(
    Profile* profile,
    const std::string& intercepted_account_email);

// Returns true if profile separation is enforced by
// `intercepted_account_separation_policies`.
bool IsProfileSeparationEnforcedByPolicies(
    const policy::ProfileSeparationPolicies&
        intercepted_profile_separation_policies);

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const policy::ProfileSeparationPolicies&
        intercepted_profile_separation_policies);

bool IsAccountExemptedFromEnterpriseProfileSeparation(Profile* profile,
                                                      const std::string& email);
#endif  // !BUILDFLAG(IS_CHROMEOS)
// Records a UMA metric if the user accepts or not to create an enterprise
// profile.
void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created);
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(b/339214136): Add a standalone unit for this function.
// Add an account with `user_email` and `gaia_id` to `profile`, and then set it
// as the primary account. A invalid refresh token will be set to mimic the
// behavior of a signed-out user. It is expected that the user is not tracked
// yet.
PrimaryAccountError SetPrimaryAccountWithInvalidToken(
    Profile* profile,
    const std::string& user_email,
    const GaiaId& gaia_id,
    bool is_under_advanced_protection,
    signin_metrics::AccessPoint access_point,
    signin_metrics::SourceForRefreshTokenOperation source);

// Returns true if the Chrome is signed into with an account that is in
// persistent error state. Always return false for Syncing users, even if in
// error state.
bool IsSigninPending(signin::IdentityManager* identity_manager);

// Returns the current state of the primary account that is used in Chrome.
SignedInState GetSignedInState(const signin::IdentityManager* identity_manager);

// Returns a string representation of `SignedInState`.
std::string SignedInStateToString(SignedInState state);

// Checks whether syncing the specified `types` is allowed by policy. May need
// to called again after a sign in to see if policies are set for the account
// rather than the device. This method does not take into account the feature
// flag `ReplaceSyncPromosWithSignInPromos`.
bool IsSyncingUserSelectableTypesAllowedByPolicy(
    const syncer::SyncService* sync_service,
    const syncer::UserSelectableTypeSet& types);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// True if the user has explicitly disabled syncing history, tabs or saved tab
// groups through the settings. The primary account must be set (this crashes
// otherwise).
// This method does not take into account the feature flag
// `ReplaceSyncPromosWithSignInPromos`.
bool HasExplicitlyDisabledHistorySync(
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager);

// Returns the value `ShouldShowHistorySyncOptinResult::kShow`
// if the necessary conditions to show the History Sync Optin screen
// are met. Otherwise it returns a skip reason.
// This method does not take into account the feature flag
// `ReplaceSyncPromosWithSignInPromos`.
// TODO(crbug.com/457397173): Consider using also on mobile and moving the
// method as necessary.
ShouldShowHistorySyncOptinResult ShouldShowHistorySyncOptinScreen(
    Profile& profile);

// Enables the types history, tabs, and saved tab groups for the account
// currently signed into Chrome. If a type cannot be enabled (e.g. by policy),
// this does not do anything for that type.
void EnableHistorySync(syncer::SyncService* sync_service);

// Returns true if the history sync optin screen could be offered via the given
// `access_point`. Returns false if enabling history sync is expected to be done
// via other means for the given access point and the history sync screen should
// not be shown.
bool IsValidAccessPointForHistoryOptinScreen(
    signin_metrics::AccessPoint access_point);

// The avatar sync promo is only shown to users with specific sign in states.
// Requires the feature enabling through
// `switches::IsAvatarSyncPromoFeatureEnabled()`.
bool ShouldShowAvatarSyncPromo(Profile* profile);

// Show a simple error message with an "OK" button to the user, displaying
// `error_message_id`.
void ShowErrorDialogWithMessage(Browser* browser, int error_message_id);
#endif  // BUILDFLAG(IS_LINUX) ||  BUILDFLAG(IS_MAC) ||  BUILDFLAG(IS_WIN)

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
