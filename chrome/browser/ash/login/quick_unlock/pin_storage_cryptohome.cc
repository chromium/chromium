// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/components/cryptohome/cryptohome_util.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/public/cryptohome_key_constants.h"
#include "ash/components/login/auth/public/user_context.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace ash::quick_unlock {
namespace {

template <typename ReplyType>
void OnCryptohomeCallComplete(PinStorageCryptohome::BoolCallback callback,
                              absl::optional<ReplyType> reply) {
  std::move(callback).Run(
      reply.has_value() &&
      reply->error() ==
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
}

// Checks to see if there is a KeyDefinition instance with the pin label. If
// `require_unlocked` is true, the key must not be locked.
void CheckCryptohomePinKey(
    PinStorageCryptohome::BoolCallback callback,
    bool require_unlocked,
    absl::optional<user_data_auth::GetKeyDataReply> reply) {
  const cryptohome::MountError return_code =
      user_data_auth::ReplyToMountError(reply);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    const std::vector<cryptohome::KeyDefinition>& key_definitions =
        user_data_auth::GetKeyDataReplyToKeyDefinitions(reply);
    for (const cryptohome::KeyDefinition& definition : key_definitions) {
      if (definition.label == kCryptohomePinLabel) {
        DCHECK(definition.policy.low_entropy_credential);
        std::move(callback).Run(!require_unlocked ||
                                !definition.policy.auth_locked);
        return;
      }
    }
  }
  std::move(callback).Run(false);
}

// Called after cryptohomed backend is available; used to check if the
// cryptohome supports low entropy credentials (ie, PIN).
void OnGetSupportedKeyPolicies(
    PinStorageCryptohome::BoolCallback callback,
    absl::optional<user_data_auth::GetSupportedKeyPoliciesReply> reply) {
  if (!reply) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(reply->low_entropy_credentials_supported());
}

// Forward declare CheckForCryptohomedService because there is a recursive
// dependency on OnCryptohomedServiceAvailable.
void CheckForCryptohomedService(int attempt,
                                PinStorageCryptohome::BoolCallback result);

// Called when cryptohomed status is available. If cryptohomed is not available
// this will rerun the status check (CheckForCryptohomedService) up to N times.
// `attempt` is the current attempt number.
void OnCryptohomedServiceAvailable(int attempt,
                                   PinStorageCryptohome::BoolCallback result,
                                   bool is_available) {
  constexpr int kMaxRetryTimes = 5;
  if (attempt > kMaxRetryTimes) {
    LOG(ERROR) << "Could not talk to cryptohomed";
    std::move(result).Run(false);
    return;
  }
  if (!is_available) {
    const int retry_delay_in_milliseconds = 500 * (1 << attempt);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CheckForCryptohomedService, attempt + 1,
                       std::move(result)),
        base::Milliseconds(retry_delay_in_milliseconds));
    return;
  }

  UserDataAuthClient::Get()->GetSupportedKeyPolicies(
      user_data_auth::GetSupportedKeyPoliciesRequest(),
      base::BindOnce(&OnGetSupportedKeyPolicies, std::move(result)));
}

void CheckForCryptohomedService(int attempt,
                                PinStorageCryptohome::BoolCallback result) {
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &OnCryptohomedServiceAvailable, attempt, std::move(result)));
}

bool IsCryptohomePinDisabledByPolicy(const AccountId& account_id,
                                     Purpose purpose) {
  auto* test_api = quick_unlock::TestApi::Get();
  if (test_api && test_api->IsQuickUnlockOverridden()) {
    return !test_api->IsPinEnabledByPolicy(purpose);
  }
  PrefService* pref_service = nullptr;
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile) {
    pref_service = profile->GetPrefs();
  }
  return pref_service && IsPinDisabledByPolicy(pref_service, purpose);
}

}  // namespace

// static
void PinStorageCryptohome::IsSupported(BoolCallback result) {
  CheckForCryptohomedService(0 /*attempt*/, std::move(result));
}

// static
absl::optional<Key> PinStorageCryptohome::TransformPinKey(
    const PinSaltStorage* pin_salt_storage,
    const AccountId& account_id,
    const Key& key) {
  Key result = key;
  result.SetLabel(kCryptohomePinLabel);

  DCHECK(key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN);
  if (key.GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN)
    return absl::nullopt;

  const std::string salt = pin_salt_storage->GetSalt(account_id);
  if (salt.empty())
    return absl::nullopt;

  result.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  return result;
}

PinStorageCryptohome::PinStorageCryptohome()
    : pin_salt_storage_(std::make_unique<PinSaltStorage>()) {
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &PinStorageCryptohome::OnSystemSaltObtained, weak_factory_.GetWeakPtr()));
}

PinStorageCryptohome::~PinStorageCryptohome() = default;

