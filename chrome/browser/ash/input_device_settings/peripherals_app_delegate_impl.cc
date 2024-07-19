// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_device_settings/peripherals_app_delegate_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/apps/peripherals/proto/peripherals.pb.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

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
              email: "cros-peripherals@google.com"
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

// Creates an example ClientDeviceContext that is needed to form a well
// structured request to the Almanac endpoint.
static apps::proto::ClientDeviceContext GetExampleClientContext() {
  apps::proto::ClientDeviceContext device_context;
  device_context.set_board("board");
  device_context.set_model("model");
  device_context.set_channel(apps::proto::ClientDeviceContext::CHANNEL_DEV);
  device_context.mutable_versions()->set_chrome_ash("124.0.12345.1");
  device_context.mutable_versions()->set_chrome_os_platform("12345.0.1");
  device_context.set_hardware_id("hardware_id");
  return device_context;
}

// Creates an example ClientUserContext that is needed to form a request to the
// Almanac endpoint. The data does not matter, the format just needs to be
// right for Almanac to accept the request.
static apps::proto::ClientUserContext GetExampleClientUserContext() {
  apps::proto::ClientUserContext user_context;
  user_context.set_language("en_US");
  user_context.set_user_type(apps::proto::ClientUserContext::USERTYPE_MANAGED);
  return user_context;
}

std::string BuildRequestBody(const std::string& device_key) {
  apps::proto::PeripheralsGetRequest peripherals_proto;

  *peripherals_proto.mutable_device_context() = GetExampleClientContext();
  *peripherals_proto.mutable_user_context() = GetExampleClientUserContext();
  *peripherals_proto.mutable_device() =
      GetDeviceKeyForMetadataRequest(device_key);
  return peripherals_proto.SerializeAsString();
}

std::optional<mojom::CompanionAppInfo> ConvertPeripheralsResponseProto(
    base::expected<apps::proto::PeripheralsGetResponse, apps::QueryError>
        query_response) {
  if (!query_response.has_value()) {
    return std::nullopt;
  }

  const apps::proto::PeripheralsGetResponse& response = query_response.value();
  mojom::CompanionAppInfo info;
  info.action_link = response.action_link();
  info.app_name = response.name();
  info.icon_url = response.icon().url();
  auto package_id = apps::PackageId::FromString(response.package_id());
  info.package_id = package_id.value().ToString();
  info.state = apps_util::GetAppWithPackageId(
                   &*ProfileManager::GetActiveUserProfile(), package_id.value())
                       .has_value()
                   ? mojom::CompanionAppState::kInstalled
                   : mojom::CompanionAppState::kAvailable;
  return info;
}

}  // namespace

PeripheralsAppDelegateImpl::PeripheralsAppDelegateImpl() = default;
PeripheralsAppDelegateImpl::~PeripheralsAppDelegateImpl() = default;

void PeripheralsAppDelegateImpl::GetCompanionAppInfo(
    const std::string& device_key,
    base::OnceCallback<void(const std::optional<mojom::CompanionAppInfo>&)>
        callback) {
  QueryAlmanacApi<apps::proto::PeripheralsGetResponse>(
      *ProfileManager::GetActiveUserProfile()->GetURLLoaderFactory().get(),
      kTrafficAnnotation, BuildRequestBody(device_key),
      kPeripheralsAlmanacEndpoint, kMaxResponseSizeInBytes,
      /*error_histogram_name=*/std::nullopt,
      base::BindOnce(&ConvertPeripheralsResponseProto)
          .Then(std::move(callback)));
}
}  // namespace ash
