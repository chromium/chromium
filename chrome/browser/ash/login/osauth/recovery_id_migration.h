// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_ID_MIGRATION_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_ID_MIGRATION_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

namespace ash {

class UserContext;

// This class encapsulates the logic for generating a fresh recovery id if one
// is needed.
class RecoveryIdMigration : public AuthFactorMigration {
 public:
  explicit RecoveryIdMigration(UserDataAuthClient* user_data_auth);
  ~RecoveryIdMigration() override;

  RecoveryIdMigration(const RecoveryIdMigration&) = delete;
  RecoveryIdMigration& operator=(const RecoveryIdMigration&) = delete;

  // Checks if a fresh recovery id is needed and generates one.
  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback) override;
  bool WasSkipped() override;
  MigrationName GetName() override;

 private:
  void OnAuthFactorConfigurationLoaded(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  void GenerateFreshRecoveryId(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);
  void OnFreshRecoveryIdGenerated(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::GenerateFreshRecoveryIdReply> reply);

  bool was_skipped_ = false;
  std::unique_ptr<AuthFactorEditor> editor_;
  raw_ptr<UserDataAuthClient> user_data_auth_;
  // Must be the last member.
  base::WeakPtrFactory<RecoveryIdMigration> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_RECOVERY_ID_MIGRATION_H_
