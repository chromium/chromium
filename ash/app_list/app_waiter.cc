// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_waiter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "base/check.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace ash {

AppWaiter::AppWaiter(AccountId account_id,
                     LoadedCallback callback,
                     std::string_view app_id)
    : account_id_(account_id), callback_(std::move(callback)), app_id_(app_id) {
  CHECK(!callback_.is_null());
  CHECK(!app_id.empty());

  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);
  if (!cache) {
    registry_observation_.Observe(&apps::AppRegistryCacheWrapper::Get());
    return;
  }

  CacheReady(cache);
}

AppWaiter::~AppWaiter() = default;

void AppWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != app_id_ || update.Name().empty()) {
    return;
  }

  std::move(callback_).Run(update.Name());
  cache_observation_.Reset();
}

void AppWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  cache_observation_.Reset();
}

void AppWaiter::OnAppRegistryCacheAdded(const AccountId& account_id) {
  if (account_id_ != account_id) {
    return;
  }

  registry_observation_.Reset();
  CacheReady(
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_));
}

void AppWaiter::CacheReady(apps::AppRegistryCache* cache) {
  CHECK(cache);

  std::optional<apps::AppUpdate> update = cache->GetAppUpdate(app_id_);
  if (update.has_value() && !update->Name().empty()) {
    std::move(callback_).Run(update->Name());
    return;
  }

  cache_observation_.Observe(cache);
}

}  // namespace ash
