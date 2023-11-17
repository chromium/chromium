// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  resource_request->headers.SetHeader("X-Goog-Api-Key",
                                      google_apis::GetAPIKey());
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

std::optional<std::string>& GetAlmanacEndpointUrlOverride() {
  static base::NoDestructor<std::optional<std::string>> url_override;
  return *url_override;
}

}  // namespace

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

absl::Status GetDownloadError(
    int net_error,
    const network::mojom::URLResponseHead* response_info,
    const std::string* response_body,
    const absl::optional<std::string>& histogram_name) {
  int response_code = 0;
  if (response_info && response_info->headers) {
    response_code = response_info->headers->response_code();
  }
  if (histogram_name.has_value()) {
    // If there is no response code, there was a net error.
    base::UmaHistogramSparse(*histogram_name,
                             response_code > 0 ? response_code : net_error);
  }
  if (net_error != net::OK) {
    return absl::InternalError(
        base::StrCat({"net error: ", net::ErrorToString(net_error)}));
  }

  if ((response_code >= 200 && response_code < 300) || response_code == 0) {
    if (!response_body) {
      return absl::InternalError("request body is nullptr");
    }
    return absl::OkStatus();
  }

  return absl::InternalError(
      base::StrCat({"HTTP error code: ", base::NumberToString(response_code)}));
}
}  // namespace apps
