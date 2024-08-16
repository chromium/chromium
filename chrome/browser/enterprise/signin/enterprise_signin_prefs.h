// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_signin {

enum class ProfileReauthPrompt {
  kDoNotPrompt = 0,
  kPromptInTab = 1,
};

namespace prefs {
// Whether or not admin wants to guide users through reauth when their GAIA
// session expires. This is a ProfileReauthPrompt enum.
inline constexpr char kProfileReauthPrompt[] =
    "enterprise_signin.profile_reauth_prompt";

// Pref storage for profile level information including user name, email etc.
// It is separated from entries that stores information for signed-in users
// since there may not be one in some cases, e.g. OIDC-managed profiles.
inline constexpr char kProfileUserDisplayName[] =
    "enterprise_signin.profile_user_display_name";
inline constexpr char kProfileUserEmail[] =
    "enterprise_signin.profile_user_email";

// Pref storage for information needed to restore OIDC profiles when policies
// are lost. `kProfileUserEmail` is also needed to identify the user.
inline constexpr char kPolicyRecoveryToken[] =
    "enterprise_signin.policy_recovery_token";
inline constexpr char kPolicyRecoveryClientId[] =
    "enterprise_signin.policy_recovery_client_id";

inline constexpr char kPolicyRecoveryRequired[] =
    "enterprise_signin.policy_recovery_required";
}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace enterprise_signin

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_
