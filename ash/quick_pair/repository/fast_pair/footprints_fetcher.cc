// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/footprints_fetcher.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/oauth_http_fetcher.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

namespace {

const char kUserDevicesUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/user/devices"
    "?key=%s&alt=proto";

const char kUserDeleteDeviceUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/user/devices/%s"
    "?key=%s&alt=proto";

// TODO(crbug/1226117): Update annotation with policy details when available.
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
          setting: "There is a toggle in OS Settings under Bluetooth."
          policy_exception_justification:
            "Not yet created, feature disabled by flag"
      })");

std::unique_ptr<HttpFetcher> CreateHttpFetcher() {
  return std::make_unique<OAuthHttpFetcher>(
      kTrafficAnnotation, GaiaConstants::kCloudPlatformProjectsOAuth2Scope);
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

FootprintsFetcher::FootprintsFetcher() = default;
FootprintsFetcher::~FootprintsFetcher() = default;

void FootprintsFetcher::GetUserDevices(UserReadDevicesCallback callback) {
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecuteGetRequest(
      GetUserDevicesUrl(),
      base::BindOnce(&FootprintsFetcher::OnGetComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcher::OnGetComplete(
    UserReadDevicesCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body) {
  QP_LOG(VERBOSE) << __func__;

  if (!response_body) {
    QP_LOG(WARNING) << __func__ << ": No response.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  nearby::fastpair::UserReadDevicesResponse devices;
  if (!devices.ParseFromString(*response_body)) {
    QP_LOG(WARNING) << __func__ << ": Failed to parse.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  QP_LOG(VERBOSE)
      << __func__
      << ": Successfully retrived footprints data.  Paired devices:";
  for (const auto& info : devices.fast_pair_info()) {
    if (info.has_device()) {
      nearby::fastpair::StoredDiscoveryItem device;
      if (device.ParseFromString(info.device().discovery_item_bytes())) {
        QP_LOG(VERBOSE) << device.title();
      }
    }
  }

  std::move(callback).Run(devices);
}

void FootprintsFetcher::AddUserDevice(nearby::fastpair::FastPairInfo info,
                                      AddDeviceCallback callback) {
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecutePostRequest(
      GetUserDevicesUrl(), info.SerializeAsString(),
      base::BindOnce(&FootprintsFetcher::OnPostComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcher::OnPostComplete(
    AddDeviceCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body) {
  QP_LOG(VERBOSE) << __func__;

  if (!response_body) {
    QP_LOG(WARNING) << __func__ << ": No response.";
    std::move(callback).Run(false);
    return;
  }

  QP_LOG(VERBOSE) << __func__ << ": Successfully saved new footprints data.";
  std::move(callback).Run(true);
}

void FootprintsFetcher::DeleteUserDevice(const std::string& hex_account_key,
                                         DeleteDeviceCallback callback) {
  auto http_fetcher = CreateHttpFetcher();
  auto* raw_http_fetcher = http_fetcher.get();
  raw_http_fetcher->ExecuteDeleteRequest(
      GetUserDeleteDeviceUrl(hex_account_key),
      base::BindOnce(&FootprintsFetcher::OnPostComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(http_fetcher)));
}

void FootprintsFetcher::OnDeleteComplete(
    DeleteDeviceCallback callback,
    std::unique_ptr<HttpFetcher> http_fetcher,
    std::unique_ptr<std::string> response_body) {
  QP_LOG(VERBOSE) << __func__;

  if (!response_body) {
    QP_LOG(WARNING) << __func__ << ": No response.";
    std::move(callback).Run(false);
    return;
  }

  QP_LOG(VERBOSE) << __func__ << ": Successfully deleted footprints data.";
  std::move(callback).Run(true);
}

}  // namespace quick_pair
}  // namespace ash
