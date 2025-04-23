// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"

#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/device_identity/chromeos/token_encryptor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

std::string LoadRefreshTokenV3() {
  std::string refresh_token;
  base::FilePath path;
  base::PathService::Get(chrome::FILE_CHROME_OS_DEVICE_REFRESH_TOKEN, &path);
  if (!base::PathExists(path) ||
      !base::ReadFileToString(path, &refresh_token)) {
    refresh_token.clear();
  }

  return refresh_token;
}

}  // namespace

namespace chromeos {

BASE_FEATURE(kRefreshTokenV3Feature,
             "RefreshTokenV3Feature",
             base::FEATURE_DISABLED_BY_DEFAULT);

DeviceOAuth2TokenStoreChromeOS::DeviceOAuth2TokenStoreChromeOS(
    PrefService* local_state)
    : local_state_(local_state),
      service_account_identity_subscription_(
          ash::CrosSettings::Get()->AddSettingsObserver(
              ash::kServiceAccountIdentity,
              base::BindRepeating(&DeviceOAuth2TokenStoreChromeOS::
                                      OnServiceAccountIdentityChanged,
                                  base::Unretained(this)))) {}

DeviceOAuth2TokenStoreChromeOS::~DeviceOAuth2TokenStoreChromeOS() {
  FlushTokenSaveCallbacks(false);
}

// static
void DeviceOAuth2TokenStoreChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshTokenV1,
                               std::string());
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshTokenV2,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceRefreshTokenAnyApiIsV3Used,
                                false);
}

