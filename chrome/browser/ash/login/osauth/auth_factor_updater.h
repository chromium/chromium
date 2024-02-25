// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_UPDATER_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_UPDATER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"
#include "chrome/browser/ash/login/osauth/auth_policy_enforcer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

class UserContext;

// This class encapsulates the logic that runs migrations by running
// `AuthFactorMigrator` and applies policies by running `AuthPolicyEnforcer`.
class AuthFactorUpdater {
 public:
  AuthFactorUpdater(AuthPolicyConnector* connector,
                    UserDataAuthClient* user_data_auth,
                    PrefService* local_state);
  ~AuthFactorUpdater();

  AuthFactorUpdater(const AuthFactorUpdater&) = delete;
  AuthFactorUpdater& operator=(const AuthFactorUpdater&) = delete;

  // Updates the configuration of auth factors.
  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback);

 private:
  void OnMigratorRun(AuthOperationCallback callback,
                     std::unique_ptr<UserContext> context,
                     std::optional<AuthenticationError> error);

  raw_ptr<AuthPolicyConnector> connector_;
  raw_ptr<UserDataAuthClient> user_data_auth_;
  raw_ptr<PrefService> local_state_;
  std::unique_ptr<AuthFactorMigrator> auth_factor_migrator_;
  std::unique_ptr<AuthPolicyEnforcer> auth_policy_enforcer_;
  // Must be the last member.
  base::WeakPtrFactory<AuthFactorUpdater> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_UPDATER_H_
