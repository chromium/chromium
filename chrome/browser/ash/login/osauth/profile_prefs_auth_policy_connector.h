// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace ash {

// Implementation of the `AuthPolicyConnector` that can be used after the
// profile is loaded. It uses the profile prefs to get the policy values.
class ProfilePrefsAuthPolicyConnector : public KeyedService,
                                        public AuthPolicyConnector {
 public:
  ProfilePrefsAuthPolicyConnector();
  ~ProfilePrefsAuthPolicyConnector() override;

  void SetLoginScreenAuthPolicyConnector(
      AuthPolicyConnector* connector) override;

  std::optional<bool> GetRecoveryInitialState(
      const AccountId& account) override;
  std::optional<bool> GetRecoveryDefaultState(
      const AccountId& account) override;
  std::optional<bool> GetRecoveryMandatoryState(
      const AccountId& account) override;

  bool IsAuthFactorManaged(const AccountId& account,
                           AshAuthFactor auth_factor) override;
  bool IsAuthFactorUserModifiable(const AccountId& account,
                                  AshAuthFactor auth_factor) override;
  void OnShutdown() override;

 private:
  raw_ptr<AuthPolicyConnector> login_screen_connector_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_H_
