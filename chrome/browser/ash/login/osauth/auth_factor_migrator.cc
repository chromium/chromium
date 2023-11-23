// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"

#include <memory>
#include <utility>

#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chrome/browser/ash/login/osauth/recovery_factor_hsm_pubkey_migration.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

AuthFactorMigrator::AuthFactorMigrator(
    std::vector<std::unique_ptr<AuthFactorMigration>> migration_steps)
    : migration_steps_{std::move(migration_steps)} {}

AuthFactorMigrator::~AuthFactorMigrator() = default;

// static
std::vector<std::unique_ptr<AuthFactorMigration>>
AuthFactorMigrator::GetMigrationsList(UserDataAuthClient* user_data_auth) {
  auto result = std::vector<std::unique_ptr<AuthFactorMigration>>();
  result.emplace_back(
      std::make_unique<RecoveryFactorHsmPubkeyMigration>(user_data_auth));
  return result;
}

void AuthFactorMigrator::Run(std::unique_ptr<UserContext> context,
                             AuthOperationCallback callback) {
  if (migration_steps_.size() == 0) {
    std::move(callback).Run(std::move(context), absl::nullopt);
  }

  RunImpl(std::move(context), std::move(callback));
}

void AuthFactorMigrator::RunImpl(std::unique_ptr<UserContext> context,
                                 AuthOperationCallback callback) {
  migration_steps_[last_migration_step_]->Run(
      std::move(context),
      base::BindOnce(&AuthFactorMigrator::OnRun, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void AuthFactorMigrator::OnRun(AuthOperationCallback callback,
                               std::unique_ptr<UserContext> context,
                               absl::optional<AuthenticationError> error) {
  // TODO(b/289178330): Send UMA metrics.
  if (error.has_value()) {
    // Note: Implementation of `AuthFactorMigration` logs the error.
    std::move(callback).Run(std::move(context), error);
    return;
  }

  if (last_migration_step_ == migration_steps_.size() - 1) {
    // All migrations were executed.
    std::move(callback).Run(std::move(context), absl::nullopt);
    return;
  }

  last_migration_step_++;
  RunImpl(std::move(context), std::move(callback));
}

}  // namespace ash
