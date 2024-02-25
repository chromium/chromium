// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_FACTOR_HSM_PUBKEY_MIGRATION_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_FACTOR_HSM_PUBKEY_MIGRATION_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "components/prefs/pref_service.h"

namespace ash {

class UserContext;

// This class encapsulates the logic that reads recovery factor configuration
// and updates it as needed.
class RecoveryFactorHsmPubkeyMigration : public AuthFactorMigration {
 public:
  explicit RecoveryFactorHsmPubkeyMigration(UserDataAuthClient* user_data_auth);
  ~RecoveryFactorHsmPubkeyMigration() override;

  RecoveryFactorHsmPubkeyMigration(const RecoveryFactorHsmPubkeyMigration&) =
      delete;
  RecoveryFactorHsmPubkeyMigration& operator=(
      const RecoveryFactorHsmPubkeyMigration&) = delete;

  // Updates the configuration of recovery factor factor.
  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback) override;
  bool WasSkipped() override;
  MigrationName GetName() override;

 private:
  void OnAuthFactorConfigurationLoaded(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  void UpdateRecoveryFactor(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback);
  void OnRecoveryUpdated(AuthOperationCallback callback,
                         std::unique_ptr<UserContext> context,
                         std::optional<AuthenticationError> error);

  bool was_skipped_ = false;
  std::unique_ptr<AuthFactorEditor> editor_;
  raw_ptr<UserDataAuthClient> user_data_auth_;
  // Must be the last member.
  base::WeakPtrFactory<RecoveryFactorHsmPubkeyMigration> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_FACTOR_HSM_PUBKEY_MIGRATION_H_
