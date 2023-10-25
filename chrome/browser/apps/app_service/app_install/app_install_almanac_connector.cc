// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace apps {

namespace {

constexpr char kAlmanacAppInstallEndpoint[] = "v1/app-install";

// TODO(b/307632613): Update annotations.xml and grouping.xml entries once
// update script issues are resolved.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("almanac_app_install", R"(
      semantics {
        sender: "App Install Service"
        description:
          "Sends a request to a Google server to fetch app data for "
          "installation."
        trigger:
          "A request is sent when an app installation is triggered by the user "
          "for apps hosted on Almanac."
        internal: {
          contacts {
            email: "cros-apps-foundation-system@google.com"
          }
        }
        user_data: {
          type: NONE
        }
        data: "Device technical specifications (e.g. model)."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-10-24"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

std::string BuildRequestBody(const DeviceInfo& info,
                             const PackageId& package_id) {
  proto::AppInstallRequest request_proto;
  *request_proto.mutable_device_context() = info.ToDeviceContext();
  *request_proto.mutable_user_context() = info.ToUserContext();
  *request_proto.mutable_package_id() = package_id.ToString();

  return request_proto.SerializeAsString();
}

absl::optional<AppInstallData> ParseAppInstallResponseProto(
    const proto::AppInstallResponse& app_install_response) {
  if (!app_install_response.has_app_instance()) {
    return absl::nullopt;
  }
  const proto::AppInstallResponse_AppInstance& instance =
      app_install_response.app_instance();

  absl::optional<PackageId> package_id =
      PackageId::FromString(instance.package_id());
  if (!package_id.has_value()) {
    return absl::nullopt;
  }

  AppInstallData result(std::move(package_id).value());

  if (!instance.has_name()) {
    return absl::nullopt;
  }
  result.name = instance.name();

  result.description = instance.description();

  for (const proto::AppInstallResponse_Icon& proto_icon : instance.icons()) {
    AppInstallIcon icon{.url = GURL(proto_icon.url()),
                        .width_in_pixels = proto_icon.width_in_pixels(),
                        .mime_type = proto_icon.mime_type(),
                        .is_masking_allowed = proto_icon.is_masking_allowed()};
    if (icon.url.is_valid() && icon.width_in_pixels > 0) {
      result.icons.push_back(std::move(icon));
    }
  }

  if (instance.has_web_extras()) {
    WebAppInstallData& web_app_data =
        result.app_type_data.emplace<WebAppInstallData>();
    web_app_data.manifest_id = GURL(result.package_id.identifier());
    if (!web_app_data.manifest_id.is_valid()) {
      return absl::nullopt;
    }
    web_app_data.document_url = GURL(instance.web_extras().document_url());
    if (!web_app_data.document_url.is_valid()) {
      return absl::nullopt;
    }
    web_app_data.original_manifest_url =
        GURL(instance.web_extras().original_manifest_url());
    if (!web_app_data.original_manifest_url.is_valid()) {
      return absl::nullopt;
    }
    web_app_data.proxied_manifest_url = GURL(instance.web_extras().scs_url());
    if (!web_app_data.proxied_manifest_url.is_valid()) {
      return absl::nullopt;
    }
  } else if (instance.has_android_extras()) {
    result.app_type_data.emplace<AndroidAppInstallData>();
  } else {
    return absl::nullopt;
  }

  return result;
}

}  // namespace

GURL AppInstallAlmanacConnector::GetEndpointUrlForTesting() {
  return GetAlmanacEndpointUrl(kAlmanacAppInstallEndpoint);
}

AppInstallAlmanacConnector::AppInstallAlmanacConnector() = default;

AppInstallAlmanacConnector::~AppInstallAlmanacConnector() = default;

void AppInstallAlmanacConnector::GetAppInstallInfo(
    PackageId package_id,
    DeviceInfo device_info,
    network::mojom::URLLoaderFactory& url_loader_factory,
    GetAppInstallInfoCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> loader = GetAlmanacUrlLoader(
      kTrafficAnnotation, BuildRequestBody(device_info, package_id),
      kAlmanacAppInstallEndpoint);

  // Retain a pointer while keeping the loader alive by std::moving it into the
  // callback.
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      &url_loader_factory,
      base::BindOnce(&AppInstallAlmanacConnector::OnAppInstallResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     std::move(callback)),
      kMaxResponseSizeInBytes);
}

void AppInstallAlmanacConnector::OnAppInstallResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    GetAppInstallInfoCallback callback,
    std::unique_ptr<std::string> response_body) {
  absl::Status error = GetDownloadError(
      loader->NetError(), loader->ResponseInfo(), response_body.get());
  if (!error.ok()) {
    LOG(ERROR) << error.message();
    std::move(callback).Run(absl::nullopt);
    return;
  }

  proto::AppInstallResponse response;
  if (!response.ParseFromString(*response_body)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(ParseAppInstallResponseProto(response));
}

}  // namespace apps
