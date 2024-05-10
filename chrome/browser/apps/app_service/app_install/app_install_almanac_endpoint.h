// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_ENDPOINT_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_ENDPOINT_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "url/gurl.h"

class GURL;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

class PackageId;
struct AppInstallData;
struct DeviceInfo;

namespace app_install_almanac_endpoint {

// Fetches app installation details from the app install endpoint of the Almanac
// server.
using GetAppInstallInfoCallback =
    base::OnceCallback<void(base::expected<AppInstallData, QueryError>)>;
void GetAppInstallInfo(PackageId package_id,
                       DeviceInfo device_info,
                       network::mojom::URLLoaderFactory& url_loader_factory,
                       GetAppInstallInfoCallback callback);

// Fetches the app install URL from the app install endpoint of the Almanac
// server.
using GetAppInstallUrlCallback =
    base::OnceCallback<void(base::expected<GURL, QueryError>)>;
void GetAppInstallUrl(std::string serialized_package_id,
                      DeviceInfo device_info,
                      network::mojom::URLLoaderFactory& url_loader_factory,
                      GetAppInstallUrlCallback callback);

GURL GetEndpointUrlForTesting();

}  // namespace app_install_almanac_endpoint
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_ALMANAC_ENDPOINT_H_
