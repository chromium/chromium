// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_performer.h"

#include "ash/components/cryptohome/cryptohome_util.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/cryptohome_parameter_utils.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

AuthPerformer::AuthPerformer() = default;
AuthPerformer::~AuthPerformer() = default;

void AuthPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

void AuthPerformer::StartAuthSession(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     StartSessionCallback callback) {
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPerformer::OnServiceRunning, weak_factory_.GetWeakPtr(),
      std::move(context), ephemeral, std::move(callback)));
}

void AuthPerformer::OnServiceRunning(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     StartSessionCallback callback,
                                     bool service_is_available) {
  if (!service_is_available) {
    // TODO(crbug.com/1262139): Maybe have this error surfaced to UI.
    LOG(FATAL) << "Cryptohome service could not start";
  }
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  if (ephemeral) {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER);
  } else {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  }

  UserDataAuthClient::Get()->StartAuthSession(
      request, base::BindOnce(&AuthPerformer::OnStartAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::AuthenticateUsingKey(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    if (context->GetKey()->GetLabel().empty()) {
      const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
      const cryptohome::KeyDefinition* key_def =
          auth_factors.FindOnlinePasswordKey();
      if (!key_def) {
        LOGIN_LOG(ERROR) << "Could not find Password key";
        std::move(callback).Run(
            std::move(context),
            CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
        return;
      }
      context->GetKey()->SetLabel(key_def->label);
    }  // empty label
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthPerformer::HashKeyAndAuthenticate, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  LOGIN_LOG(EVENT) << "Authenticating using key "
                   << context->GetKey()->GetKeyType();

  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(*context), key);

  UserDataAuthClient::Get()->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback,
                                           const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AuthenticateUsingKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyData* key_data = key->mutable_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);

  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  const cryptohome::KeyDefinition* key_def = auth_factors.FindKioskKey();
  if (!key_def) {
    LOGIN_LOG(ERROR) << "Could not find Kiosk key";
    std::move(callback).Run(
        std::move(context),
        CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  key_data->set_label(key_def->label);

  UserDataAuthClient::Get()->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

/// ---- private callbacks ----

void AuthPerformer::OnStartAuthSession(
    std::unique_ptr<UserContext> context,
    StartSessionCallback callback,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Could not start authsession " << error;
    std::move(callback).Run(false, std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());

  context->SetAuthSessionId(reply->auth_session_id());
  // Remember key metadata
  std::vector<cryptohome::KeyDefinition> key_definitions;
  for (const auto& [label, key_data] : reply->key_label_data()) {
    // Backfill kiosk key type
    // TODO(crbug.com/1310312): Find if there is any better way.
    cryptohome::KeyData data(key_data);
    if (!data.has_type()) {
      LOGIN_LOG(DEBUG) << "Backfilling Kiosk key type for key " << label;
      data.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
    }
    // "legacy-0" keys exist as label in map, but might not exist as labels
    // in KeyData.
    if (!data.has_label() || data.label().empty()) {
      data.set_label(label);
    }
    key_definitions.push_back(KeyDataToKeyDefinition(data));
  }
  AuthFactorsData auth_factors_data(std::move(key_definitions));
  context->SetAuthFactorsData(std::move(auth_factors_data));

  std::move(callback).Run(reply->user_exists(), std::move(context),
                          absl::nullopt);
}

void AuthPerformer::OnAuthenticateAuthSession(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to authenticate session " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  DCHECK(reply->authenticated());
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
