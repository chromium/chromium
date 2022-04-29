// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/tribool.h"

class Profile;

namespace signin_util {

// This class is used by cloud policy to indicate signout is disallowed for
// cloud-managed enterprise accounts. Signout would require profile destruction
// (See ChromeSigninClient::PreSignOut(),
//      PrimaryAccountPolicyManager::EnsurePrimaryAccountAllowedForProfile()).
// This class is also used on Android to disallow signout for supervised users.
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

  signin::Tribool signout_allowed() const;
  void SetSignoutAllowed(bool is_allowed);

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // `signout_allowed()` is always true for Lacros main profile despite of
  // policies.
  bool is_main_profile_ = false;
#endif
  signin::Tribool signout_allowed_ = signin::Tribool::kUnknown;
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

// Returns true if clearing the primary profile is allowed.
bool IsUserSignoutAllowedForProfile(Profile* profile);

// Sign-out is allowed by default, but some Chrome profiles (e.g. for cloud-
// managed enterprise accounts) may wish to disallow user-initiated sign-out.
// Note that this exempts sign-outs that are not user-initiated (e.g. sign-out
// triggered when cloud policy no longer allows current email pattern). See
// ChromeSigninClient::PreSignOut().
void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed);

// Updates the user sign-out state to |true| if is was never initialized.
// This should be called at the end of the flow to initialize a profile to
// ensure that the signout allowed flag is updated.
void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
// Returns true if profile separation is enforced by policy.
bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value);

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
