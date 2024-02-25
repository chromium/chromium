// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

namespace ash {
class UserDataAuthClient;

// This class encapsulates the logic that runs all auth factor migration
// operations (e.g. factor updates, policy adjustments).
class AuthFactorMigrator {
 public:
  explicit AuthFactorMigrator(
      std::vector<std::unique_ptr<AuthFactorMigration>> migration_steps);
  ~AuthFactorMigrator();

  AuthFactorMigrator(const AuthFactorMigrator&) = delete;
  AuthFactorMigrator& operator=(const AuthFactorMigrator&) = delete;

  // Returns the default list of migrations to be executed.
  static std::vector<std::unique_ptr<AuthFactorMigration>> GetMigrationsList(
      UserDataAuthClient* user_data_auth);

  // Runs all auth factor migration operations.
  // Any cryptohome errors that can happen during this process
  // would be passed to `callback`.
  // Implementation of this method should log an error.
  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback);

  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update MigrationResult enum in enums.xml when adding new entries.
  // Public for testing.
  enum class MigrationResult {
    kSkipped = 0,  // The migration was skipped because it wasn't required.
    kSuccess,
    kFailed,
    kNotRun,  // The migration wasn't run at all because the previous migration
              // failed.
    kMaxValue = kNotRun,
  };

 private:
  void RunImpl(std::unique_ptr<UserContext> context,
               AuthOperationCallback callback);
  void OnRun(AuthOperationCallback callback,
             std::unique_ptr<UserContext> context,
             std::optional<AuthenticationError> error);

  size_t last_migration_step_ = 0;
  std::vector<std::unique_ptr<AuthFactorMigration>> migration_steps_;
  // Must be the last member.
  base::WeakPtrFactory<AuthFactorMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATOR_H_
