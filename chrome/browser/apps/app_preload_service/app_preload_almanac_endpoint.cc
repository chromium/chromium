// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_almanac_endpoint.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace apps::app_preload_almanac_endpoint {
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
        last_reviewed: "2024-05-03"
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

// Filters entries in LauncherConfig and ShelfConfig to ignore when the
// specified feature is disabled.
bool IsFeatureEnabled(const std::string& name) {
  if (name == kAppPreloadServiceEnableTestApps.name) {
    return base::FeatureList::IsEnabled(kAppPreloadServiceEnableTestApps);
  } else if (!name.empty()) {
    LOG(ERROR) << "Unrecognised feature flag considered disabled: " << name;
    return false;
  }

  return true;
}

// Parses LauncherConfig from `in` and adds a LauncherItemMap entry
// into `out` LauncherOrdering keyed with `folder_name`. Uses single level of
// recursion to parse folders.
void ParseLauncherOrdering(
    const google::protobuf::RepeatedPtrField<
        proto::AppPreloadListResponse_LauncherConfig>& in,
    const std::string& folder_name,
    LauncherOrdering* out,
    bool allow_nested_folders = false) {
  LauncherItemMap item_map;
  for (const auto& item : in) {
    if (!IsFeatureEnabled(item.feature_flag())) {
      continue;
    }
    // All packages are added as keys to item_map with the same data.
    for (const auto& package_id : item.package_id()) {
      if (std::optional<apps::PackageId> parsed =
              apps::PackageId::FromString(package_id)) {
        item_map[*parsed] = LauncherItemData(item.type(), item.order());
      }
    }
    // Add nested child folder.
    if (allow_nested_folders && !item.folder_name().empty()) {
      item_map[item.folder_name()] =
          LauncherItemData(item.type(), item.order());
      ParseLauncherOrdering(item.child_config(), item.folder_name(), out);
    }
  }
  (*out)[folder_name] = std::move(item_map);
}

// Parses ShelfConfig from `in` and stores result in `out` ShelfPinOrdering.
void ParseShelfPinOrdering(const google::protobuf::RepeatedPtrField<
                               proto::AppPreloadListResponse_ShelfConfig>& in,
                           ShelfPinOrdering* out) {
  // ShelfConfig is parsed into a map of PackageId and uint32 order.
  for (const auto& item : in) {
    // Ignore any packages which specify a feature flag which is disabled on
    // this device.
    if (!IsFeatureEnabled(item.feature_flag())) {
      continue;
    }
    // All packages are added as keys to the  map with the same order value.
    for (const auto& package_id : item.package_id()) {
      if (std::optional<apps::PackageId> parsed =
              apps::PackageId::FromString(package_id)) {
        (*out)[*parsed] = item.order();
      }
    }
  }
}

void ConvertAppPreloadListResponseProto(
    base::TimeTicks request_start_time,
    GetInitialAppsCallback callback,
    base::expected<proto::AppPreloadListResponse, QueryError> query_response) {
  LauncherOrdering launcher_ordering;
  ShelfPinOrdering shelf_pin_ordering;

  if (!query_response.has_value()) {
    std::move(callback).Run(std::nullopt, std::move(launcher_ordering),
                            std::move(shelf_pin_ordering));
    return;
  }

  base::UmaHistogramTimes(kServerRoundTripTimeForFirstLogin,
                          base::TimeTicks::Now() - request_start_time);


  std::vector<PreloadAppDefinition> apps;
  for (const auto& app : query_response->apps_to_install()) {
    apps.emplace_back(app);
  }

  std::string empty_root_folder;
  ParseLauncherOrdering(query_response->launcher_config(),
                        empty_root_folder, &launcher_ordering,
                        /*allow_nested_folders=*/true);

  ParseShelfPinOrdering(query_response->shelf_config(),
                        &shelf_pin_ordering);

  std::move(callback).Run(std::move(apps), std::move(launcher_ordering),
                          std::move(shelf_pin_ordering));
}

}  // namespace

void GetAppsForFirstLogin(
    const DeviceInfo& device_info,
    network::mojom::URLLoaderFactory& url_loader_factory,
    GetInitialAppsCallback callback) {
  QueryAlmanacApi<proto::AppPreloadListResponse>(
      url_loader_factory, kTrafficAnnotation,
      BuildGetAppsForFirstLoginRequestBody(device_info),
      kAppPreloadAlmanacEndpoint, kMaxResponseSizeInBytes,
      kServerErrorHistogramName,
      base::BindOnce(&ConvertAppPreloadListResponseProto,
                     base::TimeTicks::Now(), std::move(callback)));
}

GURL GetServerUrl() {
  return GetAlmanacEndpointUrl(kAppPreloadAlmanacEndpoint);
}

}  // namespace apps::app_preload_almanac_endpoint
