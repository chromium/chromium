// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_ALMANAC_ENDPOINT_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_ALMANAC_ENDPOINT_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

class GURL;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

struct DeviceInfo;

namespace app_preload_almanac_endpoint {

using GetInitialAppsCallback =
    base::OnceCallback<void(std::optional<std::vector<PreloadAppDefinition>>,
                            LauncherOrdering,
                            ShelfPinOrdering)>;

// Fetches a list of apps to be installed on the device at first login from
// the App Provisioning Service API. `callback` will be called with a list of
// (possibly zero) apps, or `std::nullopt` if an error occurred while
// fetching apps.
void GetAppsForFirstLogin(const DeviceInfo& device_info,
                          network::mojom::URLLoaderFactory& url_loader_factory,
                          GetInitialAppsCallback callback);

// Gets the URL for the App Provisioning Service endpoint. Exposed for tests.
GURL GetServerUrl();

}  // namespace app_preload_almanac_endpoint
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_ALMANAC_ENDPOINT_H_
