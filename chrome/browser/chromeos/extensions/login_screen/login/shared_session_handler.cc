// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_ash.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace chromeos {

namespace {

constexpr size_t kSessionSecretLength = 64;
constexpr size_t kUserSaltLength = 16;
constexpr size_t kHashKeyLength = 32;
constexpr uint64_t kScryptCost = 1 << 12;
constexpr uint64_t kScryptBlockSize = 8;
constexpr uint64_t kScryptParallelization = 1;
constexpr size_t kScryptMaxMemory = 1024 * 1024 * 32;

const user_manager::User* GetManagedGuestSessionUser() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetUsers()) {
    if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
      continue;
    }

    return user;
  }
  return nullptr;
}

bool IsDeviceRestrictedManagedGuestSessionEnabled() {
  bool device_restricted_managed_guest_session_enabled = false;
  return ash::CrosSettings::Get()->GetBoolean(
             ash::kDeviceRestrictedManagedGuestSessionEnabled,
             &device_restricted_managed_guest_session_enabled) &&
         device_restricted_managed_guest_session_enabled;
}

}  // namespace

// static
SharedSessionHandler* SharedSessionHandler::Get() {
  static base::NoDestructor<SharedSessionHandler> instance;
  return instance.get();
}

SharedSessionHandler::SharedSessionHandler() = default;

SharedSessionHandler::~SharedSessionHandler() = default;

std::optional<std::string>
SharedSessionHandler::LaunchSharedManagedGuestSession(
    const std::string& password) {
  if (!IsDeviceRestrictedManagedGuestSessionEnabled()) {
    return extensions::login_api_errors::
        kDeviceRestrictedManagedGuestSessionNotEnabled;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return extensions::login_api_errors::kLoginScreenIsNotActive;
  }

  CHECK(session_secret_.empty());
  CHECK(user_secret_hash_.empty());
  CHECK(user_secret_salt_.empty());

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress())
    return extensions::login_api_errors::kAnotherLoginAttemptInProgress;

  const user_manager::User* user = GetManagedGuestSessionUser();
  if (user == nullptr)
    return extensions::login_api_errors::kNoManagedGuestSessionAccounts;

  if (!CreateAndSetUserSecretHashAndSalt(password))
    return extensions::login_api_errors::kScryptFailure;

  session_secret_ = GenerateRandomString(kSessionSecretLength);

  ash::UserContext context(user_manager::UserType::kPublicAccount,
                           user->GetAccountId());
  context.SetKey(ash::Key(session_secret_));
  context.SetCanLockManagedGuestSession(true);
  existing_user_controller->Login(context, ash::SigninSpecifics());

  return std::nullopt;
}

void SharedSessionHandler::EnterSharedSession(
    const std::string& password,
    CallbackWithOptionalError callback) {
  if (!IsDeviceRestrictedManagedGuestSessionEnabled()) {
    std::move(callback).Run(extensions::login_api_errors::
                                kDeviceRestrictedManagedGuestSessionNotEnabled);
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    std::move(callback).Run(extensions::login_api_errors::kSessionIsNotLocked);
    return;
  }

  if (session_secret_.empty()) {
    std::move(callback).Run(extensions::login_api_errors::kNoSharedMGSFound);
    return;
  }

  if (!user_secret_hash_.empty()) {
    std::move(callback).Run(
        extensions::login_api_errors::kSharedSessionAlreadyLaunched);
    return;
  }

  CHECK(user_secret_salt_.empty());

  if (chromeos::CleanupManagerAsh::Get()->is_cleanup_in_progress()) {
    std::move(callback).Run(extensions::login_api_errors::kCleanupInProgress);
    return;
  }

  if (LoginApiLockHandler::Get()->IsUnlockInProgress()) {
    std::move(callback).Run(
        extensions::login_api_errors::kAnotherUnlockAttemptInProgress);
    return;
  }

  if (!CreateAndSetUserSecretHashAndSalt(password)) {
    std::move(callback).Run(extensions::login_api_errors::kScryptFailure);
    return;
  }

  UnlockWithSessionSecret(
      base::BindOnce(&SharedSessionHandler::OnAuthenticateDone,
                     base::Unretained(this), std::move(callback)));
}

