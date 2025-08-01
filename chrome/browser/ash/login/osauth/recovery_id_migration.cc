// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/recovery_id_migration.h"

#include <memory>
#include <optional>
#include <utility>

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

RecoveryIdMigration::RecoveryIdMigration(UserDataAuthClient* user_data_auth)
    : user_data_auth_(user_data_auth) {}
RecoveryIdMigration::~RecoveryIdMigration() = default;

void RecoveryIdMigration::Run(std::unique_ptr<UserContext> context,
                              AuthOperationCallback callback) {
  auto* recovery = context->GetAuthFactorsConfiguration().FindFactorByType(
      cryptohome::AuthFactorType::kRecovery);

  if (!context->GenerateFreshRecoveryId() || !recovery) {
    was_skipped_ = true;
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  user_data_auth::GenerateFreshRecoveryIdRequest req;
  *req.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());
  user_data_auth_->GenerateFreshRecoveryId(
      req, base::BindOnce(&RecoveryIdMigration::OnFreshRecoveryIdGenerated,
                          weak_factory_.GetWeakPtr(), std::move(context),
                          std::move(callback)));
}

void RecoveryIdMigration::OnFreshRecoveryIdGenerated(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::GenerateFreshRecoveryIdReply> reply) {
  context->SetGenerateFreshRecoveryId(false);
  // We don't check for an error here, as failing to generate a fresh recovery
  // ID is not a fatal error. The rest of the migrations can proceed.
  if (!reply.has_value() ||
      reply->error() != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to generate fresh recovery ID";
  }
  std::move(callback).Run(std::move(context), std::nullopt);
}

bool RecoveryIdMigration::WasSkipped() {
  return was_skipped_;
}

AuthFactorMigration::MigrationName RecoveryIdMigration::GetName() {
  return MigrationName::kRecoveryIdMigration;
}

}  // namespace ash