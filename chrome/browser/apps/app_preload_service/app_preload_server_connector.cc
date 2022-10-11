// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/apps/app_preload_service/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// TODO(b/249427934): Temporary test data.
static constexpr char kServerUrl[] =
    "http://localhost:9876/v1/app_provisioning/apps?alt=proto";

// TODO(b/244500232): Temporary placeholder value. To be updated once server
// design is completed. Maximum accepted size of an APS Response. 1MB.
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

std::string BuildGetAppsForFirstLoginRequestBody(const apps::DeviceInfo& info) {
  base::Value::Dict request;
  request.Set("board", info.board);
  request.Set("model", info.model);
  request.Set("language", info.locale);

  base::Value::Dict versions;
  versions.Set("ash_chrome", info.version_info.ash_chrome);
  versions.Set("platform", info.version_info.platform);
  request.Set("chrome_os_version", std::move(versions));

  std::string request_body;
  base::JSONWriter::Write(request, &request_body);
  return request_body;
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

  resource_request->url = GURL(kServerUrl);
  DCHECK(resource_request->url.is_valid());

  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kTrafficAnnotation);
  loader_->AttachStringForUpload(
      BuildGetAppsForFirstLoginRequestBody(device_info), "application/json");
  loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&AppPreloadServerConnector::OnGetAppsForFirstLoginResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSizeInBytes);
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

  // TODO(b/249646015): Pass error states to the caller to handle.
  if (net_error == net::Error::ERR_INSUFFICIENT_RESOURCES) {
    LOG(ERROR) << "Network request failed due to insufficent resources.";
    std::move(callback).Run({});
    return;
  }

  // HTTP error codes in the 500-599 range represent server errors.
  const bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);
  if (server_error || response_body->empty()) {
    LOG(ERROR) << "Server error.";
    std::move(callback).Run({});
    return;
  }

  proto::AppProvisioningResponse response;

  if (!response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Parsing failed";
    std::move(callback).Run(std::vector<PreloadAppDefinition>());
    return;
  }

  std::vector<PreloadAppDefinition> apps;
  for (const auto& app : response.apps_to_install()) {
    apps.emplace_back(app);
  }

  std::move(callback).Run(std::move(apps));
}

}  // namespace apps
