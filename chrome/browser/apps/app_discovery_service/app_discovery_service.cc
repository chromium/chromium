// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

namespace apps {

AppDiscoveryService::AppDiscoveryService(Profile* profile)
    : app_fetcher_manager_(std::make_unique<AppFetcherManager>(profile)) {}

AppDiscoveryService::~AppDiscoveryService() = default;

void AppDiscoveryService::GetApps(ResultType result_type,
                                  ResultCallback callback) {
  app_fetcher_manager_->GetApps(result_type, std::move(callback));
}

base::CallbackListSubscription AppDiscoveryService::RegisterForAppUpdates(
    ResultType result_type,
    RepeatingResultCallback callback) {
  return app_fetcher_manager_->RegisterForAppUpdates(result_type,
                                                     std::move(callback));
}

}  // namespace apps
