// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DEVICE_OAUTH2_TOKEN_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_DEVICE_OAUTH2_TOKEN_SERVICE_LACROS_H_

#include "chromeos/crosapi/mojom/device_oauth2_token_service.mojom.h"

#include "base/memory/weak_ptr.h"

// This class can be used by lacros to access the device oauth2 token service
// crosapi. Lives in lacros. Allows lacros-chrome to access the device oauth2
// token service that lives in ash, in particular to fetch access tokens for the
// device (robot) account.

class DeviceOAuth2TokenServiceLacros {
 public:
  DeviceOAuth2TokenServiceLacros();
  DeviceOAuth2TokenServiceLacros(const DeviceOAuth2TokenServiceLacros&) =
      delete;
  DeviceOAuth2TokenServiceLacros& operator=(
      const DeviceOAuth2TokenServiceLacros&) = delete;
  virtual ~DeviceOAuth2TokenServiceLacros();

  void FetchAccessTokenForDeviceAccount(
      const std::vector<std::string>& scopes,
      crosapi::mojom::DeviceOAuth2TokenService::
          FetchAccessTokenForDeviceAccountCallback callback);

 private:
  void RunFetchAccessTokenForDeviceAccountCallback(
      crosapi::mojom::DeviceOAuth2TokenService::
          FetchAccessTokenForDeviceAccountCallback callback,
      crosapi::mojom::AccessTokenResultPtr result);

  base::WeakPtrFactory<class DeviceOAuth2TokenServiceLacros> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_DEVICE_OAUTH2_TOKEN_SERVICE_LACROS_H_
