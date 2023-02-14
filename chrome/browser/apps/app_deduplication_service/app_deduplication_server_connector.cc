// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_server_connector.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_mapper.h"
#include "chrome/browser/apps/app_deduplication_service/proto/app_deduplication.pb.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Endpoint for requesting app duplicate data on the ChromeOS Almanac API.
constexpr char kAppDeduplicationOnePlatformEndpoint[] =
    "https://chromeosalmanac-pa.googleapis.com/v1/deduplicate";

// Maximum size of App Deduplication Response is 1MB, current size of file at
// initial launch (v1 of deduplication endpoint is) ~6KB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("deduplication_service", R"(
      semantics {
        sender: "Deduplication Service"
        description:
          "Sends a request to a Google server to retrieve groups of apps "
          "which are duplicates of each other. The data is used on the client "
          "to determine the duplicates of a given app."
        trigger:
          "A request is sent when the user logs in and has not made a request "
          "to the server in over 24 hours."
        internal {
          contacts {
            email: "cros-apps-foundation-system@google.com"
          }
        }
        user_data: {
          type: NONE
        }
        data: "Empty request."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-01-13"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

}  // namespace

namespace apps {

AppDeduplicationServerConnector::AppDeduplicationServerConnector() = default;

AppDeduplicationServerConnector::~AppDeduplicationServerConnector() = default;

void AppDeduplicationServerConnector::GetDeduplicateAppsFromServer(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GetDeduplicateAppsCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetServerUrl();
  DCHECK(resource_request->url.is_valid());

  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");
  resource_request->headers.SetHeader("X-Goog-Api-Key",
                                      google_apis::GetAPIKey());

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  auto* loader_ptr = loader.get();
  // Server expects an empty request.
  loader_ptr->AttachStringForUpload("", "application/x-protobuf");
  loader_ptr->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_5XX |
             network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(
          &AppDeduplicationServerConnector::OnGetDeduplicateAppsResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(loader),
          std::move(callback)),
      kMaxResponseSizeInBytes);
}

GURL AppDeduplicationServerConnector::GetServerUrl() {
  return GURL(kAppDeduplicationOnePlatformEndpoint);
}

void AppDeduplicationServerConnector::OnGetDeduplicateAppsResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    GetDeduplicateAppsCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (loader->ResponseInfo()) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }

  const int net_error = loader->NetError();
  if (net_error == net::Error::ERR_INSUFFICIENT_RESOURCES) {
    LOG(ERROR) << "Network request failed due to insufficent resources.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // HTTP error codes in the 500-599 range represent server errors.
  const bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);
  if (server_error) {
    LOG(ERROR) << "Server error. "
               << "Response code: " << response_code
               << ". Net error: " << net::ErrorToString(net_error);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  proto::DeduplicateResponse response;

  if (!response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Parsing failed.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // Server should return all duplicate app data and cannot be empty.
  if (response.app_group_size() == 0) {
    LOG(ERROR) << "Response is empty.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  deduplication::AppDeduplicationMapper mapper =
      deduplication::AppDeduplicationMapper();
  absl::optional<proto::DeduplicateData> deduplicate_data =
      mapper.ToDeduplicateData(response);

  if (!deduplicate_data.has_value()) {
    LOG(ERROR) << "Mapping to deduplicate data proto failed.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(std::move(deduplicate_data));
}

}  // namespace apps
