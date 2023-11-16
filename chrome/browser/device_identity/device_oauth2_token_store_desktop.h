// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/device_identity/device_oauth2_token_store.h"
#include "google_apis/gaia/core_account_id.h"

class PrefRegistrySimple;
class PrefService;

// The pref name where this class stores the encrypted refresh token.
extern const char kCBCMServiceAccountRefreshToken[];

// The pref name where this class stores the service account's email.
extern const char kCBCMServiceAccountEmail[];

// The desktop (mac, win, linux) implementation of DeviceOAuth2TokenStore. This
// is used by the DeviceOAuth2TokenService on those platforms to encrypt and
// persist the refresh token of the service account to LocalState.
class DeviceOAuth2TokenStoreDesktop : public DeviceOAuth2TokenStore {
 public:
  explicit DeviceOAuth2TokenStoreDesktop(PrefService* local_state);
  ~DeviceOAuth2TokenStoreDesktop() override;

  DeviceOAuth2TokenStoreDesktop(const DeviceOAuth2TokenStoreDesktop& other) =
      delete;
  DeviceOAuth2TokenStoreDesktop& operator=(
      const DeviceOAuth2TokenStoreDesktop& other) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DeviceOAuth2TokenStore:
  void Init(InitCallback callback) override;
  CoreAccountId GetAccountId() const override;
  std::string GetRefreshToken() const override;
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              StatusCallback result_callback) override;
  void PrepareTrustedAccountId(TrustedAccountIdCallback callback) override;
  void SetAccountEmail(const std::string& account_email) override;

 private:
  void OnServiceAccountIdentityChanged();

  // Called the first time GetRefreshToken is called if |token_decrypted_| is
  // false. It decrypts |refresh_token_| using OSCrypt and writes it back to
  // |refresh_token_|.
  void DecryptToken() const;

  const raw_ptr<PrefService> local_state_;

  // This and the |token_decrypted_| field are mutable because they are modified
  // on the first call to |GetRefreshToken()|, which is const.
  mutable std::string refresh_token_;

  // The token is decrypted the first time it's read rather than on Init,
  // because OSCrypt hasn't been initialized early enough on some platforms.
  // This field is false and |refresh_token_| is encrypted until the first token
  // read.
  mutable bool token_decrypted_ = false;

  base::WeakPtrFactory<DeviceOAuth2TokenStoreDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_
