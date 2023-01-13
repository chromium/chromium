// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/apps/app_preload_service/almanac_api_util.h"
#include "chrome/browser/apps/app_preload_service/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Endpoint for requesting app preload data on the ChromeOS Almanac API.
constexpr char kAppPreloadAlmanacEndpoint[] =
    "v1/app_provisioning/apps?alt=proto";

// Maximum accepted size of an APS Response. 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

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

apps::proto::AppProvisioningListAppsRequest::UserType
ConvertStringUserTypeToProto(const std::string& user_type) {
  if (user_type == apps::kUserTypeUnmanaged) {
    return apps::proto::AppProvisioningListAppsRequest::USERTYPE_UNMANAGED;
  } else if (user_type == apps::kUserTypeManaged) {
    return apps::proto::AppProvisioningListAppsRequest::USERTYPE_MANAGED;
  } else if (user_type == apps::kUserTypeChild) {
    return apps::proto::AppProvisioningListAppsRequest::USERTYPE_CHILD;
  } else if (user_type == apps::kUserTypeGuest) {
    return apps::proto::AppProvisioningListAppsRequest::USERTYPE_GUEST;
  }
  return apps::proto::AppProvisioningListAppsRequest::USERTYPE_UNKNOWN;
}

apps::proto::AppProvisioningListAppsRequest::Channel ConvertChannelTypeToProto(
    const version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
      return apps::proto::AppProvisioningListAppsRequest::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return apps::proto::AppProvisioningListAppsRequest::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return apps::proto::AppProvisioningListAppsRequest::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return apps::proto::AppProvisioningListAppsRequest::CHANNEL_STABLE;
    case version_info::Channel::UNKNOWN:
      return apps::proto::AppProvisioningListAppsRequest::CHANNEL_UNDEFINED;
  }
}

std::string BuildGetAppsForFirstLoginRequestBody(const apps::DeviceInfo& info) {
  apps::proto::AppProvisioningListAppsRequest request_proto;
  request_proto.set_board(info.board);
  request_proto.set_model(info.model);
  request_proto.set_language(info.locale);
  request_proto.set_user_type(ConvertStringUserTypeToProto(info.user_type));
  // TODO(b/258566986): Load the device's real SKU ID.
  request_proto.set_sku_id("unknown");

  request_proto.mutable_chrome_os_version()->set_ash_chrome(
      info.version_info.ash_chrome);
  request_proto.mutable_chrome_os_version()->set_platform(
      info.version_info.platform);
  request_proto.mutable_chrome_os_version()->set_channel(
      ConvertChannelTypeToProto(info.version_info.channel));

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

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kTrafficAnnotation);
  loader_->AttachStringForUpload(
      BuildGetAppsForFirstLoginRequestBody(device_info),
      "application/x-protobuf");
  loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&AppPreloadServerConnector::OnGetAppsForFirstLoginResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSizeInBytes);
}

// static
GURL AppPreloadServerConnector::GetServerUrl() {
  return GURL(base::StrCat({GetAlmanacApiUrl(), kAppPreloadAlmanacEndpoint}));
}

void AppPreloadServerConnector::OnGetAppsForFirstLoginResponse(
    GetInitialAppsCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (loader_->ResponseInfo()) {
    response_code = loader_->ResponseInfo()->headers->response_code();
  }
  const int net_error = loader_->NetError();
  loader_.reset();

  if (net_error == net::Error::ERR_INSUFFICIENT_RESOURCES) {
    LOG(ERROR) << "Network request failed due to insufficent resources.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // HTTP error codes in the 500-599 range represent server errors.
  const bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);
  if (server_error) {
    LOG(ERROR) << "Server error.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  proto::AppProvisioningListAppsResponse response;

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
