// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"

namespace apps {

AppFetcherManager::AppFetcherManager()
    : recommended_arc_app_fetcher_(
          std::make_unique<RecommendedArcAppFetcher>()) {}

AppFetcherManager::~AppFetcherManager() = default;

void AppFetcherManager::GetApps(const ResultType& app_list_type,
                                ResultCallback callback) {
  switch (app_list_type) {
    case ResultType::kRecommendedArcApps:
      recommended_arc_app_fetcher_->GetApps(std::move(callback));
      return;
  }
}

}  // namespace apps
