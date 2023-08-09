// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

namespace network {
class SimpleURLLoader;
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
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
