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
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/user_selectable_type.h"
#include "net/base/schemeful_site.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;
struct CoreAccountId;
class GaiaId;
class PrefService;
class Profile;

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
    std::string_view intercepted_account_email);

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
                                                      std::string_view email);
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
// rather than the device.
bool IsSyncingUserSelectableTypesAllowedByPolicy(
    const syncer::SyncService* sync_service,
    const syncer::UserSelectableTypeSet& types);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// True if the user has explicitly disabled syncing history, tabs or saved tab
// groups through the settings. The primary account must be set (this crashes
// otherwise).
bool HasExplicitlyDisabledHistorySync(
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager);

// Returns the value `ShouldShowHistorySyncOptinResult::kShow`
// if the necessary conditions to show the History Sync Optin screen
// are met. Otherwise it returns a skip reason.
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

// Show a simple error message with an "OK" button to the user, displaying
// `error_message_id`.
void ShowErrorDialogWithMessage(BrowserWindowInterface* browser,
                                int error_message_id);

#endif  // BUILDFLAG(IS_LINUX) ||  BUILDFLAG(IS_MAC) ||  BUILDFLAG(IS_WIN)

// Returns the list of sites that register Device Bound Sessions that
// are of interest to signin code.
std::vector<net::SchemefulSite> GetDeviceBoundSessionRestrictedSites();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Result of attempting an in-place primary account switch via
// `SwitchPrimaryAccountWithinProfile()`.
enum class SwitchPrimaryAccountWithinProfileResult {
  // Primary account was switched to the target account.
  kSuccess,
  // `profile` is null or not a regular profile.
  kInvalidProfile,
  // Required sign-in services (`IdentityManager`, `PrimaryAccountMutator`, or
  // `SigninClient`) are unavailable for `profile`.
  kSigninServicesUnavailable,
  // Sign-in to Chrome is disabled (`prefs::kSigninAllowed` is false).
  kSigninNotAllowed,
  // Sign-in cookies are disallowed for `profile`.
  kSigninCookiesDisallowed,
  // Clearing the current primary account is disallowed by `SigninClient` (e.g.
  // managed profile).
  kSignoutDisallowed,
  // Force sign-in policy is enabled.
  kForceSigninEnabled,
  // The current primary account has legacy `ConsentLevel::kSync`.
  kPrimaryAccountHasSyncConsent,
  // The profile does not currently have a primary account.
  kNoPrimaryAccount,
  // `target_account_id` is empty.
  kEmptyTargetAccountId,
  // `target_account_id` is already the primary account.
  kAlreadyPrimaryAccount,
  // Refresh tokens have not finished loading from disk yet.
  kRefreshTokensNotLoaded,
  // `target_account_id` has no refresh token in `IdentityManager` (including
  // unknown accounts).
  kTargetAccountMissingRefreshToken,
  // `target_account_id` has a refresh token in a persistent auth error state.
  kTargetAccountRefreshTokenInError,
  // Extended `AccountInfo` for `target_account_id` is empty or missing email
  // or Gaia ID.
  kTargetAccountInfoIncomplete,
  // `target_account_id`'s email is disallowed by
  // `prefs::kGoogleServicesUsernamePattern`.
  kUsernameNotAllowedByPattern,
  // `target_account_id` is a confirmed managed account (including Flex Org
  // accounts), which cannot be switched into in place without enterprise
  // management consent and profile separation checks.
  kTargetAccountManaged,
  // `target_account_id`'s management status is unknown on a potentially
  // enterprise domain, so switching fails closed to avoid triggering the
  // post-signin enterprise disclaimer flow.
  kTargetAccountManagementStatusUnknown,
  // `target_account_id` is not exempted by
  // `prefs::kProfileSeparationDomainExceptionList` on `profile`.
  kProfileSeparationRequired,
  // `target_account_id` is already signed in as the primary account of another
  // Chrome profile.
  kAccountSignedInOnOtherProfile,
  // Removing the current primary account failed unexpectedly.
  kSignoutFailed,
  // Setting the target account as primary failed unexpectedly.
  kSigninFailed,
};

// Switches `profile`'s primary account within the profile to
// `target_account_id` at `ConsentLevel::kSignin`, leaving the previous primary
// account signed in as a web account (preserving its refresh token, cookies,
// and `SigninPrefs` entries). `local_state` is checked for
// `prefs::kGoogleServicesUsernamePattern` and may be null (in which case
// username pattern restrictions are skipped).
//
// This helper is specific to in-profile switching: it validates the
// preconditions needed to safely perform this swap and executes it, whereas
// candidate selection heuristics and switching promo logic and triggers (e.g.
// `AccountSwitchingManager`) live separately.
//
// This synchronously clears the previous primary account without querying
// `SyncService::GetTypesWithUnsyncedData()` or showing an unsynced-data
// confirmation prompt (stopping Sync and discarding any pending unsynced
// account-scoped data). Callers must prompt beforehand if confirmation is
// required.
[[nodiscard]] SwitchPrimaryAccountWithinProfileResult
SwitchPrimaryAccountWithinProfile(Profile* profile,
                                  const PrefService* local_state,
                                  const CoreAccountId& target_account_id,
                                  signin_metrics::AccessPoint access_point);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
