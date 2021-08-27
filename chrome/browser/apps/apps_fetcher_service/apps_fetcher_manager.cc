// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_manager.h"

#include <utility>

#include "chrome/browser/apps/apps_fetcher_service/recommended_arc_apps_fetcher.h"

namespace apps {

// Initialise app fetchers in constructor.
AppsFetcherManager::AppsFetcherManager()
    : recommended_arc_apps_fetcher_(
          std::make_unique<RecommendedArcAppsFetcher>()) {}

AppsFetcherManager::~AppsFetcherManager() = default;

void AppsFetcherManager::GetApps(const ResultType& app_list_type,
                                 ResultCallback callback) {
  switch (app_list_type) {
    case ResultType::kRecommendedArcApps:
      recommended_arc_apps_fetcher_->GetApps(std::move(callback));
      return;
  }
}

}  // namespace apps
