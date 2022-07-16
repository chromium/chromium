// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/app_discovery_features.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_fetcher.h"

namespace apps {

// static
AppFetcher* AppFetcherManager::g_test_fetcher_ = nullptr;

AppFetcherManager::AppFetcherManager(Profile* profile)
    : recommended_arc_app_fetcher_(
          std::make_unique<RecommendedArcAppFetcher>()),
      remote_url_fetcher_(std::make_unique<RemoteUrlFetcher>(profile)) {}

AppFetcherManager::~AppFetcherManager() = default;

void AppFetcherManager::GetApps(ResultType result_type,
                                ResultCallback callback) {
  if (g_test_fetcher_) {
    g_test_fetcher_->GetApps(std::move(callback));
    return;
  }

  switch (result_type) {
    case ResultType::kRecommendedArcApps:
      recommended_arc_app_fetcher_->GetApps(std::move(callback));
      return;
    case ResultType::kRemoteUrlSearch:
      remote_url_fetcher_->GetApps(std::move(callback));
      return;
  }
}

// static
void AppFetcherManager::SetOverrideFetcherForTesting(AppFetcher* fetcher) {
  g_test_fetcher_ = fetcher;
}

}  // namespace apps
