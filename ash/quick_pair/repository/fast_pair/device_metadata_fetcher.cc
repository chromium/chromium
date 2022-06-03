// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/unauthenticated_http_fetcher.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace {

const char kGetObservedDeviceUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/device/"
    "%d?key=%s&mode=MODE_RELEASE&alt=proto";

// TODO(crbug/1226117): Update annotation with policy details when available.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fast_pair", R"(
        semantics {
          sender: "Fast Pair repository access"
          description:
            "Retrieves details about bluetooth devices that have been "
            "discovered in range."
          trigger: "Eligible bluetooth device is in range."
          data:
            "Details about a bluetooth peripheral, including name and picture."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "There is a toggle in OS Settings under Bluetooth."
          policy_exception_justification:
            "Not yet created, feature disabled by flag"
        })");

}  // namespace

namespace ash {
namespace quick_pair {

DeviceMetadataFetcher::DeviceMetadataFetcher()
    : http_fetcher_(
          std::make_unique<UnauthenticatedHttpFetcher>(kTrafficAnnotation)) {}

DeviceMetadataFetcher::DeviceMetadataFetcher(
    std::unique_ptr<HttpFetcher> http_fetcher)
    : http_fetcher_(std::move(http_fetcher)) {}

DeviceMetadataFetcher::~DeviceMetadataFetcher() = default;

void DeviceMetadataFetcher::LookupDeviceId(int id,
                                           GetObservedDeviceCallback callback) {
  GURL url = GURL(base::StringPrintf(kGetObservedDeviceUrl, id,
                                     google_apis::GetAPIKey().c_str()));
  http_fetcher_->ExecuteGetRequest(
      url, base::BindOnce(&DeviceMetadataFetcher::OnFetchComplete,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void DeviceMetadataFetcher::LookupHexDeviceId(
    const std::string& hex_id,
    GetObservedDeviceCallback callback) {
  int id = std::strtol(hex_id.c_str(), nullptr, 16);
  LookupDeviceId(id, std::move(callback));
}

void DeviceMetadataFetcher::OnFetchComplete(
    GetObservedDeviceCallback callback,
    std::unique_ptr<std::string> response_body) {
  QP_LOG(VERBOSE) << __func__;

  if (!response_body) {
    QP_LOG(WARNING) << "No response.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  nearby::fastpair::GetObservedDeviceResponse device_metadata;
  if (!device_metadata.ParseFromString(*response_body)) {
    QP_LOG(WARNING) << "Failed to parse.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(device_metadata);
}

}  // namespace quick_pair
}  // namespace ash
