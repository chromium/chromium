// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace apps {

// Returns the base URL (scheme, host and port) for the ChromeOS
// Almanac API. This can be overridden with the command-line switch
// --almanac-api-url.
std::string GetAlmanacApiUrl();

// Returns the URL for the specified endpoint for the ChromeOS Almanac
// API. An endpoint suffix is e.g. "v1/app-preload".
GURL GetAlmanacEndpointUrl(std::string_view endpoint_suffix);

// Returns a SimpleURLLoader for the ChromeOS Almanac API created from
// the given parameters. request_body is a proto serialized as string.
// An endpoint suffix is e.g. "v1/app-preload".
std::unique_ptr<network::SimpleURLLoader> GetAlmanacUrlLoader(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& request_body,
    std::string_view endpoint_suffix);

// Checks whether an error occurred during downloading and handles it. Logs the
// error for the given endpoint. Adds the error to UMA if a histogram name is
// specified. Note the response body can be empty even if no other error
// occurred.
bool HasDownloadError(
    int net_error,
    const network::mojom::URLResponseHead* response_info,
    const std::string* response_body,
    std::string_view endpoint,
    const absl::optional<std::string>& histogram_name = absl::nullopt);
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