void PinStorageCryptohome::IsPinSetInCryptohome(const AccountId& account_id,
                                                BoolCallback result) const {
  user_data_auth::GetKeyDataRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  request.mutable_authorization_request();
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);

  UserDataAuthClient::Get()->GetKeyData(
      request, base::BindOnce(&CheckCryptohomePinKey, std::move(result),
                              false /*require_unlocked*/));
}

void PinStorageCryptohome::SetPin(const UserContext& user_context,
                                  const std::string& pin,
                                  const absl::optional<std::string>& pin_salt,
                                  BoolCallback did_set) {
  // Rerun this method only after we have system salt.
  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(base::BindOnce(
        &PinStorageCryptohome::SetPin, weak_factory_.GetWeakPtr(), user_context,
        pin, pin_salt, std::move(did_set)));
    return;
  }

  DCHECK(!user_context.GetAccountId().empty());

  // Passwords are hashed with SHA256.
  Key key = *user_context.GetKey();
  if (key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN)
    key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt_);

  // If the caller provided a salt then this is a migration from prefs-based
  // PIN, in which case `pin` is already hashed.
  std::string secret;
  std::string salt;
  if (pin_salt) {
    salt = *pin_salt;
    secret = pin;
  } else {
    salt = PinBackend::ComputeSalt();
    secret = PinBackend::ComputeSecret(pin, salt, Key::KEY_TYPE_PASSWORD_PLAIN);
  }

  pin_salt_storage_->WriteSalt(user_context.GetAccountId(), salt);

  ::user_data_auth::AddKeyRequest request;
  const cryptohome::KeyDefinition key_def =
      cryptohome::KeyDefinition::CreateForPassword(secret, kCryptohomePinLabel,
                                                   cryptohome::PRIV_MIGRATE);
  cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
  request.mutable_key()
      ->mutable_data()
      ->mutable_policy()
      ->set_low_entropy_credential(true);
  request.set_clobber_if_exists(true);
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(key.GetLabel(), key.GetSecret());
  UserDataAuthClient::Get()->AddKey(
      request,
      base::BindOnce(&OnCryptohomeCallComplete<::user_data_auth::AddKeyReply>,
                     std::move(did_set)));
}

void PinStorageCryptohome::RemovePin(const UserContext& user_context,
                                     BoolCallback did_remove) {
  // Rerun this method only after we have system salt.
  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(base::BindOnce(
        &PinStorageCryptohome::RemovePin, weak_factory_.GetWeakPtr(),
        user_context, std::move(did_remove)));
    return;
  }

  // Remove any PIN data from cryptohome.
  ::user_data_auth::RemoveKeyRequest request;
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(
          user_context.GetKey()->GetLabel(),
          user_context.GetKey()->GetSecret());
  UserDataAuthClient::Get()->RemoveKey(
      request, base::BindOnce(
                   &OnCryptohomeCallComplete<::user_data_auth::RemoveKeyReply>,
                   std::move(did_remove)));
}

void PinStorageCryptohome::OnSystemSaltObtained(
    const std::string& system_salt) {
  salt_obtained_ = true;
  system_salt_ = system_salt;

  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, system_salt_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();
  // Verify no new callbacks have been added, since they will never run.
  DCHECK(system_salt_callbacks_.empty());
}

void PinStorageCryptohome::CanAuthenticate(const AccountId& account_id,
                                           Purpose purpose,
                                           BoolCallback result) const {
  if (IsCryptohomePinDisabledByPolicy(account_id, purpose)) {
    std::move(result).Run(false);
    return;
  }

  user_data_auth::GetKeyDataRequest request;
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  request.mutable_authorization_request();
  UserDataAuthClient::Get()->GetKeyData(
      request, base::BindOnce(&CheckCryptohomePinKey, std::move(result),
                              true /*require_unlocked*/));
}

void PinStorageCryptohome::TryAuthenticate(const AccountId& account_id,
                                           const Key& key,
                                           Purpose purpose,
                                           BoolCallback result) {
  if (IsCryptohomePinDisabledByPolicy(account_id, purpose)) {
    std::move(result).Run(false);
    return;
  }
  const std::string secret = PinBackend::ComputeSecret(
      key.GetSecret(), pin_salt_storage_->GetSalt(account_id),
      key.GetKeyType());
  ::user_data_auth::CheckKeyRequest request;
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(account_id));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(kCryptohomePinLabel, secret);
  if (purpose == Purpose::kWebAuthn) {
    request.set_unlock_webauthn_secret(true);
  }

  UserDataAuthClient::Get()->CheckKey(
      request,
      base::BindOnce(&OnCryptohomeCallComplete<::user_data_auth::CheckKeyReply>,
                     std::move(result)));
}

void PinStorageCryptohome::SetPinSaltStorageForTesting(
    std::unique_ptr<PinSaltStorage> pin_salt_storage) {
  pin_salt_storage_ = std::move(pin_salt_storage);
}

}  // namespace ash::quick_unlock
