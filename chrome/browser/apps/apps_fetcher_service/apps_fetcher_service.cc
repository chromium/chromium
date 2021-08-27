// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_service.h"

#include <utility>

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_manager.h"

namespace apps {

AppsFetcherService::AppsFetcherService(Profile* profile)
    : apps_fetcher_manager_(std::make_unique<AppsFetcherManager>()) {}

AppsFetcherService::~AppsFetcherService() = default;

void AppsFetcherService::GetApps(const ResultType& result_type,
                                 ResultCallback callback) {
  apps_fetcher_manager_->GetApps(result_type, std::move(callback));
}

}  // namespace apps
