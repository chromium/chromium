// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user_manager.h"

namespace ash::quick_unlock {

namespace {

using ::cryptohome::KeyLabel;

void CheckCryptohomePinFactor(
    PinStorageCryptohome::AvailabilityCallback callback,
    bool require_unlocked,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  const cryptohome::AuthFactor* pin_factor =
      config.FindFactorByType(cryptohome::AuthFactorType::kPin);
  if (!pin_factor) {
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  if (require_unlocked && pin_factor->GetPinStatus().IsLockedFactor()) {
    std::move(callback).Run(false, pin_factor->GetPinStatus().AvailableAt());
    return;
  }

  std::move(callback).Run(true, pin_factor->GetPinStatus().AvailableAt());
}

// Called after cryptohomed backend is available; used to check if the
// cryptohome supports low entropy credentials (ie, PIN).
void OnGetSupportedKeyPolicies(
    PinStorageCryptohome::BoolCallback callback,
    std::optional<user_data_auth::GetSupportedKeyPoliciesReply> reply) {
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
std::optional<Key> PinStorageCryptohome::TransformPinKey(
    const PinSaltStorage* pin_salt_storage,
    const AccountId& account_id,
    const Key& key) {
  Key result = key;
  result.SetLabel(kCryptohomePinLabel);

  DCHECK(key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN);
  if (key.GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN)
    return std::nullopt;

  const std::string salt = pin_salt_storage->GetSalt(account_id);
  if (salt.empty())
    return std::nullopt;

  result.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  return result;
}

PinStorageCryptohome::PinStorageCryptohome()
    : pin_salt_storage_(std::make_unique<PinSaltStorage>()),
      auth_factor_editor_(UserDataAuthClient::Get()),
      auth_performer_(UserDataAuthClient::Get()) {
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &PinStorageCryptohome::OnSystemSaltObtained, weak_factory_.GetWeakPtr()));
}

PinStorageCryptohome::~PinStorageCryptohome() = default;

void PinStorageCryptohome::IsPinSetInCryptohome(
    std::unique_ptr<UserContext> user_context,
    BoolCallback result) {
  // Pass the enabled boolean result, ignore the availability timestamp.
  auto availability_callback =
      [](BoolCallback callback, bool enabled,
         cryptohome::PinLockAvailability available_at) {
        std::move(callback).Run(enabled);
      };
  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(&CheckCryptohomePinFactor,
                     base::BindOnce(availability_callback, std::move(result)),
                     false /*require_unlocked*/));
}

void PinStorageCryptohome::SetPin(std::unique_ptr<UserContext> user_context,
                                  const std::string& pin,
                                  const std::optional<std::string>& pin_salt,
                                  AuthOperationCallback callback) {
  // Rerun this method only after we have system salt.
  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(base::BindOnce(
        &PinStorageCryptohome::SetPin, weak_factory_.GetWeakPtr(),
        std::move(user_context), std::move(pin), std::move(pin_salt),
        std::move(callback)));
    return;
  }

  // Possible TODO: Get rid of this requirement. pin_salt has a value if this
  // call is for migrating a pref pin to a cryptohome pin.
  DCHECK(!pin_salt.has_value());

  cryptohome::PinSalt salt{PinBackend::ComputeSalt()};
  cryptohome::RawPin raw_pin{pin};

  pin_salt_storage_->WriteSalt(user_context->GetAccountId(), *salt);
  auto on_pin_edited =
      base::BindOnce(&PinStorageCryptohome::OnAuthFactorsEdit,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  // There's a rare condition if we crash here or something goes wrong in the
  // next call: In that case we have updated the salt, but cryptohome is not
  // aware of the changed pin that uses the new salt.
  if (user_context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPin)) {
    auth_factor_editor_.ReplacePinFactor(std::move(user_context),
                                         std::move(salt), std::move(raw_pin),
                                         std::move(on_pin_edited));
    return;
  } else {
    auth_factor_editor_.AddPinFactor(std::move(user_context), std::move(salt),
                                     std::move(raw_pin),
                                     std::move(on_pin_edited));
  }
}

