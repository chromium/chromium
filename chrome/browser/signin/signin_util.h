// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include <string>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace signin_util {

enum class ProfileSeparationPolicyState {
  kEnforcedByExistingProfile,
  kEnforcedByInterceptedAccount,
  kStrict,
  kEnforcedOnMachineLevel,
  kKeepsBrowsingData,
  kMaxValue = kKeepsBrowsingData
};

using ProfileSeparationPolicyStateSet =
    base::EnumSet<ProfileSeparationPolicyState,
                  ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                  ProfileSeparationPolicyState::kMaxValue>;

// This class is used by cloud policy to indicate signout is disallowed for
// cloud-managed enterprise accounts. Signout would require profile destruction
// (See ChromeSigninClient::PreSignOut(),
//      PrimaryAccountPolicyManager::EnsurePrimaryAccountAllowedForProfile()).
// This class is also used on Android to disallow signout for supervised users.
// Note: Preventing Clear primary account is not fully supported yet.
// This class does not handle UI changes, each feature is responsible of
// handling required UI changes to align with the desired product behavior
// (e.g. hide signout button).
class UserSignoutSetting : public base::SupportsUserData::Data {
 public:
  // Fetch from Profile. Make and store if not already present.
  static UserSignoutSetting* GetForProfile(Profile* profile);

  // Public as this class extends base::SupportsUserData::Data. Use
  // |GetForProfile()| to get the instance associated with a profile.
  UserSignoutSetting();
  ~UserSignoutSetting() override;
  UserSignoutSetting(const UserSignoutSetting&) = delete;
  UserSignoutSetting& operator=(const UserSignoutSetting&) = delete;

  // Initialize the user signout setting if it has not yet been set.
  // Delaying the initialization allows services to bet created  and forbid
  // signout if needed. This protects against allowing signout during this time
  // where it is not known if it should be allowed.
  // This function must be called after profile services are initialized to
  // ensure users who are allowed to signout can signout.
  void InitializeUserSignoutSettingIfNeeded();

  // Reset Signout setting to the default value.
  void ResetSignoutSetting();

  // Returns true if removing/changing a non empty primary account from the
  // profile is allowed. For example: Lacros main profile, the primary account
  // must be the device account and can't be changed/cleared. The same applies
  // for cloud-managed enterprise accounts that has accepted managed.
  bool IsClearPrimaryAccountAllowed() const;
  void SetClearPrimaryAccountAllowed(bool allowed);

  // If Revoke sync consent is disallowed, it implies clearing the primary
  // account is disallowed.
  // This function will be deprecated as soon as turn off sync allowed for
  // enterprise and child users fully launches.
  bool IsRevokeSyncConsentAllowed() const;
  void SetRevokeSyncConsentAllowed(bool allowed);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void IgnoreIsMainProfileForTesting();
#endif

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Turn sync off is always true for Lacros main profile despite of
  // policies.
  bool ignore_is_main_profile_for_testing_ = false;
  bool is_main_profile_ = false;
#endif

  // TODO(crbug.com/1369980): Remove revoke sync restriction when both:
  // - |kAllowSyncOffForChildAccounts| is fully launched and feature flag
  // removed.
  // - Allowing enterprise users to revoke sync fully launches.
  signin::Tribool revoke_sync_consent_allowed_ = signin::Tribool::kUnknown;

  signin::Tribool clear_primary_account_allowed_ = signin::Tribool::kUnknown;
};

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
// Returns the state of profile separation on any account that would signin
// inside `profile`. Returns an empty set if profile separation is not enforced
// on accounts that will sign in the content area of `profile`.
ProfileSeparationPolicyStateSet GetProfileSeparationPolicyState(
    Profile* profile,
    const absl::optional<std::string>& intercepted_account_level_policy_value =
        absl::nullopt);

// Returns true if profile separation must be enforced on an account signing in
// the content area of `profile` by the ManagedAccountsSigninRestriction policy
// for `profile` or if the value of 'intercepted_account_level_policy_value'
// enforces profile separation for an intercepted account.
// `intercepted_account_level_policy_value` has a value only in the case of an
// account interception. This is used mainly in DiceWebSigninInterceptor to
// determine if an intercepted account requires a new profile.
bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const absl::optional<std::string>& intercepted_account_level_policy_value =
        absl::nullopt);

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value);
#endif  // !BUILDFLAG(IS_CHROMEOS)
// Records a UMA metric if the user accepts or not to create an enterprise
// profile.
void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
