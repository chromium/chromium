// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

// static
AppFetcher* AppFetcherManager::g_test_fetcher_ = nullptr;

AppFetcherManager::AppFetcherManager(Profile* profile)
    : profile_(profile),
      recommended_arc_app_fetcher_(
          std::make_unique<RecommendedArcAppFetcher>(profile_)) {}

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
  }
}

// static
void AppFetcherManager::SetOverrideFetcherForTesting(AppFetcher* fetcher) {
  g_test_fetcher_ = fetcher;
}

}  // namespace apps
