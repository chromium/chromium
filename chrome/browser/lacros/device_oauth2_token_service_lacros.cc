// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/device_oauth2_token_service_lacros.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/account_manager_core/account_manager_util.h"

namespace {

using RemoteMinVersions =
    crosapi::mojom::DeviceOAuth2TokenService::MethodMinVersions;

chromeos::LacrosService* GetLacrosService(int min_version,
                                          const std::string& function_name) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service)
    return nullptr;
  int interface_version =
      service->GetInterfaceVersion<crosapi::mojom::DeviceOAuth2TokenService>();
  if (interface_version < min_version) {
    DLOG(ERROR) << "Unsupported ash version for " << function_name;
    return nullptr;
  }
  return service;
}

}  // namespace

DeviceOAuth2TokenServiceLacros::DeviceOAuth2TokenServiceLacros() {}

DeviceOAuth2TokenServiceLacros::~DeviceOAuth2TokenServiceLacros() = default;

void DeviceOAuth2TokenServiceLacros::FetchAccessTokenForDeviceAccount(
    const std::vector<std::string>& scopes,
    crosapi::mojom::DeviceOAuth2TokenService::
        FetchAccessTokenForDeviceAccountCallback callback) {
  chromeos::LacrosService* service = GetLacrosService(
      RemoteMinVersions::kFetchAccessTokenForDeviceAccountMinVersion,
      "FetchAccessTokenForDeviceAccount");
  if (!service) {
    if (callback) {
      std::move(callback).Run(crosapi::mojom::AccessTokenResult::NewError(
          account_manager::ToMojoGoogleServiceAuthError(
              GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR))));
    }
    return;
  }

  service->GetRemote<crosapi::mojom::DeviceOAuth2TokenService>()
      ->FetchAccessTokenForDeviceAccount(
          scopes,
          base::BindOnce(&DeviceOAuth2TokenServiceLacros::
                             RunFetchAccessTokenForDeviceAccountCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceOAuth2TokenServiceLacros::
    RunFetchAccessTokenForDeviceAccountCallback(
        crosapi::mojom::DeviceOAuth2TokenService::
            FetchAccessTokenForDeviceAccountCallback callback,
        crosapi::mojom::AccessTokenResultPtr result) {
  if (callback)
    std::move(callback).Run(std::move(result));
}
