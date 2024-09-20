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
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/status/status.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::string_view endpoint_suffix,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<std::string, QueryError>)> callback,
    const std::string& request_body);

template <typename T>
base::expected<T, QueryError> ParseResponse(
    base::expected<std::string, QueryError> string_response) {
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
}

}  // namespace internal

// Sends a network fetch to the `endpoint_suffix` of the Almanac server, with a
// client context attached, and calls `callback` with the result.
//
// `partial_request` is a instance of a RequestProto message for an Almanac
// endpoint which has all non-context fields filled. This method will attach a
// device and user context, serialize the proto, and attach it to the network
// request. RequestProto must have fields for the device_context and
// user_context, e.g.:
//
//   optional ClientDeviceContext device_context = 1;
//   optional ClientUserContext user_context = 2;
//
// Any response from the server will be parsed into a ResponseProto and
// returned. If any HTTP or other failure occurs, a QueryError is returned
// instead.
//
// Adds HTTP response codes to UMA if an `error_histogram_name` is specified.
// The histogram must be defined in a histograms.xml file using enum
// CombinedHttpResponseAndNetErrorCode.
template <typename RequestProto, typename ResponseProto>
void QueryAlmanacApiWithContext(
    Profile* profile,
    std::string_view endpoint_suffix,
    RequestProto partial_request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<ResponseProto, QueryError>)>
        callback) {
  DeviceInfoManager* manager = DeviceInfoManagerFactory::GetForProfile(profile);
  if (!manager) {
    std::move(callback).Run(base::unexpected(
        QueryError{QueryError::kBadRequest,
                   "Unavailable for incognito and system profiles"}));
  }

  base::OnceCallback<void(const std::string&)> query_with_request_body =
      base::BindOnce(&internal::QueryAlmanacApiRaw,
                     profile->GetURLLoaderFactory(), traffic_annotation,
                     endpoint_suffix, max_response_size, error_histogram_name,
                     base::BindOnce(&internal::ParseResponse<ResponseProto>)
                         .Then(std::move(callback)));

  manager->GetDeviceInfo(base::BindOnce(
                             [](RequestProto partial_request, DeviceInfo info) {
                               *partial_request.mutable_device_context() =
                                   info.ToDeviceContext();
                               *partial_request.mutable_user_context() =
                                   info.ToUserContext();
                               return partial_request.SerializeAsString();
                             },
                             std::move(partial_request))
                             .Then(std::move(query_with_request_body)));
}

// Sends a network fetch to the `endpoint_suffix` of the Almanac server with
// `request_body` and returns the T proto response or QueryError if there was a
// failure.
// Adds HTTP response codes to UMA if a histogram name is specified. The
// histogram must be defined in apps/histograms.xml using enum
// CombinedHttpResponseAndNetErrorCode.
template <typename T>
void QueryAlmanacApi(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& request_body,
    std::string_view endpoint_suffix,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<T, QueryError>)> callback) {
  internal::QueryAlmanacApiRaw(
      url_loader_factory, traffic_annotation, endpoint_suffix,
      max_response_size, error_histogram_name,
      base::BindOnce(&internal::ParseResponse<T>).Then(std::move(callback)),
      request_body);
}

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_API_UTIL_H_
