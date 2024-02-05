// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/knowledge_factor_hash_info_migration.h"

#include <memory>
#include <optional>
#include <utility>

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/user_manager/known_user.h"

namespace ash {

KnowledgeFactorHashInfoMigration::KnowledgeFactorHashInfoMigration(
    UserDataAuthClient* user_data_auth)
    : editor_(std::make_unique<AuthFactorEditor>(user_data_auth)),
      pin_salt_storage_(std::make_unique<quick_unlock::PinSaltStorage>()) {}

KnowledgeFactorHashInfoMigration::~KnowledgeFactorHashInfoMigration() = default;

void KnowledgeFactorHashInfoMigration::Run(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback) {
  if (!context->HasAuthFactorsConfiguration()) {
    editor_->GetAuthFactorsConfiguration(
        std::move(context),
        base::BindOnce(
            &KnowledgeFactorHashInfoMigration::OnAuthFactorConfigurationLoaded,
            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  CheckAuthFactorsToUpdate(std::move(context), std::move(callback));
}

void KnowledgeFactorHashInfoMigration::OnAuthFactorConfigurationLoaded(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(std::move(context), error);
    return;
  }
  CheckAuthFactorsToUpdate(std::move(context), std::move(callback));
}

void KnowledgeFactorHashInfoMigration::CheckAuthFactorsToUpdate(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  // Check if the password factor needs to be updated.
  auto* password = context->GetAuthFactorsConfiguration().FindFactorByType(
      cryptohome::AuthFactorType::kPassword);
  bool password_needs_update =
      password && !password->GetPasswordMetadata().hash_info().has_value();

  // Check if the PIN factor needs to be updated.
  auto* pin = context->GetAuthFactorsConfiguration().FindFactorByType(
      cryptohome::AuthFactorType::kPin);
  bool pin_needs_update =
      (pin && !pin->GetPinMetadata().hash_info().has_value());

  if (!password_needs_update && !pin_needs_update) {
    was_skipped_ = true;
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  if (password_needs_update) {
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &KnowledgeFactorHashInfoMigration::UpdatePasswordFactorMetadata,
        weak_factory_.GetWeakPtr(), std::move(context), std::move(callback),
        password->ref().label(), pin_needs_update));
  } else {
    UpdatePinFactorMetadata(std::move(context), std::move(callback));
  }
}

void KnowledgeFactorHashInfoMigration::UpdatePasswordFactorMetadata(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const cryptohome::KeyLabel& password_label,
    bool pin_needs_update,
    const std::string& system_salt) {
  editor_->UpdatePasswordFactorMetadata(
      std::move(context), password_label, cryptohome::SystemSalt(system_salt),
      base::BindOnce(
          &KnowledgeFactorHashInfoMigration::OnPasswordFactorMetadataUpdated,
          weak_factory_.GetWeakPtr(), std::move(callback), pin_needs_update));
}

void KnowledgeFactorHashInfoMigration::OnPasswordFactorMetadataUpdated(
    AuthOperationCallback callback,
    bool pin_needs_update,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    // It's better that we don't continue to attempt updating PIN metadata. It
    // is likely that PIN metadata update will also fail after after this
    // failure. And even if it does succeed, this migration will need to be run
    // again after next login still, as password metadata hasn't been updated.
    LOG(ERROR) << "Failed to update password factor metadata "
               << error->ToDebugString();
    std::move(callback).Run(std::move(context), error);
    return;
  }
  // UpdatePasswordFactorMetadata clears context auth factor configurations on
  // success, so we don't have to clear it again here.
  if (!pin_needs_update) {
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }
  UpdatePinFactorMetadata(std::move(context), std::move(callback));
}

void KnowledgeFactorHashInfoMigration::UpdatePinFactorMetadata(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  cryptohome::PinSalt salt(pin_salt_storage_->GetSalt(context->GetAccountId()));
  editor_->UpdatePinFactorMetadata(
      std::move(context), std::move(salt),
      base::BindOnce(
          &KnowledgeFactorHashInfoMigration::OnPinFactorMetadataUpdated,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KnowledgeFactorHashInfoMigration::OnPinFactorMetadataUpdated(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to update PIN factor metadata "
               << error->ToDebugString();
    std::move(callback).Run(std::move(context), error);
    return;
  }
  // UpdatePinFactorMetadata clears context auth factor configurations on
  // success, so we don't have to clear it again here.
  std::move(callback).Run(std::move(context), std::nullopt);
}

bool KnowledgeFactorHashInfoMigration::WasSkipped() {
  return was_skipped_;
}

AuthFactorMigration::MigrationName KnowledgeFactorHashInfoMigration::GetName() {
  return MigrationName::kKnowledgeFactorHashInfoMigration;
}

}  // namespace ash
