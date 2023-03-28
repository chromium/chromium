// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Endpoint for requesting app preload data on the ChromeOS Almanac API.
constexpr char kAppPreloadAlmanacEndpoint[] = "v1/app-preload?alt=proto";

// Maximum accepted size of an APS Response. 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

constexpr char kServerErrorHistogramName[] =
    "AppPreloadService.ServerResponseCodes";

constexpr char kServerRoundTripTimeForFirstLogin[] =
    "AppPreloadService.ServerRoundTripTimeForFirstLogin";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("app_preload_service", R"(
      semantics {
        sender: "App Preload Service"
        description:
          "Sends a request to a Google server to determine a list of apps to "
          "be installed on the device."
        trigger:
          "A request can be sent when a device is being set up, or after a "
          "device update."
        data: "Device technical specifications (e.g. model)."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

std::string BuildGetAppsForFirstLoginRequestBody(const apps::DeviceInfo& info) {
  apps::proto::AppPreloadListRequest request_proto;
  *request_proto.mutable_device_context() = info.ToDeviceContext();
  *request_proto.mutable_user_context() = info.ToUserContext();

  return request_proto.SerializeAsString();
}

}  // namespace

namespace apps {

AppPreloadServerConnector::AppPreloadServerConnector() = default;

AppPreloadServerConnector::~AppPreloadServerConnector() = default;

void AppPreloadServerConnector::GetAppsForFirstLogin(
    const DeviceInfo& device_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GetInitialAppsCallback callback) {
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
  loader_ptr->AttachStringForUpload(
      BuildGetAppsForFirstLoginRequestBody(device_info),
      "application/x-protobuf");

  loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&AppPreloadServerConnector::OnGetAppsForFirstLoginResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     base::TimeTicks::Now(), std::move(callback)),
      kMaxResponseSizeInBytes);
}

// static
GURL AppPreloadServerConnector::GetServerUrl() {
  return GURL(base::StrCat({GetAlmanacApiUrl(), kAppPreloadAlmanacEndpoint}));
}

void AppPreloadServerConnector::OnGetAppsForFirstLoginResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    base::TimeTicks request_start_time,
    GetInitialAppsCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (loader->ResponseInfo()) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }

  const int net_error = loader->NetError();

  // If there is no response code, there was a net error.
  base::UmaHistogramSparse(kServerErrorHistogramName,
                           response_code > 0 ? response_code : net_error);

  // HTTP error codes in the 500-599 range represent server errors.
  if (net_error != net::OK || (response_code >= 500 && response_code < 600)) {
    LOG(ERROR) << "Server error. "
               << "Response code: " << response_code
               << ". Net error: " << net::ErrorToString(net_error);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  base::UmaHistogramTimes(kServerRoundTripTimeForFirstLogin,
                          base::TimeTicks::Now() - request_start_time);

  proto::AppPreloadListResponse response;

  if (!response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Parsing failed";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::vector<PreloadAppDefinition> apps;
  for (const auto& app : response.apps_to_install()) {
    apps.emplace_back(app);
  }

  std::move(callback).Run(std::move(apps));
}

}  // namespace apps
