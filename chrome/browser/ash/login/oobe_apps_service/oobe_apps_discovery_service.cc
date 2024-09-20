// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"

#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_almanac_endpoint.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_types.h"

namespace ash {

OobeAppsDiscoveryService::OobeAppsDiscoveryService(Profile* profile)
    : profile_(profile) {}

OobeAppsDiscoveryService::~OobeAppsDiscoveryService() = default;

void OobeAppsDiscoveryService::GetAppsAndUseCases(
    ResultCallbackAppsAndUseCases callback) {
  // The whole feature would not work unless the build includes the Google
  // Chrome API key.
  if (!google_apis::IsGoogleChromeAPIKeyUsed()) {
    PropagateResult(std::move(callback), AppsFetchingResult::kErrorMissingKey);
    return;
  }
  if (!(apps_list_.empty()) && !(use_cases_.empty())) {
    PropagateResult(std::move(callback), AppsFetchingResult::kSuccess);
  } else {
    CHECK(callback_.is_null());
    callback_ = std::move(callback);
    DownloadAppsAndUseCases();
  }
}

void OobeAppsDiscoveryService::PropagateResult(
    ResultCallbackAppsAndUseCases callback,
    AppsFetchingResult result) {
  std::move(callback).Run(apps_list_, use_cases_, result);
}

void OobeAppsDiscoveryService::DownloadAppsAndUseCases() {
  oobe_apps_almanac_endpoint::GetAppsAndUseCases(
      profile_, base::BindOnce(&OobeAppsDiscoveryService::OnServerResponse,
                               weak_factory_.GetWeakPtr()));
}

void OobeAppsDiscoveryService::OnServerResponse(
    std::optional<oobe::proto::OOBEListResponse> response) {
  if (!response.has_value()) {
    PropagateResult(std::move(callback_),
                    AppsFetchingResult::kErrorRequestFailed);
    return;
  }
  for (oobe::proto::OOBEListResponse::App app_info : response->apps()) {
    apps_list_.emplace_back(std::move(app_info));
  }
  for (oobe::proto::OOBEListResponse::Tag usecase : response->tags()) {
    use_cases_.emplace_back(std::move(usecase));
  }
  std::sort(use_cases_.begin(), use_cases_.end());
  PropagateResult(std::move(callback_), AppsFetchingResult::kSuccess);
}

}  // namespace ash
