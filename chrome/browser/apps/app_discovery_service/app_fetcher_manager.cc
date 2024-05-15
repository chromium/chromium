// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"
#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

base::CallbackListSubscription AppFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::CallbackListSubscription();
}

void AppFetcher::GetIcon(const std::string& icon_id,
                         int32_t size_hint_in_dip,
                         GetIconCallback callback) {
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(gfx::ImageSkia(),
                          DiscoveryError::kErrorRequestFailed);
}

// static
AppFetcher* AppFetcherManager::g_test_fetcher_ = nullptr;

AppFetcherManager::AppFetcherManager(Profile* profile)
    : profile_(profile),
      recommended_arc_app_fetcher_(
          std::make_unique<RecommendedArcAppFetcher>(profile_)),
      almanac_fetcher_(std::make_unique<AlmanacFetcher>(
          profile_,
          std::make_unique<AlmanacIconCache>(profile->GetProfileKey()))) {}

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
      DCHECK(almanac_fetcher_);
      almanac_fetcher_->GetApps(std::move(callback));
      return;
  }
}

base::CallbackListSubscription AppFetcherManager::RegisterForAppUpdates(
    ResultType result_type,
    RepeatingResultCallback callback) {
  switch (result_type) {
    case ResultType::kRecommendedArcApps:
      NOTREACHED_IN_MIGRATION();
      // |result_type| does not support updates, return an empty
      // CallbackListSubscription.
      return base::CallbackListSubscription();
    case ResultType::kTestType:
      DCHECK(g_test_fetcher_);
      return g_test_fetcher_->RegisterForAppUpdates(std::move(callback));
    case ResultType::kGameSearchCatalog:
      DCHECK(almanac_fetcher_);
      return almanac_fetcher_->RegisterForAppUpdates(std::move(callback));
  }
}

void AppFetcherManager::GetIcon(const std::string& icon_id,
                                int32_t size_hint_in_dip,
                                ResultType result_type,
                                GetIconCallback callback) {
  switch (result_type) {
    case ResultType::kRecommendedArcApps:
      NOTREACHED_IN_MIGRATION();
      std::move(callback).Run(gfx::ImageSkia(),
                              DiscoveryError::kErrorRequestFailed);
      return;
    case ResultType::kTestType:
      NOTREACHED_IN_MIGRATION();
      std::move(callback).Run(gfx::ImageSkia(),
                              DiscoveryError::kErrorRequestFailed);
      return;
    case ResultType::kGameSearchCatalog:
      DCHECK(almanac_fetcher_);
      almanac_fetcher_->GetIcon(icon_id, size_hint_in_dip, std::move(callback));
      return;
  }
}

// static
void AppFetcherManager::SetOverrideFetcherForTesting(AppFetcher* fetcher) {
  g_test_fetcher_ = fetcher;
}

}  // namespace apps
