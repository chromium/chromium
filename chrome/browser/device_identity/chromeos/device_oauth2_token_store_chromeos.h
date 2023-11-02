// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_DEVICE_OAUTH2_TOKEN_STORE_CHROMEOS_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_DEVICE_OAUTH2_TOKEN_STORE_CHROMEOS_H_

#include "chrome/browser/device_identity/device_oauth2_token_store.h"

#include "chrome/browser/ash/settings/cros_settings.h"

class PrefRegistrySimple;

namespace chromeos {

// ChromeOS specific implementation of the DeviceOAuth2TokenStore interface used
// by the DeviceOAuth2TokenService to store and retrieve encrypted device-level
// refresh tokens.
class DeviceOAuth2TokenStoreChromeOS : public DeviceOAuth2TokenStore {
 public:
  enum class State {
    STOPPED,
    INITIALIZING,
    READY,
  };

  explicit DeviceOAuth2TokenStoreChromeOS(PrefService* local_state);
  ~DeviceOAuth2TokenStoreChromeOS() override;

  DeviceOAuth2TokenStoreChromeOS(const DeviceOAuth2TokenStoreChromeOS& other) =
      delete;
  DeviceOAuth2TokenStoreChromeOS& operator=(
      const DeviceOAuth2TokenStoreChromeOS& other) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DeviceOAuth2TokenStore:
  void Init(InitCallback callback) override;
  CoreAccountId GetAccountId() const override;
  std::string GetRefreshToken() const override;
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              StatusCallback result_callback) override;
  // Asks CrosSettings to prepare trusted values and waits for that to complete
  // before invoking |callback|, at which point it will be known if there is an
  // available trusted account ID.
  void PrepareTrustedAccountId(TrustedAccountIdCallback callback) override;

 private:
  // Flushes |token_save_callbacks_|, indicating the specified result.
  void FlushTokenSaveCallbacks(bool result);

  // Encrypts and saves the refresh token. Should only be called when the system
  // salt is available.
  void EncryptAndSaveToken();

  // Handles completion of the system salt input. Will invoke |callback| since
  // this function is what happens at the end of the initialization process.
  void DidGetSystemSalt(InitCallback callback, const std::string& system_salt);

  // Invoked by CrosSettings when the robot account ID becomes available.
  void OnServiceAccountIdentityChanged();

  State state_ = State::STOPPED;

  PrefService* local_state_;

  base::CallbackListSubscription service_account_identity_subscription_;

  // The system salt for encrypting and decrypting the refresh token.
  std::string system_salt_;

  // Cache the decrypted refresh token, so we only decrypt once.
  std::string refresh_token_;

  // Token save callbacks waiting to be completed.
  std::vector<StatusCallback> token_save_callbacks_;

  base::WeakPtrFactory<DeviceOAuth2TokenStoreChromeOS> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_DEVICE_OAUTH2_TOKEN_STORE_CHROMEOS_H_
