// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"

#include "base/functional/callback.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "ui/gfx/image/image.h"

namespace crosapi {

DeviceOAuth2TokenServiceAsh::DeviceOAuth2TokenServiceAsh()
    : OAuth2AccessTokenManager::Consumer("device_oauth2_token_service_ash") {}

DeviceOAuth2TokenServiceAsh::~DeviceOAuth2TokenServiceAsh() = default;

void DeviceOAuth2TokenServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceOAuth2TokenServiceAsh::FetchAccessTokenForDeviceAccount(
    const std::vector<std::string>& scopes,
    FetchAccessTokenForDeviceAccountCallback callback) {
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      DeviceOAuth2TokenServiceFactory::Get()->StartAccessTokenRequest(
          signin::ScopeSet(scopes.begin(), scopes.end()), this);
  auto [it, success] =
      access_token_requests_.emplace(std::move(request), std::move(callback));
  DCHECK(success);
}

void DeviceOAuth2TokenServiceAsh::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  OnTokenFetchComplete(
      request,
      mojom::AccessTokenResult::NewAccessTokenInfo(mojom::AccessTokenInfo::New(
          token_response.access_token, token_response.expiration_time,
          token_response.id_token)));
}

void DeviceOAuth2TokenServiceAsh::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  OnTokenFetchComplete(
      request, mojom::AccessTokenResult::NewError(
                   account_manager::ToMojoGoogleServiceAuthError(error)));
}

void DeviceOAuth2TokenServiceAsh::OnTokenFetchComplete(
    const OAuth2AccessTokenManager::Request* request,
    mojom::AccessTokenResultPtr result) {
  auto it = access_token_requests_.find(request);
  if (it == access_token_requests_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  // To avoid UaF, the item should be removed from the map before calling the
  // callback. This requires keeping the callback in a temporary variable.
  auto callback = std::move(it->second);
  access_token_requests_.erase(it);
  if (callback)
    std::move(callback).Run(std::move(result));
  // `this` may be destroyed.
}

}  // namespace crosapi
