// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_IDENTITY_PROVIDER_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_IDENTITY_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/invalidation/public/identity_provider.h"

class DeviceOAuth2TokenService;

// Identity provider implementation backed by DeviceOAuth2TokenService.
class DeviceIdentityProvider : public invalidation::IdentityProvider {
 public:
  explicit DeviceIdentityProvider(DeviceOAuth2TokenService* token_service);

  DeviceIdentityProvider(const DeviceIdentityProvider&) = delete;
  DeviceIdentityProvider& operator=(const DeviceIdentityProvider&) = delete;

  ~DeviceIdentityProvider() override;

  // IdentityProvider:
  CoreAccountId GetActiveAccountId() override;
  bool IsActiveAccountWithRefreshToken() override;
  std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
  FetchAccessToken(
      const std::string& oauth_consumer_name,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      invalidation::ActiveAccountAccessTokenCallback callback) override;
  void InvalidateAccessToken(const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token) override;

 private:
  void OnRefreshTokenAvailable();

  raw_ptr<DeviceOAuth2TokenService> token_service_;
};

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_IDENTITY_PROVIDER_H_