void SharedSessionHandler::UnlockSharedSession(
    const std::string& password,
    CallbackWithOptionalError callback) {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    std::move(callback).Run(extensions::login_api_errors::kSessionIsNotLocked);
    return;
  }

  if (session_secret_.empty()) {
    std::move(callback).Run(extensions::login_api_errors::kNoSharedMGSFound);
    return;
  }

  if (user_secret_hash_.empty()) {
    std::move(callback).Run(
        extensions::login_api_errors::kSharedSessionIsNotActive);
    return;
  }

  CHECK(!user_secret_salt_.empty());

  if (chromeos::CleanupManagerAsh::Get()->is_cleanup_in_progress()) {
    std::move(callback).Run(extensions::login_api_errors::kCleanupInProgress);
    return;
  }

  if (LoginApiLockHandler::Get()->IsUnlockInProgress()) {
    std::move(callback).Run(
        extensions::login_api_errors::kAnotherUnlockAttemptInProgress);
    return;
  }

  std::optional<std::string> scrypt_result =
      GetHashFromScrypt(password, user_secret_salt_);

  if (!scrypt_result) {
    std::move(callback).Run(extensions::login_api_errors::kScryptFailure);
    return;
  }

  const std::string& hash_key = *scrypt_result;

  CHECK(hash_key.length() == user_secret_hash_.length());

  if (!crypto::SecureMemEqual(hash_key.data(), user_secret_hash_.data(),
                              user_secret_hash_.size())) {
    std::move(callback).Run(
        extensions::login_api_errors::kAuthenticationFailed);
    return;
  }

  UnlockWithSessionSecret(
      base::BindOnce(&SharedSessionHandler::OnAuthenticateDone,
                     base::Unretained(this), std::move(callback)));
}

void SharedSessionHandler::EndSharedSession(
    CallbackWithOptionalError callback) {
  if (session_secret_.empty()) {
    std::move(callback).Run(extensions::login_api_errors::kNoSharedMGSFound);
    return;
  }

  if (user_secret_hash_.empty()) {
    std::move(callback).Run(
        extensions::login_api_errors::kSharedSessionIsNotActive);
    return;
  }

  chromeos::CleanupManagerAsh* cleanup_manager =
      chromeos::CleanupManagerAsh::Get();
  if (cleanup_manager->is_cleanup_in_progress()) {
    std::move(callback).Run(extensions::login_api_errors::kCleanupInProgress);
    return;
  }

  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  DCHECK(session_state == session_manager::SessionState::LOCKED ||
         session_state == session_manager::SessionState::ACTIVE);

  user_secret_hash_.clear();
  user_secret_salt_.clear();

  if (session_state != session_manager::SessionState::LOCKED) {
    LoginApiLockHandler::Get()->RequestLockScreen();
  }

  cleanup_manager->Cleanup(base::BindOnce(&SharedSessionHandler::OnCleanupDone,
                                          base::Unretained(this),
                                          std::move(callback)));
}

const std::string& SharedSessionHandler::GetSessionSecretForTesting() const {
  return session_secret_;
}

const std::string& SharedSessionHandler::GetUserSecretHashForTesting() const {
  return user_secret_hash_;
}

const std::string& SharedSessionHandler::GetUserSecretSaltForTesting() const {
  return user_secret_salt_;
}

void SharedSessionHandler::ResetStateForTesting() {
  session_secret_.clear();
  user_secret_hash_.clear();
  user_secret_salt_.clear();
}

std::optional<std::string> SharedSessionHandler::GetHashFromScrypt(
    const std::string& password,
    const std::string& salt) {
  std::string hash_key;
  uint8_t* key_data = reinterpret_cast<uint8_t*>(
      base::WriteInto(&hash_key, kHashKeyLength + 1));
  int scrypt_ok =
      EVP_PBE_scrypt(password.data(), password.size(),
                     reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
                     kScryptCost, kScryptBlockSize, kScryptParallelization,
                     kScryptMaxMemory, key_data, kHashKeyLength);

  if (!scrypt_ok)
    return std::nullopt;
  return hash_key;
}

void SharedSessionHandler::UnlockWithSessionSecret(
    base::OnceCallback<void(bool)> callback) {
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                active_user->GetAccountId());
  user_context.SetKey(ash::Key(session_secret_));
  LoginApiLockHandler::Get()->Authenticate(user_context, std::move(callback));
}

bool SharedSessionHandler::CreateAndSetUserSecretHashAndSalt(
    const std::string& password) {
  std::string salt = GenerateRandomString(kUserSaltLength);
  std::optional<std::string> scrypt_result = GetHashFromScrypt(password, salt);

  if (!scrypt_result)
    return false;

  user_secret_hash_ = std::move(*scrypt_result);
  user_secret_salt_ = std::move(salt);

  return true;
}

void SharedSessionHandler::OnAuthenticateDone(
    CallbackWithOptionalError callback,
    bool auth_success) {
  if (!auth_success) {
    std::move(callback).Run(extensions::login_api_errors::kUnlockFailure);
    return;
  }

  std::move(callback).Run(std::nullopt);
}

void SharedSessionHandler::OnCleanupDone(
    CallbackWithOptionalError callback,
    const std::optional<std::string>& errors) {
  if (errors) {
    std::move(callback).Run(*errors);
    return;
  }

  std::move(callback).Run(std::nullopt);
}

std::string SharedSessionHandler::GenerateRandomString(size_t size) {
  return base::HexEncode(crypto::RandBytesAsVector(size));
}

}  // namespace chromeos