void DeviceOAuth2TokenStoreChromeOS::Init(InitCallback callback) {
  state_ = State::INITIALIZING;

  if (!base::FeatureList::IsEnabled(kRefreshTokenV3Feature) ||
      !local_state_->GetBoolean(prefs::kDeviceRefreshTokenAnyApiIsV3Used)) {
    // Pull in the system salt.
    ash::SystemSaltGetter::Get()->GetSystemSalt(
        base::BindOnce(&DeviceOAuth2TokenStoreChromeOS::DidGetSystemSalt,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    base::ThreadPool::CreateTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
        ->PostTaskAndReplyWithResult(
            FROM_HERE, base::BindOnce(&LoadRefreshTokenV3),
            base::BindOnce(
                &DeviceOAuth2TokenStoreChromeOS::OnRefreshTokenLoadedV3,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

CoreAccountId DeviceOAuth2TokenStoreChromeOS::GetAccountId() const {
  std::string email;
  ash::CrosSettings::Get()->GetString(ash::kServiceAccountIdentity, &email);
  return CoreAccountId::FromEmail(email);
}

std::string DeviceOAuth2TokenStoreChromeOS::GetRefreshToken() const {
  return refresh_token_;
}

void DeviceOAuth2TokenStoreChromeOS::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    StatusCallback callback) {
  refresh_token_ = refresh_token;
  // If the robot account ID is not available yet, do not announce the token. It
  // will be done from OnServiceAccountIdentityChanged() once the robot account
  // ID becomes available as well.
  if (observer() && !GetAccountId().empty()) {
    observer()->OnRefreshTokenAvailable();
  }

  token_save_callbacks_.push_back(std::move(callback));
  if (state_ == State::READY) {
    // TODO(b/320682630): When the feature is removed, we need to make sure to
    // remove the code that stores the token using the old methods, but also
    // make sure the FlushTokenSaveCallbacks() is called.
    if (base::FeatureList::IsEnabled(kRefreshTokenV3Feature)) {
      StoreRefreshTokenV3();
      return;
    }

    if (system_salt_.empty()) {
      FlushTokenSaveCallbacks(false);
    } else {
      EncryptAndSaveToken();
    }
  }
}

void DeviceOAuth2TokenStoreChromeOS::OnStoreTokenV3Done(bool success) {
  if (success) {
    // The value true is meant to be just a flag, that means the version 3 of
    // storage for refresh_token is used.
    local_state_->SetBoolean(prefs::kDeviceRefreshTokenAnyApiIsV3Used, true);
  }

  // Keep storing the token with the old method, to have a backup in case the
  // feature is rolled back.
  if (system_salt_.empty()) {
    FlushTokenSaveCallbacks(false);
  } else {
    EncryptAndSaveToken();
  }
}

void DeviceOAuth2TokenStoreChromeOS::OnRefreshTokenLoadedV3(
    InitCallback callback,
    const std::string& refresh_token) {
  if (refresh_token.empty()) {
    std::move(callback).Run(/*init_result=*/false,
                            /*validation_required=*/false);
    return;
  }
  refresh_token_ = refresh_token;
  std::move(callback).Run(/*init_result=*/true, /*validation_required=*/true);
}

void DeviceOAuth2TokenStoreChromeOS::StoreRefreshTokenV3() {
  base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              [](const std::string& refresh_token) {
                base::FilePath path;
                base::PathService::Get(
                    chrome::FILE_CHROME_OS_DEVICE_REFRESH_TOKEN, &path);
                return base::ImportantFileWriter::WriteFileAtomically(
                    path, refresh_token);
              },
              refresh_token_),
          base::BindOnce(&DeviceOAuth2TokenStoreChromeOS::OnStoreTokenV3Done,
                         weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOAuth2TokenStoreChromeOS::PrepareTrustedAccountId(
    TrustedAccountIdCallback callback) {
  // Make sure the value returned by GetRobotAccountId has been validated
  // against current device settings.
  switch (ash::CrosSettings::Get()->PrepareTrustedValues(
      base::BindOnce(&DeviceOAuth2TokenStoreChromeOS::PrepareTrustedAccountId,
                     weak_ptr_factory_.GetWeakPtr(), callback))) {
    case ash::CrosSettingsProvider::TRUSTED:
      // All good, let the service compare account ids.
      callback.Run(true);
      return;
    case ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // The callback passed to PrepareTrustedValues above will trigger a
      // re-check eventually.
      return;
    case ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // There's no trusted account id, which is equivalent to no token present.
      LOG(WARNING) << "Device settings permanently untrusted.";
      callback.Run(false);
      return;
  }
}

void DeviceOAuth2TokenStoreChromeOS::FlushTokenSaveCallbacks(bool result) {
  std::vector<DeviceOAuth2TokenStore::StatusCallback> callbacks;
  callbacks.swap(token_save_callbacks_);
  for (std::vector<DeviceOAuth2TokenStore::StatusCallback>::iterator callback(
           callbacks.begin());
       callback != callbacks.end(); ++callback) {
    if (!callback->is_null()) {
      std::move(*callback).Run(result);
    }
  }
}

void DeviceOAuth2TokenStoreChromeOS::EncryptAndSaveToken() {
  ash::CryptohomeTokenEncryptor encryptor(system_salt_);
  std::string encrypted_refresh_token =
      encryptor.EncryptWithSystemSalt(refresh_token_);
  bool result = true;
  if (encrypted_refresh_token.empty()) {
    LOG(ERROR) << "Failed to encrypt refresh token; save aborted.";
    result = false;
  } else {
    local_state_->SetString(prefs::kDeviceRobotAnyApiRefreshTokenV2,
                            encrypted_refresh_token);
  }

  FlushTokenSaveCallbacks(result);
}

std::optional<std::string>
DeviceOAuth2TokenStoreChromeOS::LoadAndDecryptToken() {
  // Try to load a more strongly encrypted v2 token if it exists, but if it does
  // not it will fall back to trying to load a weaker v1 token. If neither
  // exists we return an empty string.
  ash::CryptohomeTokenEncryptor encryptor(system_salt_);
  std::string encrypted_token, decrypted_token;
  if (encrypted_token =
          local_state_->GetString(prefs::kDeviceRobotAnyApiRefreshTokenV2);
      !encrypted_token.empty()) {
    decrypted_token = encryptor.DecryptWithSystemSalt(encrypted_token);
    if (decrypted_token.empty()) {
      LOG(ERROR) << "Failed to decrypt v2 refresh token.";
      return std::nullopt;
    }
  } else if (encrypted_token = local_state_->GetString(
                 prefs::kDeviceRobotAnyApiRefreshTokenV1);
             !encrypted_token.empty()) {
    decrypted_token = encryptor.WeakDecryptWithSystemSalt(encrypted_token);
    if (decrypted_token.empty()) {
      LOG(ERROR) << "Failed to decrypt v1 refresh token.";
      return std::nullopt;
    }
  }
  return decrypted_token;
}

void DeviceOAuth2TokenStoreChromeOS::DidGetSystemSalt(
    InitCallback callback,
    const std::string& system_salt) {
  state_ = State::READY;
  system_salt_ = system_salt;

  // Bail out if system salt is not available.
  if (system_salt_.empty()) {
    LOG(ERROR) << "Failed to get system salt.";
    FlushTokenSaveCallbacks(false);
    std::move(callback).Run(false, false);
    return;
  }

  // If the token has been set meanwhile, write it to |local_state_|.
  if (!refresh_token_.empty()) {
    if (base::FeatureList::IsEnabled(kRefreshTokenV3Feature)) {
      StoreRefreshTokenV3();
      return;
    }

    EncryptAndSaveToken();
    std::move(callback).Run(true, false);
    return;
  }

  // Otherwise, load the refresh token from |local_state_|.
  std::optional<std::string> token = LoadAndDecryptToken();
  if (token.has_value()) {
    refresh_token_ = std::move(*token);
    std::move(callback).Run(true, true);
    return;
  }
  std::move(callback).Run(false, false);
}

void DeviceOAuth2TokenStoreChromeOS::OnServiceAccountIdentityChanged() {
  if (observer() && !GetAccountId().empty() && !refresh_token_.empty()) {
    observer()->OnRefreshTokenAvailable();
  }
}

}  // namespace chromeos
