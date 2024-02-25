// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"

#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "chrome/common/pref_names.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// This pref will hold the base64-encoded representation of the encrypted
// refresh token for the browser's service account.
const char kCBCMServiceAccountRefreshToken[] =
    "cbcm.service_account_refresh_token";

// The account email for the robot account used for policy invalidations on
// Desktop platforms by Chrome Browser Cloud Management (CBCM). This is similar
// to kDeviceRobotAnyApiRefreshToken on ChromeOS.
const char kCBCMServiceAccountEmail[] = "cbcm.service_account_email";

DeviceOAuth2TokenStoreDesktop::DeviceOAuth2TokenStoreDesktop(
    PrefService* local_state)
    : local_state_(local_state) {}
DeviceOAuth2TokenStoreDesktop::~DeviceOAuth2TokenStoreDesktop() = default;

// static
void DeviceOAuth2TokenStoreDesktop::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kCBCMServiceAccountRefreshToken, std::string());
  registry->RegisterStringPref(kCBCMServiceAccountEmail, std::string());
}

void DeviceOAuth2TokenStoreDesktop::Init(InitCallback callback) {
  std::string base64_encrypted_token =
      local_state_->GetString(kCBCMServiceAccountRefreshToken);

  if (base64_encrypted_token.empty()) {
    // It's valid for the refresh token to not exist in the store, in
    // which case init is successful and there shouldn't be a token
    // validation step.
    std::move(callback).Run(true, false);
    return;
  }

  std::string decoded;
  if (!base::Base64Decode(base64_encrypted_token, &decoded)) {
    std::move(callback).Run(false, true);
    return;
  }

  refresh_token_ = decoded;

  // If the robot account ID is not available yet, do not announce the token.
  // It will be done from OnServiceAccountIdentityChanged() once the robot
  // account ID becomes available as well.
  if (observer() && !GetAccountId().empty())
    observer()->OnRefreshTokenAvailable();

  std::move(callback).Run(true, true);
}

CoreAccountId DeviceOAuth2TokenStoreDesktop::GetAccountId() const {
  return CoreAccountId::FromRobotEmail(
      local_state_->GetString(kCBCMServiceAccountEmail));
}

std::string DeviceOAuth2TokenStoreDesktop::GetRefreshToken() const {
  // Only trigger token decryption if there is a refresh token present already,
  // it wasn't decrypted already.
  if (!token_decrypted_ && !refresh_token_.empty())
    DecryptToken();

  return refresh_token_;
}

void DeviceOAuth2TokenStoreDesktop::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    StatusCallback result_callback) {
  std::string encrypted_token;
  bool success = OSCrypt::EncryptString(refresh_token, &encrypted_token);

  if (success) {
    refresh_token_ = refresh_token;

    // Set token_decrypted_ to true here, because it's possible that there was
    // no encrypted token on disc, or that it wasn't read, meaning that follow
    // up calls to GetRefreshToken would attempt to decrypt an already
    // un-encrypted token.
    token_decrypted_ = true;

    // The string must be encoded as base64 for storage in local state.
    std::string encoded = base::Base64Encode(encrypted_token);

    local_state_->SetString(kCBCMServiceAccountRefreshToken, encoded);
    if (observer() && !GetAccountId().empty())
      observer()->OnRefreshTokenAvailable();
  }

  std::move(result_callback).Run(success);
}

void DeviceOAuth2TokenStoreDesktop::PrepareTrustedAccountId(
    TrustedAccountIdCallback callback) {
  // There's no cryptohome or anything similar to initialize on non-chromeos
  // platforms, so just run the callback as a success now.
  callback.Run(true);
}

void DeviceOAuth2TokenStoreDesktop::SetAccountEmail(
    const std::string& account_email) {
  if (GetAccountId() == CoreAccountId::FromRobotEmail(account_email))
    return;

  local_state_->SetString(kCBCMServiceAccountEmail, account_email);
  OnServiceAccountIdentityChanged();
}

void DeviceOAuth2TokenStoreDesktop::OnServiceAccountIdentityChanged() {
  if (observer() && !GetAccountId().empty() && !refresh_token_.empty())
    observer()->OnRefreshTokenAvailable();
}

void DeviceOAuth2TokenStoreDesktop::DecryptToken() const {
  DCHECK(!token_decrypted_);
  DCHECK(!refresh_token_.empty());

  std::string decrypted_token;
  bool success = OSCrypt::DecryptString(refresh_token_, &decrypted_token);
  if (success) {
    refresh_token_ = decrypted_token;
    token_decrypted_ = true;
  }
}
