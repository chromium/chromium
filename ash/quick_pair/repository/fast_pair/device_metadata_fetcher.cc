// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/unauthenticated_http_fetcher.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "components/cross_device/logging/logging.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace {

const char kGetObservedDeviceUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/device/"
    "%d?key=%s&mode=%s&alt=proto";
const char kReleaseMode[] = "MODE_RELEASE";
const char kDebugMode[] = "MODE_DEBUG";

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fast_pair_device_metadata_fetcher", R"(
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
          setting:
            "You can enable or disable this feature by toggling on/off the "
            "Fast Pair toggle in chrome://os-settings under 'Bluetooth'. The "
            "feature is enabled by default. "
          chrome_policy {
            FastPairEnabled {
                FastPairEnabled: false
            }
          }
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
  const char* mode;
  if (features::IsFastPairDebugMetadataEnabled()) {
    CD_LOG(INFO, Feature::FP) << __func__ << ": Fetching DEBUG_MODE metadata.";
    mode = kDebugMode;
  } else {
    mode = kReleaseMode;
  }

  GURL url = GURL(base::StringPrintf(kGetObservedDeviceUrl, id,
                                     google_apis::GetAPIKey().c_str(), mode));

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
    std::unique_ptr<std::string> response_body,
    std::unique_ptr<FastPairHttpResult> http_result) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": HTTP result: "
      << (http_result ? http_result->ToString() : "[null]");

  if (!http_result) {
    CD_LOG(WARNING, Feature::FP) << __func__ << "Unable to make request.";
    std::move(callback).Run(std::nullopt, /*has_retryable_error=*/true);
    return;
  }

  RecordDeviceMetadataFetchResult(*http_result);

  if (!response_body) {
    CD_LOG(WARNING, Feature::FP) << "No response.";
    // Only suggest retrying when the actual request failed, otherwise there is
    // no matching metadata for the given model_id.
    std::move(callback).Run(std::nullopt,
                            /*has_retryable_error=*/!http_result->IsSuccess());
    return;
  }

  nearby::fastpair::GetObservedDeviceResponse device_metadata;
  if (!device_metadata.ParseFromString(*response_body)) {
    CD_LOG(WARNING, Feature::FP) << "Failed to parse.";
    std::move(callback).Run(std::nullopt, /*has_retryable_error=*/true);
    return;
  }

  std::move(callback).Run(device_metadata, /*has_retryable_error=*/false);
}

}  // namespace quick_pair
}  // namespace ash
