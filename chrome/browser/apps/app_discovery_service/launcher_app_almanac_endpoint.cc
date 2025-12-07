// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_endpoint.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace apps::launcher_app_almanac_endpoint {
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
          type: HW_OS_INFO
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

std::optional<proto::LauncherAppResponse> MakeResponseOptional(
    base::expected<proto::LauncherAppResponse, QueryError> query_response) {
  if (query_response.has_value()) {
    return std::move(query_response).value();
  }
  return std::nullopt;
}

}  // namespace

void GetApps(Profile* profile, GetAppsCallback callback) {
  proto::LauncherAppRequest request_proto;

  QueryAlmanacApiWithContext<proto::LauncherAppRequest,
                             proto::LauncherAppResponse>(
      profile, kAlmanacLauncherAppEndpoint, request_proto, kTrafficAnnotation,
      kMaxResponseSizeInBytes,
      /*error_histogram_name=*/std::nullopt,
      base::BindOnce(&MakeResponseOptional).Then(std::move(callback)));
}

GURL GetServerUrl() {
  return GetAlmanacEndpointUrl(kAlmanacLauncherAppEndpoint);
}

}  // namespace apps::launcher_app_almanac_endpoint
