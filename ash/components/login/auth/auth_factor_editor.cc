// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_factor_editor.h"

#include "ash/components/cryptohome/cryptohome_util.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/cryptohome_key_constants.h"
#include "ash/components/login/auth/cryptohome_parameter_utils.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

AuthFactorEditor::AuthFactorEditor() = default;
AuthFactorEditor::~AuthFactorEditor() = default;

void AuthFactorEditor::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthFactorEditor> AuthFactorEditor::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthFactorEditor::AddKioskKey(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback) {
  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  if (auto* key_def = auth_factors.FindKioskKey()) {
    LOGIN_LOG(ERROR) << "Adding Kiosk key while one already exists";
    std::move(callback).Run(
        std::move(context),
        CryptohomeError{user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED});
    return;
  }

  LOGIN_LOG(EVENT) << "Adding Kiosk key";
  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::KeyData* key_data =
      request.mutable_authorization()->mutable_key()->mutable_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
  key_data->set_label(kCryptohomePublicMountLabel);

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    if (context->GetKey()->GetLabel().empty()) {
      context->GetKey()->SetLabel(kCryptohomeGaiaKeyLabel);
    }  // empty label
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthFactorEditor::HashContextKeyAndAdd, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  LOGIN_LOG(EVENT) << "Adding knowledge key from the context "
                   << context->GetKey()->GetKeyType();

  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(*context), key);

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());

  LOGIN_LOG(EVENT) << "Adding challenge-response key from the context";

  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  // Note that we need to specify key_delegate even when we set up a factor,
  // not only when we use it.
  *request.mutable_authorization() = CreateAuthorizationRequestFromKeyDef(
      cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
          *context));

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::HashContextKeyAndAdd(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AddContextKnowledgeKey(std::move(context), std::move(callback));
}

void AuthFactorEditor::ReplaceContextKey(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());
  DCHECK(context->HasReplacementKey());
  DCHECK_NE(context->GetReplacementKey()->GetKeyType(),
            Key::KEY_TYPE_PASSWORD_PLAIN);

  LOGIN_LOG(EVENT) << "Replacing key from context "
                   << context->GetReplacementKey()->GetKeyType();

  user_data_auth::UpdateCredentialRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_old_credential_label(context->GetKey()->GetLabel());
  const Key* key = context->GetReplacementKey();
  cryptohome::KeyDefinitionToKey(
      cryptohome::KeyDefinition::CreateForPassword(
          key->GetSecret(), key->GetLabel(), cryptohome::PRIV_DEFAULT),
      request.mutable_authorization()->mutable_key());

  UserDataAuthClient::Get()->UpdateCredential(
      request, base::BindOnce(&AuthFactorEditor::OnUpdateCredential,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddRecoveryFactor(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  CHECK(features::IsCryptohomeRecoverySetupEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/1310312): Check whether a recovery key already exists and
  // return immediately.

  LOGIN_LOG(EVENT) << "Adding recovery key";

  user_data_auth::AddAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());

  user_data_auth::AuthFactor* factor = req.mutable_auth_factor();
  factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
  factor->set_label(kCryptohomeRecoveryKeyLabel);

  // The cryptohome recovery meta data struct does not have members at the
  // moment.
  factor->mutable_cryptohome_recovery_metadata();

  // TODO(crbug.com/1310312): We only need to set the mediator public key here;
  // all other members are for other operations. The public key will likely be
  // hardcoded, although perhaps configurable via a command line switch for
  // testing.
  user_data_auth::CryptohomeRecoveryAuthInput* input =
      req.mutable_auth_input()->mutable_cryptohome_recovery_input();
  input->set_mediator_pub_key("STUB MEDIATOR PUB KEY");

  auto add_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRecoveryFactorAdded, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));

  UserDataAuthClient::Get()->AddAuthFactor(std::move(req),
                                           std::move(add_auth_factor_callback));
}

void AuthFactorEditor::RemoveRecoveryFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  CHECK(features::IsCryptohomeRecoverySetupEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/1310312): Check whether a recovery key already exists and
  // return immediately if there are no recovery keys.

  LOGIN_LOG(EVENT) << "Removing recovery key";

  user_data_auth::RemoveAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());
  req.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  auto remove_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRecoveryFactorRemoved, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  UserDataAuthClient::Get()->RemoveAuthFactor(
      req, std::move(remove_auth_factor_callback));
}

/// ---- private callbacks ----

void AuthFactorEditor::OnAddCredentials(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AddCredentialsReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AddCredentials failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully added credentials";
  std::move(callback).Run(std::move(context), absl::nullopt);
  // TODO(crbug.com/1310312): Think if we should update AuthFactorsData in
  // context after such operation.
}

void AuthFactorEditor::OnUpdateCredential(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::UpdateCredentialReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "UpdateCredential failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully updated credential";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthFactorEditor::OnRecoveryFactorAdded(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<::user_data_auth::AddAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "AddAuthFactor for recovery failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }

  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully added recovery key";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthFactorEditor::OnRecoveryFactorRemoved(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<::user_data_auth::RemoveAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "RemoveAuthFactor for recovery failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }

  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully removed recovery key";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
