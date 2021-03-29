// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_remove_keys_operation.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

EasyUnlockRemoveKeysOperation::EasyUnlockRemoveKeysOperation(
    const UserContext& user_context,
    size_t start_index,
    RemoveKeysCallback callback)
    : user_context_(user_context),
      callback_(std::move(callback)),
      key_index_(start_index) {
  // Must have the secret and callback.
  DCHECK(!user_context_.GetKey()->GetSecret().empty());
  DCHECK(!callback_.is_null());
}

EasyUnlockRemoveKeysOperation::~EasyUnlockRemoveKeysOperation() {}

void EasyUnlockRemoveKeysOperation::Start() {
  if (user_context_.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    SystemSaltGetter::Get()->GetSystemSalt(
        base::BindOnce(&EasyUnlockRemoveKeysOperation::OnGetSystemSalt,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  RemoveKey();
}

void EasyUnlockRemoveKeysOperation::OnGetSystemSalt(
    const std::string& system_salt) {
  user_context_.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                    system_salt);
  RemoveKey();
}

void EasyUnlockRemoveKeysOperation::RemoveKey() {
  const Key* const auth_key = user_context_.GetKey();
  // TODO(crbug.com/558497): Use ListKeyEx and delete by label instead of by
  // index.
  ::user_data_auth::RemoveKeyRequest request;
  request.mutable_key()->mutable_data()->set_label(
      EasyUnlockKeyManager::GetKeyLabel(key_index_));
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification((user_context_.GetAccountId())));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret());
  chromeos::UserDataAuthClient::Get()->RemoveKey(
      request, base::BindOnce(&EasyUnlockRemoveKeysOperation::OnKeyRemoved,
                              weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockRemoveKeysOperation::OnKeyRemoved(
    base::Optional<::user_data_auth::RemoveKeyReply> reply) {
  if (reply.has_value() &&
      reply->error() ==
          ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    ++key_index_;
    RemoveKey();
    return;
  }

  cryptohome::MountError return_code =
      user_data_auth::CryptohomeErrorToMountError(reply->error());
  // MOUNT_ERROR_KEY_FAILURE is considered as success. Other error codes are
  // treated as failures.
  if (return_code == cryptohome::MOUNT_ERROR_KEY_FAILURE) {
    std::move(callback_).Run(true);
  } else {
    LOG(ERROR) << "Easy unlock remove keys operation failed, code="
               << return_code;
    std::move(callback_).Run(false);
  }
}

}  // namespace chromeos
