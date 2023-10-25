// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_CONNECTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

class PackageId;
struct AppInstallData;
struct DeviceInfo;

class AppInstallAlmanacConnector {
 public:
  static GURL GetEndpointUrlForTesting();

  AppInstallAlmanacConnector();
  AppInstallAlmanacConnector(const AppInstallAlmanacConnector&) = delete;
  AppInstallAlmanacConnector& operator=(const AppInstallAlmanacConnector&) =
      delete;
  ~AppInstallAlmanacConnector();

  // TODO(b/304681468): Report specific errors on failure for metrics.
  using GetAppInstallInfoCallback =
      base::OnceCallback<void(absl::optional<AppInstallData>)>;

  void GetAppInstallInfo(PackageId package_id,
                         DeviceInfo device_info,
                         network::mojom::URLLoaderFactory& url_loader_factory,
                         GetAppInstallInfoCallback callback);

 private:
  void OnAppInstallResponse(std::unique_ptr<network::SimpleURLLoader> loader,
                            GetAppInstallInfoCallback callback,
                            std::unique_ptr<std::string> response_body);

  base::WeakPtrFactory<AppInstallAlmanacConnector> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_CONNECTOR_H_
