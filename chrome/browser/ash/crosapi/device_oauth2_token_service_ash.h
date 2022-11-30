// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_OAUTH2_TOKEN_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_OAUTH2_TOKEN_SERVICE_ASH_H_

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/device_oauth2_token_service.mojom.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi device OAuth2 token service interface. Lives in
// ash-chrome. Allows lacros-chrome to access the device oauth2 token service
// that lives in ash, in particular to fetch access tokens for the device
// (robot) account.
class DeviceOAuth2TokenServiceAsh : public mojom::DeviceOAuth2TokenService,
                                    OAuth2AccessTokenManager::Consumer {
 public:
  DeviceOAuth2TokenServiceAsh();
  DeviceOAuth2TokenServiceAsh(const DeviceOAuth2TokenServiceAsh&) = delete;
  DeviceOAuth2TokenServiceAsh& operator=(const DeviceOAuth2TokenServiceAsh&) =
      delete;
  ~DeviceOAuth2TokenServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver);

  // crosapi::mojom::DeviceOAuth2TokenService:
  void FetchAccessTokenForDeviceAccount(
      const std::vector<std::string>& scopes,
      FetchAccessTokenForDeviceAccountCallback callback) override;

  // OAuth2AccessTokenManager::Consumer
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

 private:
  void OnTokenFetchComplete(const OAuth2AccessTokenManager::Request* request,
                            mojom::AccessTokenResultPtr result);

  mojo::ReceiverSet<mojom::DeviceOAuth2TokenService> receivers_;
  base::flat_map<std::unique_ptr<OAuth2AccessTokenManager::Request>,
                 FetchAccessTokenForDeviceAccountCallback,
                 base::UniquePtrComparator>
      access_token_requests_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_OAUTH2_TOKEN_SERVICE_ASH_H_
