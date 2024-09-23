// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "url/gurl.h"

namespace apps {

namespace {

// Returns a resource request for the specified endpoint for the ChromeOS
// Almanac API.
std::unique_ptr<network::ResourceRequest> GetAlmanacResourceRequest(
    std::string_view endpoint_suffix) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetAlmanacEndpointUrl(endpoint_suffix);
  CHECK(resource_request->url.is_valid());

  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");
  google_apis::AddAPIKeyToRequest(*resource_request, google_apis::GetAPIKey());
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

std::optional<std::string>& GetAlmanacEndpointUrlOverride() {
  static base::NoDestructor<std::optional<std::string>> url_override;
  return *url_override;
}

std::unique_ptr<network::SimpleURLLoader> GetAlmanacUrlLoader(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& response_body,
    std::string_view endpoint_suffix) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(
          GetAlmanacResourceRequest(endpoint_suffix), traffic_annotation);
  loader->AttachStringForUpload(response_body, "application/x-protobuf");
  // Retry requests twice (so, three requests total) if requests fail due to
  // network issues.
  constexpr int kMaxRetries = 2;
  loader->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                       network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  return loader;
}

base::expected<std::string, QueryError> ValidateDownloadedString(
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> error_histogram_name,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }

  if (error_histogram_name.has_value()) {
    // If there is no response code, there was a net error.
    base::UmaHistogramSparse(
        error_histogram_name.value(),
        response_code > 0 ? response_code : loader->NetError());
  }

  if (loader->NetError() != net::OK &&
      loader->NetError() != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    return base::unexpected(QueryError{
        QueryError::kConnectionError,
        base::StrCat({"net error: ", net::ErrorToString(loader->NetError())})});
  }

  if ((response_code >= 200 && response_code < 300) || response_code == 0) {
    if (!response_body) {
      return base::unexpected(
          QueryError{QueryError::kBadResponse, "request body is nullptr"});
    }
    return std::move(*response_body);
  }

  if (response_code >= 400 && response_code < 500) {
    return base::unexpected(
        QueryError{QueryError::kBadRequest,
                   base::StrCat({"HTTP error code: ",
                                 base::NumberToString(response_code)})});
  }

  return base::unexpected(
      QueryError{QueryError::kConnectionError,
                 base::StrCat({"HTTP error code: ",
                               base::NumberToString(response_code)})});
}

}  // namespace

namespace internal {

void QueryAlmanacApiRaw(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::string_view endpoint_suffix,
    int max_response_size,
    std::optional<std::string> error_histogram_name,
    base::OnceCallback<void(base::expected<std::string, QueryError>)> callback,
    const std::string& request_body) {
  std::unique_ptr<network::SimpleURLLoader> loader = apps::GetAlmanacUrlLoader(
      traffic_annotation, request_body, endpoint_suffix);

  // Retain a pointer while keeping the loader alive by std::moving it into the
  // callback.
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&ValidateDownloadedString, std::move(loader),
                     std::move(error_histogram_name))
          .Then(std::move(callback)),
      max_response_size);
}

}  // namespace internal

std::string GetAlmanacApiUrl() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kAlmanacApiUrl)) {
    return command_line->GetSwitchValueASCII(ash::switches::kAlmanacApiUrl);
  }

  return GetAlmanacEndpointUrlOverride().value_or(
      "https://chromeosalmanac-pa.googleapis.com/");
}

GURL GetAlmanacEndpointUrl(std::string_view endpoint_suffix) {
  return GURL(base::StrCat({GetAlmanacApiUrl(), endpoint_suffix}));
}

void SetAlmanacEndpointUrlForTesting(std::optional<std::string> url_override) {
  GetAlmanacEndpointUrlOverride() = std::move(url_override);
}

bool QueryError::operator==(const QueryError& other) const {
  return type == other.type && message == other.message;
}

std::ostream& operator<<(std::ostream& out, const QueryError& error) {
  switch (error.type) {
    case QueryError::kConnectionError:
      out << "Connection error: ";
      break;
    case QueryError::kBadRequest:
      out << "Bad request: ";
      break;
    case QueryError::kBadResponse:
      out << "Bad response: ";
      break;
  }
  out << error.message;
  return out;
}

}  // namespace apps
