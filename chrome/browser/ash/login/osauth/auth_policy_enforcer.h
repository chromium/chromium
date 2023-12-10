// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_POLICY_ENFORCER_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_POLICY_ENFORCER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "components/prefs/pref_service.h"

namespace ash {

class UserContext;

// This class encapsulates the logic that, given access to current policies
// (via `AuthPolicyConnector`) and authenticated AuthSession (in form of
// `UserContext`) adjusts auth factor configuration according to policies.
class AuthPolicyEnforcer {
 public:
  AuthPolicyEnforcer(AuthPolicyConnector* connector,
                     UserDataAuthClient* user_data_auth,
                     PrefService* local_state);
  ~AuthPolicyEnforcer();

  AuthPolicyEnforcer(const AuthPolicyEnforcer&) = delete;
  AuthPolicyEnforcer& operator=(const AuthPolicyEnforcer&) = delete;

  // Attempts to apply mandatory policy values to the authenticated
  // authsession. `Context` should contain AuthSession authenticated
  // for `Decrypt` intent.
  // Any cryptohome errors that can happen during this process
  // would be passed to `callback`.
  void CheckAndEnforcePolicies(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);

 private:
  // Determines if `AuthPolicyConnector` affects any of the factors
  // and fills out `out_factors` accordingly.
  void DetermineAffectedFactors(const AccountId& account_id,
                                AuthFactorsSet& out_factors);
  void OnAuthFactorConfigurationLoaded(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  // Ensures that recovery-related policies are applied.
  // If no errors happen, would end up calling `OnPolicesApplied`.
  void EnforceRecoveryPolicies(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);
  void OnRecoveryUpdated(AuthOperationCallback callback,
                         std::unique_ptr<UserContext> context,
                         std::optional<AuthenticationError> error);
  // Called when policies for all applicable factor types were applied.
  void OnPolicesApplied(std::unique_ptr<UserContext> context,
                        AuthOperationCallback callback);

  std::unique_ptr<AuthFactorEditor> editor_;
  raw_ptr<AuthPolicyConnector> connector_;
  raw_ptr<UserDataAuthClient> user_data_auth_;
  raw_ptr<PrefService> local_state_;
  // Must be the last member.
  base::WeakPtrFactory<AuthPolicyEnforcer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_POLICY_ENFORCER_H_