void PinStorageCryptohome::RemovePin(std::unique_ptr<UserContext> user_context,
                                     AuthOperationCallback callback) {
  // Rerun this method only after we have system salt.
  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(base::BindOnce(
        &PinStorageCryptohome::RemovePin, weak_factory_.GetWeakPtr(),
        std::move(user_context), std::move(callback)));
    return;
  }

  auto on_pin_edited =
      base::BindOnce(&PinStorageCryptohome::OnAuthFactorsEdit,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  auth_factor_editor_.RemovePinFactor(std::move(user_context),
                                      std::move(on_pin_edited));
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

void PinStorageCryptohome::CanAuthenticate(
    std::unique_ptr<UserContext> user_context,
    Purpose purpose,
    AvailabilityCallback result_callback) {
  if (IsCryptohomePinDisabledByPolicy(user_context->GetAccountId(), purpose)) {
    std::move(result_callback).Run(false, std::nullopt);
    return;
  }

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(&CheckCryptohomePinFactor, std::move(result_callback),
                     true /*require_unlocked*/));
}

void PinStorageCryptohome::TryAuthenticate(
    std::unique_ptr<UserContext> user_context,
    const Key& key,
    Purpose purpose,
    AuthOperationCallback callback) {
  if (IsCryptohomePinDisabledByPolicy(user_context->GetAccountId(), purpose)) {
    AuthenticationError error{AuthFailure::AUTH_DISABLED};
    std::move(callback).Run(std::move(user_context), std::move(error));
    return;
  }
  CHECK_NE(purpose, Purpose::kWebAuthn)
      << "Webauth dialog uses direct interaction with cryptohome";

  if (!user_context->GetAuthSessionId().empty()) {
    NOTREACHED_IN_MIGRATION()
        << "TryAuthenticate called with existing auth session";
    user_context->ResetAuthSessionIds();
  }

  // We need to start an auth session, which requires us to specify whether
  // the user is ephemeral or not. Ephemeral users shouldn't be able to set
  // up a pin, so we should never end up here.
  bool ephemeral = false;
  if (user_manager::UserManager::IsInitialized()) {
    ephemeral = user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
        user_context->GetAccountId());
  }
  DCHECK(!ephemeral);

  auto on_start_auth_session =
      base::BindOnce(&PinStorageCryptohome::TryAuthenticateWithAuthSession,
                     weak_factory_.GetWeakPtr(), key, std::move(callback));
  auth_performer_.StartAuthSession(
      std::move(user_context), ephemeral /*ephemeral*/,
      AuthSessionIntent::kVerifyOnly, std::move(on_start_auth_session));
}

void PinStorageCryptohome::SetPinSaltStorageForTesting(
    std::unique_ptr<PinSaltStorage> pin_salt_storage) {
  pin_salt_storage_ = std::move(pin_salt_storage);
}

void PinStorageCryptohome::OnAuthFactorsEdit(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to edit pin, code " << error->get_cryptohome_error();
    std::move(callback).Run(std::move(user_context), std::move(error));
    return;
  }

  auth_factor_editor_.GetAuthFactorsConfiguration(std::move(user_context),
                                                  std::move(callback));
}

void PinStorageCryptohome::TryAuthenticateWithAuthSession(
    const Key& key,
    AuthOperationCallback callback,
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  DCHECK_EQ(key.GetKeyType(), Key::KEY_TYPE_PASSWORD_PLAIN);
  DCHECK(user_exists);

  if (error.has_value()) {
    std::move(callback).Run(std::move(user_context), std::move(error));
    return;
  }

  const std::string& raw_pin = key.GetSecret();
  std::string salt = pin_salt_storage_->GetSalt(user_context->GetAccountId());
  auth_performer_.AuthenticateWithPin(
      raw_pin, std::move(salt), std::move(user_context), std::move(callback));
}

}  // namespace ash::quick_unlock
