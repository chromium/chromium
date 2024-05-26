// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_almanac_endpoint.h"

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_mapper.h"
#include "chrome/browser/apps/app_deduplication_service/proto/app_deduplication.pb.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace apps::app_deduplication_almanac_endpoint {

namespace {

// Endpoint for requesting app duplicate data on the ChromeOS Almanac API.
constexpr char kAppDeduplicationAlmanacEndpoint[] = "v1/deduplicate";

// Maximum size of App Deduplication Response is 1MB, current size of file at
// initial launch (v1 of deduplication endpoint is) ~6KB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("deduplication_service", R"(
      semantics {
        sender: "Deduplication Service"
        description:
          "Sends a request to a Google server to retrieve groups of apps "
          "which are duplicates of each other. The data is used on the client "
          "to determine the duplicates of a given app."
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
        last_reviewed: "2023-01-13"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

std::string BuildRequestBody(const DeviceInfo& info) {
  proto::DeduplicateRequest request_proto;
  *request_proto.mutable_device_context() = info.ToDeviceContext();
  *request_proto.mutable_user_context() = info.ToUserContext();

  return request_proto.SerializeAsString();
}

std::optional<proto::DeduplicateData> ConvertDeduplicateResponseProto(
    base::expected<proto::DeduplicateResponse, QueryError> query_response) {
  if (!query_response.has_value()) {
    return std::nullopt;
  }

  // Server should return all duplicate app data and cannot be empty.
  if (query_response.value().app_group_size() == 0) {
    LOG(ERROR) << "Response is empty.";
    return std::nullopt;
  }

  deduplication::AppDeduplicationMapper mapper =
      deduplication::AppDeduplicationMapper();
  std::optional<proto::DeduplicateData> deduplicate_data =
      mapper.ToDeduplicateData(query_response.value());

  if (!deduplicate_data.has_value()) {
    LOG(ERROR) << "Mapping to deduplicate data proto failed.";
    return std::nullopt;
  }

  return deduplicate_data;
}

}  // namespace

void GetDeduplicateAppsFromServer(
    const DeviceInfo& device_info,
    network::mojom::URLLoaderFactory& url_loader_factory,
    GetDeduplicateAppsCallback callback) {
  QueryAlmanacApi<proto::DeduplicateResponse>(
      url_loader_factory, kTrafficAnnotation, BuildRequestBody(device_info),
      kAppDeduplicationAlmanacEndpoint, kMaxResponseSizeInBytes,
      /*error_histogram_name=*/std::nullopt,
      base::BindOnce(&ConvertDeduplicateResponseProto)
          .Then(std::move(callback)));
}

GURL GetServerUrl() {
  return GetAlmanacEndpointUrl(kAppDeduplicationAlmanacEndpoint);
}

}  // namespace apps::app_deduplication_almanac_endpoint
