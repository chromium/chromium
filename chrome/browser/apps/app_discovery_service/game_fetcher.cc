// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"

#include <utility>

#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"

namespace apps {

GameFetcher::GameFetcher() {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
}

GameFetcher::~GameFetcher() = default;

void GameFetcher::GetApps(ResultCallback callback) {
  // TODO(melzhang): Pull data from AppProvisioningDataManager & filter it
  // based on locale and language. Then call here and in OnAppDataUpdated.
  std::move(callback).Run({}, DiscoveryError::kErrorRequestFailed);
}

base::CallbackListSubscription GameFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  return result_callback_list_.Add(std::move(callback));
}

void GameFetcher::OnAppDataUpdated(std::unique_ptr<proto::AppData> app_data) {
  // TODO(melzhang): Implement.
}

}  // namespace apps
