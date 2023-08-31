// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

base::CallbackListSubscription AppFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  NOTREACHED();
  return base::CallbackListSubscription();
}

void AppFetcher::GetIcon(const std::string& app_id,
                         int32_t size_hint_in_dip,
                         GetIconCallback callback) {
  NOTREACHED();
  std::move(callback).Run(gfx::ImageSkia(),
                          DiscoveryError::kErrorRequestFailed);
}

// static
AppFetcher* AppFetcherManager::g_test_fetcher_ = nullptr;

AppFetcherManager::AppFetcherManager(Profile* profile)
    : profile_(profile),
      recommended_arc_app_fetcher_(
          std::make_unique<RecommendedArcAppFetcher>(profile_)),
      game_fetcher_(std::make_unique<GameFetcher>(profile_)) {}

AppFetcherManager::~AppFetcherManager() = default;

void AppFetcherManager::GetApps(ResultType result_type,
                                ResultCallback callback) {
  switch (result_type) {
    case ResultType::kTestType:
      DCHECK(g_test_fetcher_);
      g_test_fetcher_->GetApps(std::move(callback));
      return;
    case ResultType::kRecommendedArcApps:
      DCHECK(recommended_arc_app_fetcher_);
      recommended_arc_app_fetcher_->GetApps(std::move(callback));
      return;
    case ResultType::kGameSearchCatalog:
      DCHECK(game_fetcher_);
      game_fetcher_->GetApps(std::move(callback));
      return;
  }
}

base::CallbackListSubscription AppFetcherManager::RegisterForAppUpdates(
    ResultType result_type,
    RepeatingResultCallback callback) {
  switch (result_type) {
    case ResultType::kRecommendedArcApps:
      NOTREACHED();
      // |result_type| does not support updates, return an empty
      // CallbackListSubscription.
      return base::CallbackListSubscription();
    case ResultType::kTestType:
      DCHECK(g_test_fetcher_);
      return g_test_fetcher_->RegisterForAppUpdates(std::move(callback));
    case ResultType::kGameSearchCatalog:
      DCHECK(game_fetcher_);
      return game_fetcher_->RegisterForAppUpdates(std::move(callback));
  }
}

void AppFetcherManager::GetIcon(const std::string& app_id,
                                int32_t size_hint_in_dip,
                                ResultType result_type,
                                GetIconCallback callback) {
  switch (result_type) {
    case ResultType::kRecommendedArcApps:
      NOTREACHED();
      std::move(callback).Run(gfx::ImageSkia(),
                              DiscoveryError::kErrorRequestFailed);
      return;
    case ResultType::kTestType:
      NOTREACHED();
      std::move(callback).Run(gfx::ImageSkia(),
                              DiscoveryError::kErrorRequestFailed);
      return;
    case ResultType::kGameSearchCatalog:
      DCHECK(game_fetcher_);
      game_fetcher_->GetIcon(app_id, size_hint_in_dip, std::move(callback));
      return;
  }
}

// static
void AppFetcherManager::SetOverrideFetcherForTesting(AppFetcher* fetcher) {
  g_test_fetcher_ = fetcher;
}

}  // namespace apps
