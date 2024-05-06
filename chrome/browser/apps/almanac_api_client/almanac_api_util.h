// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/status/status.h"

class GURL;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace apps {

// Returns the base URL (scheme, host and port) for the ChromeOS
// Almanac API. This can be overridden with the command-line switch
// --almanac-api-url.
std::string GetAlmanacApiUrl();

// Returns the URL for the specified endpoint for the ChromeOS Almanac
// API. An endpoint suffix is e.g. "v1/app-preload".
GURL GetAlmanacEndpointUrl(std::string_view endpoint_suffix);

// Overrides the Almanac endpoint URL for testing.
void SetAlmanacEndpointUrlForTesting(std::optional<std::string> url_override);

struct QueryError {
  enum Type {
    kConnectionError,
    kBadRequest,
    kBadResponse,
  } type;
  std::string message;

  bool operator==(const QueryError& other) const;
};

std::ostream& operator<<(std::ostream& out, const QueryError& error);

namespace internal {

void QueryAlmanacApiRaw(
    network::mojom::URLLoaderFactory& url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& request_body,
    std::string_view endpoint_suffix,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<std::string, QueryError>)> callback);

}  // namespace internal

// Sends a network fetch to the `endpoint_suffix` of the Almanac server with
// `request_body` and returns the T proto response or QueryError if there was a
// failure.
// Adds HTTP response codes to UMA if a histogram name is specified. The
// histogram must be defined in apps/histograms.xml using enum
// CombinedHttpResponseAndNetErrorCode.
template <typename T>
void QueryAlmanacApi(
    network::mojom::URLLoaderFactory& url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& request_body,
    std::string_view endpoint_suffix,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<T, QueryError>)> callback) {
  internal::QueryAlmanacApiRaw(
      url_loader_factory, traffic_annotation, request_body, endpoint_suffix,
      max_response_size, error_histogram_name,
      base::BindOnce([](base::expected<std::string, QueryError> string_response)
                         -> base::expected<T, QueryError> {
        if (!string_response.has_value()) {
          return base::unexpected(std::move(string_response).error());
        }

        T result;
        if (result.ParseFromString(*string_response)) {
          return base::ok(result);
        }

        return base::unexpected(QueryError{
            QueryError::kBadResponse,
            "Parsing failed",
        });
      }).Then(std::move(callback)));
}

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
