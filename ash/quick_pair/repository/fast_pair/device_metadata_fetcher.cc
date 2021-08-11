// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/http_fetcher.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace {

// TODO(crbug/1234546): Switch to requesting a binary proto once the server
// supports it.
const char kGetObservedDeviceUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/device/"
    "%d?key=%s&mode=MODE_RELEASE";

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
    : http_fetcher_(std::make_unique<HttpFetcher>(kTrafficAnnotation)) {}

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

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&DeviceMetadataFetcher::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceMetadataFetcher::OnJsonParsed(
    GetObservedDeviceCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (result.error) {
    QP_LOG(WARNING) << "Failed to parse JSON, error=" << *result.error;
    std::move(callback).Run(absl::nullopt);
    return;
  }

  base::Value* device = result.value->FindDictPath("device");
  if (!device) {
    QP_LOG(WARNING) << "No Device in JSON response.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  nearby::fastpair::GetObservedDeviceResponse device_metadata;

  std::string* name = device->FindStringPath("name");
  if (name) {
    device_metadata.mutable_device()->set_name(*name);
  }

  std::string* image_url = device->FindStringPath("imageUrl");
  if (image_url) {
    device_metadata.mutable_device()->set_image_url(*image_url);
  }

  std::string* encoded_image = result.value->FindStringPath("image");
  if (encoded_image) {
    std::string decoded;
    base::Base64Decode(*encoded_image, &decoded);
    device_metadata.set_image(decoded);
  }

  absl::optional<int> ble_tx_power = device->FindIntPath("bleTxPower");
  if (ble_tx_power) {
    device_metadata.mutable_device()->set_ble_tx_power(*ble_tx_power);
  }

  absl::optional<double> trigger_distance =
      device->FindDoublePath("triggerDistance");
  if (trigger_distance) {
    device_metadata.mutable_device()->set_trigger_distance(*trigger_distance);
  }

  std::string* encoded_public_key =
      device->FindStringPath("antiSpoofingKeyPair.publicKey");
  if (encoded_public_key) {
    std::string decoded;
    base::Base64Decode(*encoded_public_key, &decoded);
    device_metadata.mutable_device()
        ->mutable_anti_spoofing_key_pair()
        ->set_public_key(decoded);
  }

  base::Value* true_wireless_images =
      device->FindDictPath("trueWirelessImages");
  if (true_wireless_images) {
    std::string* left_bud_url =
        true_wireless_images->FindStringPath("leftBudUrl");
    if (left_bud_url) {
      device_metadata.mutable_device()
          ->mutable_true_wireless_images()
          ->set_left_bud_url(*left_bud_url);
    }

    std::string* right_bud_url =
        true_wireless_images->FindStringPath("rightBudUrl");
    if (left_bud_url) {
      device_metadata.mutable_device()
          ->mutable_true_wireless_images()
          ->set_right_bud_url(*right_bud_url);
    }

    std::string* case_url = true_wireless_images->FindStringPath("caseUrl");
    if (left_bud_url) {
      device_metadata.mutable_device()
          ->mutable_true_wireless_images()
          ->set_case_url(*case_url);
    }
  }

  std::move(callback).Run(device_metadata);
}

}  // namespace quick_pair
}  // namespace ash
