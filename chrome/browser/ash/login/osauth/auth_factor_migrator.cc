// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chrome/browser/ash/login/osauth/knowledge_factor_hash_info_migration.h"
#include "chrome/browser/ash/login/osauth/recovery_factor_hsm_pubkey_migration.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

namespace {

// Histogram prefix for the result of a migration.
// See metadata/ash/histograms.xml.
constexpr char kMigrationResultHistogramPrefix[] =
    "Ash.OSAuth.Login.AuthFactorMigrationResult.";

// Histogram name which is used as a suffix for the result of a migration.
// See metadata/ash/histograms.xml.
std::string GetAuthFactorMigrationName(
    AuthFactorMigration::MigrationName name) {
  switch (name) {
    case AuthFactorMigration::MigrationName::kRecoveryFactorHsmPubkeyMigration:
      return "RecoveryFactorHsmPubkeyMigration";
    case AuthFactorMigration::MigrationName::kKnowledgeFactorHashInfoMigration:
      return "KnowledgeFactorHashInfoMigration";
  }
}

AuthFactorMigrator::MigrationResult GetAuthFactorMigrationResult(
    bool is_skipped,
    bool has_error) {
  if (is_skipped) {
    return AuthFactorMigrator::MigrationResult::kSkipped;
  }
  if (has_error) {
    return AuthFactorMigrator::MigrationResult::kFailed;
  }
  return AuthFactorMigrator::MigrationResult::kSuccess;
}

void RecordMigrationResultMetrics(AuthFactorMigration::MigrationName name,
                                  AuthFactorMigrator::MigrationResult result) {
  std::string histogram_name = base::StrCat(
      {kMigrationResultHistogramPrefix, GetAuthFactorMigrationName(name)});
  base::UmaHistogramEnumeration(histogram_name, result);
}

}  // namespace

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
  result.emplace_back(
      std::make_unique<KnowledgeFactorHashInfoMigration>(user_data_auth));
  return result;
}

void AuthFactorMigrator::Run(std::unique_ptr<UserContext> context,
                             AuthOperationCallback callback) {
  if (migration_steps_.size() == 0) {
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
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
                               std::optional<AuthenticationError> error) {
  auto* migration_step = migration_steps_[last_migration_step_].get();
  RecordMigrationResultMetrics(
      migration_step->GetName(),
      GetAuthFactorMigrationResult(migration_step->WasSkipped(),
                                   error.has_value()));

  if (error.has_value()) {
    LOG(ERROR) << "Migration "
               << GetAuthFactorMigrationName(migration_step->GetName())
               << " failed with error " << error->ToDebugString();

    size_t last_not_run = last_migration_step_ + 1;
    while (last_not_run < migration_steps_.size()) {
      auto* not_run_step = migration_steps_[last_not_run].get();
      RecordMigrationResultMetrics(
          not_run_step->GetName(),
          AuthFactorMigrator::MigrationResult::kNotRun);
      ++last_not_run;
    }

    std::move(callback).Run(std::move(context), error);
    return;
  }

  if (last_migration_step_ == migration_steps_.size() - 1) {
    // All migrations were executed.
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  ++last_migration_step_;
  RunImpl(std::move(context), std::move(callback));
}

}  // namespace ash
