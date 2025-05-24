// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_device_settings/peripherals_app_delegate_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/almanac_app_icon_loader.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/apps/peripherals/proto/peripherals.pb.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace {

// Endpoint for requesting peripherals app info on the ChromeOS Almanac API.
constexpr char kPeripheralsAlmanacEndpoint[] = "v1/peripherals";

// Maximum size of peripherals response is 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("peripherals_companion_app", R"(
      semantics {
        sender: "Input Device Settings"
        description:
          "Retrieves companion app information for supported devices. Given a "
          "device key, Google's servers will return the app information (name, "
          "icon, etc) and an action link that will be used to trigger the app "
          "installation dialog."
        trigger:
          "A request is sent when the user initiates the install in the "
          "Settings app."
        data:
          "A device_key in the format <vid>:<pid> "
          "(where VID = vendor ID and PID = product ID) is "
          "used to specify the device image to fetch."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
              email: "cros-device-enablement@google.com"
          }
        }
        user_data {
          type: DEVICE_ID
        }
        last_reviewed: "2024-06-21"
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

PeripheralsAppDelegateImpl::PeripheralsAppDelegateImpl() = default;
PeripheralsAppDelegateImpl::~PeripheralsAppDelegateImpl() = default;

void PeripheralsAppDelegateImpl::GetCompanionAppInfo(
    const std::string& device_key,
    GetCompanionAppInfoCallback callback) {
  // Ensure that the build uses the Google-internal file containing the
  // official API keys, which are required to make queries to the Almanac.
  if (!google_apis::IsGoogleChromeAPIKeyUsed() && !is_testing_) {
    LOG(ERROR) << "Invalid API key or missing internal file. Cannot make "
                  "queries to Almanac. Device key: "
               << device_key;
    return;
  }

  Profile* active_user_profile = GetActiveUserProfile();

  apps::proto::PeripheralsGetRequest request;
  request.set_device(GetDeviceKeyForMetadataRequest(device_key));

  QueryAlmanacApiWithContext<apps::proto::PeripheralsGetRequest,
                             apps::proto::PeripheralsGetResponse>(
      active_user_profile, kPeripheralsAlmanacEndpoint, request,
      kTrafficAnnotation, kMaxResponseSizeInBytes,
      /*error_histogram_name=*/std::nullopt,
      base::BindOnce(
          &PeripheralsAppDelegateImpl::ConvertPeripheralsResponseProto,
          weak_factory_.GetWeakPtr(), active_user_profile->GetWeakPtr(),
          std::move(callback)));
}

void PeripheralsAppDelegateImpl::ConvertPeripheralsResponseProto(
    base::WeakPtr<Profile> active_user_profile_weak_ptr,
    GetCompanionAppInfoCallback callback,
    base::expected<apps::proto::PeripheralsGetResponse, apps::QueryError>
        query_response) {
  Profile* profile = active_user_profile_weak_ptr.get();
  if (!profile) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!query_response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const auto& response = query_response.value();
  auto package_id = apps::PackageId::FromString(response.package_id());
  if (!package_id.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  mojom::CompanionAppInfo info;
  info.action_link = response.action_link();
  info.app_name = response.name();
  info.package_id = package_id.value().ToString();
  info.state =
      apps_util::GetAppWithPackageId(&*profile, package_id.value()).has_value()
          ? mojom::CompanionAppState::kInstalled
          : mojom::CompanionAppState::kAvailable;

  icon_loader_ = std::make_unique<apps::AlmanacAppIconLoader>(*profile);
  auto icon = response.icon();
  apps::AppInstallIcon app_install_icon{
      .url = GURL(icon.url()),
      .width_in_pixels = icon.width_in_pixels(),
      .mime_type = icon.mime_type(),
      .is_masking_allowed = icon.is_masking_allowed()};
  // Callback execution is not critical if object is deleted before icon load.
  // This should rarely occur as the InputDeviceSettingsController, the primary
  // user of this delegate, is initialized in shell and typically persistent.
  icon_loader_->GetAppIcon(
      app_install_icon.url, app_install_icon.mime_type,
      app_install_icon.is_masking_allowed,
      base::BindOnce(&PeripheralsAppDelegateImpl::OnAppIconLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(info)));
}

void PeripheralsAppDelegateImpl::OnAppIconLoaded(
    GetCompanionAppInfoCallback callback,
    mojom::CompanionAppInfo info,
    apps::IconValuePtr icon_value) {
  icon_loader_.reset();
  if (icon_value) {
    info.icon_url = webui::GetBitmapDataUrl(*icon_value->uncompressed.bitmap());
  }
  std::move(callback).Run(info);
}

Profile* PeripheralsAppDelegateImpl::GetActiveUserProfile() {
  if (is_testing_) {
    return profile_for_testing_;
  }

  return ProfileManager::GetActiveUserProfile();
}

GURL PeripheralsAppDelegateImpl::GetServerUrl() {
  return apps::GetAlmanacEndpointUrl(kPeripheralsAlmanacEndpoint);
}

}  // namespace ash
