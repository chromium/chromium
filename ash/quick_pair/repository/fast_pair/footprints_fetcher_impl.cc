// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/footprints_fetcher_impl.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/oauth_http_fetcher.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "components/cross_device/logging/logging.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

namespace {

const char kUserDevicesUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/userdevices"
    "?key=%s&alt=proto";

const char kUserDeleteDeviceUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/userdevices/%s"
    "?key=%s&alt=proto";

const net::PartialNetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("fast_pair_footprints_request",
                                               "oauth2_api_call_flow",
                                               R"(
      semantics {
          sender: "Fast Pair repository access"
        description:
            "Retrieves and updates details about bluetooth devices which have "
            "been paired with a user's account."
        trigger:
          "This request is sent at the start of a session."
        data: "List of paired devices with metadata."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature by toggling on/off the "
            "Fast Pair toggle in chrome://os-settings under 'Bluetooth'. The "
            "feature is enabled by default. Fast Pair does not fetch data from "
            "the repository if the user is not signed in."
          chrome_policy {
            FastPairEnabled {
                FastPairEnabled: true
            }
          }
        })");

std::unique_ptr<HttpFetcher> CreateHttpFetcher() {
  return std::make_unique<OAuthHttpFetcher>(
      kTrafficAnnotation, GaiaConstants::kNearbyDevicesOAuth2Scope);
}

GURL GetUserDevicesUrl() {
  return GURL(
      base::StringPrintf(kUserDevicesUrl, google_apis::GetAPIKey().c_str()));
}

GURL GetUserDeleteDeviceUrl(const std::string& hex_account_key) {
  return GURL(base::StringPrintf(kUserDeleteDeviceUrl, hex_account_key.c_str(),
                                 google_apis::GetAPIKey().c_str()));
}

}  // namespace

FootprintsFetcherImpl::FootprintsFetcherImpl() = default;
FootprintsFetcherImpl::~FootprintsFetcherImpl() = default;

void FootprintsFetcherImpl::GetUserDevices(UserReadDevicesCallback callback) {
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecuteGetRequest(
      GetUserDevicesUrl(),
      base::BindOnce(&FootprintsFetcherImpl::OnGetComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcherImpl::OnGetComplete(
    UserReadDevicesCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body,
    std::unique_ptr<FastPairHttpResult> http_result) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": HTTP result: "
      << (http_result ? http_result->ToString() : "[null]");

  if (http_result)
    RecordFootprintsFetcherGetResult(*http_result);

  if (!response_body) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": No response.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  nearby::fastpair::UserReadDevicesResponse devices;
  if (!devices.ParseFromString(*response_body)) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": Failed to parse.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": Successfully retrived footprints data.  Paired devices:";
  for (const auto& info : devices.fast_pair_info()) {
    if (info.has_device()) {
      nearby::fastpair::StoredDiscoveryItem device;
      if (device.ParseFromString(info.device().discovery_item_bytes())) {
        CD_LOG(VERBOSE, Feature::FP) << device.title();
      }
    }
  }

  std::move(callback).Run(devices);
}

void FootprintsFetcherImpl::AddUserFastPairInfo(
    nearby::fastpair::FastPairInfo info,
    AddDeviceCallback callback) {
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecutePostRequest(
      GetUserDevicesUrl(), info.SerializeAsString(),
      base::BindOnce(&FootprintsFetcherImpl::OnPostComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcherImpl::OnPostComplete(
    AddDeviceCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body,
    std::unique_ptr<FastPairHttpResult> http_result) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": HTTP result: "
      << (http_result ? http_result->ToString() : "[null]");

  if (http_result)
    RecordFootprintsFetcherPostResult(*http_result);

  if (!response_body) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": No response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Successfully saved new footprints data.";
  std::move(callback).Run(/*success=*/true);
}

void FootprintsFetcherImpl::DeleteUserDevice(const std::string& hex_account_key,
                                             DeleteDeviceCallback callback) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << " Deleting user device for acc key " << hex_account_key;
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecuteDeleteRequest(
      GetUserDeleteDeviceUrl(hex_account_key),
      base::BindOnce(&FootprintsFetcherImpl::OnDeleteComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcherImpl::OnDeleteComplete(
    DeleteDeviceCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body,
    std::unique_ptr<FastPairHttpResult> http_result) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": HTTP result: "
      << (http_result ? http_result->ToString() : "[null]");

  if (http_result)
    RecordFootprintsFetcherDeleteResult(*http_result);

  if (!response_body) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": No response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Successfully deleted footprints data.";
  std::move(callback).Run(/*success=*/true);
}

}  // namespace quick_pair
}  // namespace ash
