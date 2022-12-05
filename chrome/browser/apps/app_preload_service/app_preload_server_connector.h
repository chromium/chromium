// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace apps {

struct DeviceInfo;
class PreloadAppDefinition;

using GetInitialAppsCallback =
    base::OnceCallback<void(absl::optional<std::vector<PreloadAppDefinition>>)>;

// The AppPreloadServerConnector is used to talk to the App Provisioning Service
// API endpoint. Its role is to build requests and convert responses into
// usable objects.
class AppPreloadServerConnector {
 public:
  AppPreloadServerConnector();
  AppPreloadServerConnector(const AppPreloadServerConnector&) = delete;
  AppPreloadServerConnector& operator=(const AppPreloadServerConnector&) =
      delete;
  ~AppPreloadServerConnector();

  // Fetches a list of apps to be installed on the device at first login from
  // the App Provisioning Service API. `callback` will be called with a list of
  // (possibly zero) apps, or `absl::nullopt` if an error occurred while
  // fetching apps.
  void GetAppsForFirstLogin(
      const DeviceInfo& device_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetInitialAppsCallback callback);

  // Returns the URL for the App Provisioning Service endpoint. Exposed for
  // tests.
  static GURL GetServerUrl();

 private:
  void OnGetAppsForFirstLoginResponse(
      GetInitialAppsCallback callback,
      std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Weak Factory should go last.
  base::WeakPtrFactory<AppPreloadServerConnector> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_
