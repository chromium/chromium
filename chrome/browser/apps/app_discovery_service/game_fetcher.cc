// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

GameFetcher::GameFetcher(Profile* profile) : profile_(profile) {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
}

GameFetcher::~GameFetcher() = default;

void GameFetcher::GetApps(ResultCallback callback) {
  auto error = last_results_.empty() ? DiscoveryError::kErrorRequestFailed
                                     : DiscoveryError::kSuccess;
  std::move(callback).Run(last_results_, error);
}

base::CallbackListSubscription GameFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  return result_callback_list_.Add(std::move(callback));
}

void GameFetcher::OnAppDataUpdated(const proto::AppWithLocaleList& app_data) {
  last_results_ = GetAppsForCurrentLocale(app_data);
  result_callback_list_.Notify(last_results_);
}

std::vector<Result> GameFetcher::GetAppsForCurrentLocale(
    const proto::AppWithLocaleList& app_data) {
  std::vector<Result> results;
  for (const auto& app_with_locale : app_data.app_with_locale()) {
    // TODO(melzhang) : Filter based on locale.
    auto extras = std::make_unique<GameExtras>(
        absl::nullopt, GameExtras::Source::kTestSource, GURL());
    // TODO(melzhang) : Pass in locale specific title.
    results.push_back(Result(AppSource::kGames,
                             app_with_locale.app().app_id_for_platform(),
                             u"TempAppTitle", std::move(extras)));
  }
  return results;
}

}  // namespace apps
