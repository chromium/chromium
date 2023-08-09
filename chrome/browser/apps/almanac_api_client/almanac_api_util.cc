// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
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

}  // namespace

std::string GetAlmanacApiUrl() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kAlmanacApiUrl)) {
    return command_line->GetSwitchValueASCII(ash::switches::kAlmanacApiUrl);
  }

  return "https://chromeosalmanac-pa.googleapis.com/";
}

GURL GetAlmanacEndpointUrl(std::string_view endpoint_suffix) {
  return GURL(base::StrCat({GetAlmanacApiUrl(), endpoint_suffix}));
}

std::unique_ptr<network::SimpleURLLoader> GetAlmanacUrlLoader(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& request_body,
    std::string_view endpoint_suffix) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(
          GetAlmanacResourceRequest(endpoint_suffix), traffic_annotation);
  loader->AttachStringForUpload(request_body, "application/x-protobuf");
  // Retry requests twice (so, three requests total) if requests fail due to
  // network issues.
  constexpr int kMaxRetries = 2;
  loader->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                       network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  return loader;
}
}  // namespace apps
