// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_ALMANAC_ENDPOINT_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_ALMANAC_ENDPOINT_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"

class GURL;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

struct DeviceInfo;

namespace app_deduplication_almanac_endpoint {

using GetDeduplicateAppsCallback =
    base::OnceCallback<void(std::optional<proto::DeduplicateData>)>;

// Fetches a list of duplicate app groups from the App Deduplication Service
// endpoint in the Almanac server.
void GetDeduplicateAppsFromServer(
    const DeviceInfo& device_info,
    network::mojom::URLLoaderFactory& url_loader_factory,
    GetDeduplicateAppsCallback callback);

// Returns the GURL for the App Deduplication Service endpoint. Exposed for
// tests.
GURL GetServerUrl();

}  // namespace app_deduplication_almanac_endpoint
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_ALMANAC_ENDPOINT_H_
