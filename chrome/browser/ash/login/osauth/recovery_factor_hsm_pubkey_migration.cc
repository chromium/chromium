// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/recovery_factor_hsm_pubkey_migration.h"

#include <memory>
#include <optional>
#include <utility>

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/user_manager/known_user.h"

namespace ash {

RecoveryFactorHsmPubkeyMigration::RecoveryFactorHsmPubkeyMigration(
    UserDataAuthClient* user_data_auth)
    : user_data_auth_(user_data_auth) {}
RecoveryFactorHsmPubkeyMigration::~RecoveryFactorHsmPubkeyMigration() = default;

void RecoveryFactorHsmPubkeyMigration::Run(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback) {
  editor_ = std::make_unique<AuthFactorEditor>(user_data_auth_);
  if (!context->HasAuthFactorsConfiguration()) {
    editor_->GetAuthFactorsConfiguration(
        std::move(context),
        base::BindOnce(
            &RecoveryFactorHsmPubkeyMigration::OnAuthFactorConfigurationLoaded,
            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  UpdateRecoveryFactor(std::move(context), std::move(callback));
}

void RecoveryFactorHsmPubkeyMigration::OnAuthFactorConfigurationLoaded(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(std::move(context), error);
    return;
  }
  UpdateRecoveryFactor(std::move(context), std::move(callback));
}

void RecoveryFactorHsmPubkeyMigration::UpdateRecoveryFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  auto* recovery = context->GetAuthFactorsConfiguration().FindFactorByType(
      cryptohome::AuthFactorType::kRecovery);
  if (!recovery) {
    // No recovery factor.
    was_skipped_ = true;
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  std::string mediator_key =
      recovery->GetCryptohomeRecoveryMetadata().mediator_pub_key;
  if (mediator_key == GetRecoveryHsmPublicKey()) {
    // The latest public key was used for recovery, no need to update.
    was_skipped_ = true;
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  // The key rotation is happening in the background, and the user did not
  // authenticate with the currently present recovery factor. Therefore, we
  // don't want to rotate the recovery id.
  editor_->RotateRecoveryFactor(
      std::move(context),
      /*ensure_fresh_recovery_id=*/false,
      base::BindOnce(&RecoveryFactorHsmPubkeyMigration::OnRecoveryUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RecoveryFactorHsmPubkeyMigration::OnRecoveryUpdated(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to update recovery factor " << error->ToDebugString();
    std::move(callback).Run(std::move(context), error);
    return;
  }
  LOG(WARNING) << "Recovery factor updated";
  std::move(callback).Run(std::move(context), std::nullopt);
}

bool RecoveryFactorHsmPubkeyMigration::WasSkipped() {
  return was_skipped_;
}

AuthFactorMigration::MigrationName RecoveryFactorHsmPubkeyMigration::GetName() {
  return MigrationName::kRecoveryFactorHsmPubkeyMigration;
}

}  // namespace ash
