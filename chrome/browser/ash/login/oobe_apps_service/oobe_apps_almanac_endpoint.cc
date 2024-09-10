// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_almanac_endpoint.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash::oobe_apps_almanac_endpoint {

namespace {

// Endpoint for requesting personalized recommended apps on the ChromeOS Almanac
// API.
constexpr char kAlmanacOobeAppsEndpoint[] = "v1/oobe";

// Maximum size of the response is 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

constexpr char kServerErrorHistogramName[] =
    "Apps.OobeAppRecommendationsService.ServerResponseCodes";

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "personalized_recommended_apps_download",
        R"(
      semantics {
        sender: "ChromeOS OOBE Recommended Apps Screen"
        description:
          "Sends a request to the Almanac Google server to retrieve "
          "apps and use-cases list."
        trigger:
          "When we display the recommended apps screen for the user "
          "during onboarding."
        internal {
          contacts {
            email: "cros-oobe@google.com"
          }
        }
        user_data: {
          type: HW_OS_INFO
        }
        data: "Device technical specifications (e.g. model)."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-05-13"
      }
      policy {
        cookies_allowed: NO
        setting: "NA"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      }
    )");

// Builds the OOBE List request from the given device info.
std::string BuildRequestBody(const apps::DeviceInfo& info) {
  oobe::proto::OOBEListRequest request_proto;
  *request_proto.mutable_device_context() = info.ToDeviceContext();
  *request_proto.mutable_user_context() = info.ToUserContext();

  return request_proto.SerializeAsString();
}

std::optional<oobe::proto::OOBEListResponse> MakeResponseOptional(
    base::expected<oobe::proto::OOBEListResponse, apps::QueryError>
        query_response) {
  if (query_response.has_value()) {
    return std::move(query_response).value();
  }
  LOG(ERROR) << query_response.error();
  return std::nullopt;
}

}  // namespace

void GetAppsAndUseCases(
    const apps::DeviceInfo& device_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GetAppsCallback callback) {
  QueryAlmanacApi<oobe::proto::OOBEListResponse>(
      url_loader_factory, kTrafficAnnotation, BuildRequestBody(device_info),
      kAlmanacOobeAppsEndpoint, kMaxResponseSizeInBytes,
      kServerErrorHistogramName,
      base::BindOnce(&MakeResponseOptional).Then(std::move(callback)));
}

}  // namespace ash::oobe_apps_almanac_endpoint
