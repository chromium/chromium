// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_connector.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace apps {
namespace {

// Endpoint for requesting Launcher apps on the ChromeOS Almanac API.
constexpr char kAlmanacLauncherAppEndpoint[] = "v1/launcher-app";

// Maximum size of the response is 1MB, current file size is ~650KB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("almanac_launcher_app", R"(
      semantics {
        sender: "Almanac Launcher App"
        description:
          "Sends a request to the Almanac Google server to retrieve game apps. "
          "The data is used in Launcher for displaying app results."
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
        data: "Device technical specifications (e.g. model)."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-08-23"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

// Builds the Launcher App request from the given device info.
std::string BuildRequestBody(const DeviceInfo& info) {
  proto::LauncherAppRequest request_proto;
  *request_proto.mutable_device_context() = info.ToDeviceContext();
  *request_proto.mutable_user_context() = info.ToUserContext();

  return request_proto.SerializeAsString();
}

}  // namespace

LauncherAppAlmanacConnector::LauncherAppAlmanacConnector() = default;
LauncherAppAlmanacConnector::~LauncherAppAlmanacConnector() = default;

void LauncherAppAlmanacConnector::GetApps(
    const DeviceInfo& device_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GetAppsCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      GetAlmanacUrlLoader(kTrafficAnnotation, BuildRequestBody(device_info),
                          kAlmanacLauncherAppEndpoint);

  // Retain a pointer while keeping the loader alive by std::moving it into the
  // callback.
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&LauncherAppAlmanacConnector::OnGetAppsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     std::move(callback)),
      kMaxResponseSizeInBytes);
}

GURL LauncherAppAlmanacConnector::GetServerUrl() {
  return GetAlmanacEndpointUrl(kAlmanacLauncherAppEndpoint);
}

void LauncherAppAlmanacConnector::OnGetAppsResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    GetAppsCallback callback,
    std::unique_ptr<std::string> response_body) {
  absl::Status error = GetDownloadError(
      loader->NetError(), loader->ResponseInfo(), response_body.get());
  if (!error.ok()) {
    LOG(ERROR) << error.message();
    std::move(callback).Run(absl::nullopt);
    return;
  }

  proto::LauncherAppResponse response;
  if (!response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Parsing failed.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace apps
