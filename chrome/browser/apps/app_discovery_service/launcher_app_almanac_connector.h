// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_CONNECTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace apps {

using GetAppsCallback =
    base::OnceCallback<void(absl::optional<proto::LauncherAppResponse>)>;

// The LauncherAppAlmanacConnector is used to talk to the Launcher App endpoint
// in the Almanac server. Its role is to make requests and receive responses.
class LauncherAppAlmanacConnector {
 public:
  LauncherAppAlmanacConnector();
  LauncherAppAlmanacConnector(const LauncherAppAlmanacConnector&) = delete;
  LauncherAppAlmanacConnector& operator=(const LauncherAppAlmanacConnector&) =
      delete;
  ~LauncherAppAlmanacConnector();

  // Fetches a list of apps from the endpoint.
  void GetApps(
      const DeviceInfo& device_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetAppsCallback callback);

  // Returns the GURL for the endpoint. Exposed for tests.
  static GURL GetServerUrl();

 private:
  // Processes the response to the given request and checks for errors.
  void OnGetAppsResponse(std::unique_ptr<network::SimpleURLLoader> loader,
                         GetAppsCallback callback,
                         std::unique_ptr<std::string> response_body);

  base::WeakPtrFactory<LauncherAppAlmanacConnector> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_CONNECTOR_H_
