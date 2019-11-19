// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_cryptohome.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace chromeos {
namespace quick_unlock {

namespace {

// Key label in cryptohome.
constexpr char kCryptohomePinLabel[] = "pin";

// Read the salt from local state.
std::string GetSalt(const AccountId& account_id) {
  std::string salt;
  user_manager::known_user::GetStringPref(
      account_id, ash::prefs::kQuickUnlockPinSalt, &salt);
  return salt;
}

// Write the salt to local state.
void WriteSalt(const AccountId& account_id, const std::string& salt) {
  user_manager::known_user::SetStringPref(
      account_id, ash::prefs::kQuickUnlockPinSalt, salt);
}

void OnCryptohomeCallComplete(PinStorageCryptohome::BoolCallback callback,
                              bool success,
                              cryptohome::MountError return_code) {
  std::move(callback).Run(success &&
                          return_code == cryptohome::MOUNT_ERROR_NONE);
}

// Checks to see if there is a KeyDefinition instance with the pin label. If
// |require_unlocked| is true, the key must not be locked.
void CheckCryptohomePinKey(PinStorageCryptohome::BoolCallback callback,
                           bool require_unlocked,
                           base::Optional<cryptohome::BaseReply> reply) {
  const cryptohome::MountError return_code =
      cryptohome::GetKeyDataReplyToMountError(reply);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    const std::vector<cryptohome::KeyDefinition>& key_definitions =
        cryptohome::GetKeyDataReplyToKeyDefinitions(reply);
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
void OnGetSupportedKeyPolicies(PinStorageCryptohome::BoolCallback callback,
                               base::Optional<cryptohome::BaseReply> reply) {
  if (!reply) {
    std::move(callback).Run(false);
    return;
  }

  const cryptohome::GetSupportedKeyPoliciesReply& data =
      reply->GetExtension(cryptohome::GetSupportedKeyPoliciesReply::reply);
  std::move(callback).Run(data.low_entropy_credentials());
}

// Forward declare CheckForCryptohomedService because there is a recursive
// dependency on OnCryptohomedServiceAvailable.
void CheckForCryptohomedService(int attempt,
                                PinStorageCryptohome::BoolCallback result);

// Called when cryptohomed status is available. If cryptohomed is not available
// this will rerun the status check (CheckForCryptohomedService) up to N times.
// |attempt| is the current attempt number.
void OnCryptohomedServiceAvailable(int attempt,
                                   PinStorageCryptohome::BoolCallback result,
                                   bool is_available) {
  constexpr int kMaxRetryTimes = 5;
  if (attempt > kMaxRetryTimes) {
    LOG(ERROR) << "Could not talk to cryptohomed";
    std::move(result).Run(false);
  }
  if (!is_available) {
    const int retry_delay_in_milliseconds = 500 * (1 << attempt);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CheckForCryptohomedService, attempt + 1,
                       std::move(result)),
        base::TimeDelta::FromMilliseconds(retry_delay_in_milliseconds));
    return;
  }

  CryptohomeClient::Get()->GetSupportedKeyPolicies(
      cryptohome::GetSupportedKeyPoliciesRequest(),
      base::BindOnce(&OnGetSupportedKeyPolicies, std::move(result)));
}

void CheckForCryptohomedService(int attempt,
                                PinStorageCryptohome::BoolCallback result) {
  CryptohomeClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &OnCryptohomedServiceAvailable, attempt, std::move(result)));
}

}  // namespace

// static
void PinStorageCryptohome::IsSupported(BoolCallback result) {
  CheckForCryptohomedService(0 /*attempt*/, std::move(result));
}

// static
base::Optional<Key> PinStorageCryptohome::TransformKey(
    const AccountId& account_id,
    const Key& key) {
  Key result = key;
  result.SetLabel(kCryptohomePinLabel);

  DCHECK(key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN);
  if (key.GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN)
    return base::nullopt;

  // Try to lookup in known_user.
  const std::string salt = GetSalt(account_id);
  if (salt.empty())
    return base::nullopt;

  result.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  return result;
}

PinStorageCryptohome::PinStorageCryptohome() {
  SystemSaltGetter::Get()->GetSystemSalt(base::AdaptCallbackForRepeating(
      base::BindOnce(&PinStorageCryptohome::OnSystemSaltObtained,
                     weak_factory_.GetWeakPtr())));
}

PinStorageCryptohome::~PinStorageCryptohome() = default;

void PinStorageCryptohome::IsPinSetInCryptohome(const AccountId& account_id,
                                                BoolCallback result) const {
  cryptohome::GetKeyDataRequest request;
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);
  chromeos::CryptohomeClient::Get()->GetKeyDataEx(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      cryptohome::AuthorizationRequest(), request,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&CheckCryptohomePinKey, std::move(result),
                         false /*require_unlocked*/)));
}

void PinStorageCryptohome::SetPin(const UserContext& user_context,
                                  const std::string& pin,
                                  const base::Optional<std::string>& pin_salt,
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
  // PIN, in which case |pin| is already hashed.
  std::string secret;
  std::string salt;
  if (pin_salt) {
    salt = *pin_salt;
    secret = pin;
  } else {
    salt = PinBackend::ComputeSalt();
    secret = PinBackend::ComputeSecret(pin, salt, Key::KEY_TYPE_PASSWORD_PLAIN);
  }

  WriteSalt(user_context.GetAccountId(), salt);

  cryptohome::AddKeyRequest request;
  const cryptohome::KeyDefinition key_def =
      cryptohome::KeyDefinition::CreateForPassword(
          secret, kCryptohomePinLabel,
          cryptohome::PRIV_MOUNT | cryptohome::PRIV_MIGRATE);
  cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
  request.mutable_key()
      ->mutable_data()
      ->mutable_policy()
      ->set_low_entropy_credential(true);
  request.set_clobber_if_exists(true);
  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(key.GetLabel(), key.GetSecret()),
      request,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&OnCryptohomeCallComplete, std::move(did_set))));
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
  cryptohome::RemoveKeyRequest request;
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);
  cryptohome::HomedirMethods::GetInstance()->RemoveKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(
          user_context.GetKey()->GetLabel(),
          user_context.GetKey()->GetSecret()),
      request,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&OnCryptohomeCallComplete, std::move(did_remove))));
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
                                           BoolCallback result) const {
  cryptohome::GetKeyDataRequest request;
  request.mutable_key()->mutable_data()->set_label(kCryptohomePinLabel);
  chromeos::CryptohomeClient::Get()->GetKeyDataEx(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      cryptohome::AuthorizationRequest(), request,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&CheckCryptohomePinKey, std::move(result),
                         true /*require_unlocked*/)));
}

void PinStorageCryptohome::TryAuthenticate(const AccountId& account_id,
                                           const Key& key,
                                           BoolCallback result) {
  const std::string secret = PinBackend::ComputeSecret(
      key.GetSecret(), GetSalt(account_id), key.GetKeyType());
  cryptohome::HomedirMethods::GetInstance()->CheckKeyEx(
      cryptohome::Identification(account_id),
      cryptohome::CreateAuthorizationRequest(kCryptohomePinLabel, secret),
      cryptohome::CheckKeyRequest(),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&OnCryptohomeCallComplete, std::move(result))));
}

}  // namespace quick_unlock
}  // namespace chromeos
