// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace apps {

namespace {

// Endpoint for requesting promise app data on the ChromeOS Almanac API.
constexpr char kPromiseAppAlmanacEndpoint[] = "v1/promise-app/";

// Maximum accepted size of an APS Response. 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("promise_app_service", R"(
      semantics {
        sender: "Promise App Service"
        description:
          "Sends a request to a Google server to get the name and "
          "icon data of an app being installed on the device."
        trigger:
          "A request can be sent when an app starts installing."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "chromeos-apps-foundation-team@google.com"
          }
        }
        user_data {
          type: PROFILE_DATA
        }
        data: "Name of the app platform and the platform-specific ID of the "
          "app package being installed, e.g. android:com.example.myapp"
        last_reviewed: "2023-04-21"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request is enabled by app sync without passphrase. You can"
          "disable this request in the 'Sync and Google services' section"
          "in Settings by either: 1. Going into the 'Manage What You Sync'"
          "settings page and turning off Apps sync; OR 2. In the 'Encryption"
          "Options' settings page, select the option to use a sync passphrase."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

std::optional<PromiseAppWrapper> ConvertPromiseAppResponseProto(
    base::expected<proto::PromiseAppResponse, QueryError> query_response) {
  if (query_response.has_value()) {
    return PromiseAppWrapper(std::move(query_response).value());
  }
  return std::nullopt;
}

}  // namespace

PromiseAppAlmanacConnector::PromiseAppAlmanacConnector(Profile* profile)
    : profile_(profile) {}

PromiseAppAlmanacConnector::~PromiseAppAlmanacConnector() = default;

void PromiseAppAlmanacConnector::GetPromiseAppInfo(
    const PackageId& package_id,
    GetPromiseAppCallback callback) {
  // Ensure that the build uses the Google-internal file containing the
  // official API keys, which are required to make queries to the Almanac.
  if (!google_apis::IsGoogleChromeAPIKeyUsed() &&
      !skip_api_key_check_for_testing_) {
    return;
  }
  if (locale_.empty()) {
    DeviceInfoManager* device_info_manager =
        DeviceInfoManagerFactory::GetForProfile(profile_);
    device_info_manager->GetDeviceInfo(base::BindOnce(
        &PromiseAppAlmanacConnector::SetLocale, weak_ptr_factory_.GetWeakPtr(),
        package_id, std::move(callback)));
  } else {
    GetPromiseAppInfoImpl(package_id, std::move(callback));
  }
}

// static
GURL PromiseAppAlmanacConnector::GetServerUrl() {
  return GetAlmanacEndpointUrl(kPromiseAppAlmanacEndpoint);
}

void PromiseAppAlmanacConnector::SetSkipApiKeyCheckForTesting(
    bool skip_api_key_check) {
  skip_api_key_check_for_testing_ = skip_api_key_check;
}

void PromiseAppAlmanacConnector::GetPromiseAppInfoImpl(
    const PackageId& package_id,
    GetPromiseAppCallback callback) {
  QueryAlmanacApi<proto::PromiseAppResponse>(
      profile_->GetURLLoaderFactory(), kTrafficAnnotation,
      BuildGetPromiseAppRequestBody(package_id), kPromiseAppAlmanacEndpoint,
      kMaxResponseSizeInBytes,
      /*error_histogram_name=*/std::nullopt,
      base::BindOnce(&ConvertPromiseAppResponseProto)
          .Then(std::move(callback)));
}

void PromiseAppAlmanacConnector::SetLocale(const PackageId& package_id,
                                           GetPromiseAppCallback callback,
                                           DeviceInfo device_info) {
  locale_ = device_info.locale;
  GetPromiseAppInfoImpl(package_id, std::move(callback));
}

std::string PromiseAppAlmanacConnector::BuildGetPromiseAppRequestBody(
    const apps::PackageId& package_id) {
  apps::proto::PromiseAppRequest request_proto;
  request_proto.set_language(locale_);
  request_proto.set_package_id(package_id.ToString());
  return request_proto.SerializeAsString();
}

}  // namespace apps
