// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

namespace apps {

AppDiscoveryService::AppDiscoveryService(Profile* profile)
    : app_fetcher_manager_(std::make_unique<AppFetcherManager>(profile)) {}

AppDiscoveryService::~AppDiscoveryService() = default;

base::CallbackListSubscription AppDiscoveryService::RegisterForAppUpdates(
    ResultType result_type,
    RepeatingResultCallback callback) {
  return app_fetcher_manager_->RegisterForAppUpdates(result_type,
                                                     std::move(callback));
}

void AppDiscoveryService::GetApps(ResultType result_type,
                                  ResultCallback callback) {
  app_fetcher_manager_->GetApps(result_type, std::move(callback));
}

void AppDiscoveryService::GetIcon(const std::string& icon_id,
                                  int32_t size_hint_in_dip,
                                  ResultType result_type,
                                  GetIconCallback callback) {
  app_fetcher_manager_->GetIcon(icon_id, size_hint_in_dip, result_type,
                                std::move(callback));
}

}  // namespace apps
