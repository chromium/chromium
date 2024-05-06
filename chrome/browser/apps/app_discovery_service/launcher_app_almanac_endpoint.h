// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_ENDPOINT_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_ENDPOINT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"

class GURL;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

struct DeviceInfo;

namespace launcher_app_almanac_endpoint {

using GetAppsCallback =
    base::OnceCallback<void(std::optional<proto::LauncherAppResponse>)>;

// Fetches a list of apps from the Launcher App endpoint in the Almanac server.
void GetApps(const DeviceInfo& device_info,
             network::mojom::URLLoaderFactory& url_loader_factory,
             GetAppsCallback callback);

// Returns the GURL for the endpoint. Exposed for tests.
GURL GetServerUrl();

}  // namespace launcher_app_almanac_endpoint
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_LAUNCHER_APP_ALMANAC_ENDPOINT_H_
