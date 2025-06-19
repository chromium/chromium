// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_WAITER_H_
#define ASH_APP_LIST_APP_WAITER_H_

#include <string>
#include <string_view>

#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"

namespace ash {

// Wait for `app_id` to be loaded with a non-empty name for `account_id`.
class AppWaiter : public apps::AppRegistryCache::Observer,
                  public apps::AppRegistryCacheWrapper::Observer {
 public:
  using LoadedCallback = base::OnceCallback<void(std::string name)>;
  AppWaiter(AccountId account_id,
            LoadedCallback callback,
            std::string_view app_id);
  ~AppWaiter() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // apps::AppRegistryCacheWrapper::Observer:
  void OnAppRegistryCacheAdded(const AccountId& account_id) override;

 private:
  void CacheReady(apps::AppRegistryCache* cache);

  const AccountId account_id_;
  LoadedCallback callback_;
  const std::string app_id_;

  base::ScopedObservation<apps::AppRegistryCacheWrapper,
                          apps::AppRegistryCacheWrapper::Observer>
      registry_observation_{this};
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      cache_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_WAITER_H_
