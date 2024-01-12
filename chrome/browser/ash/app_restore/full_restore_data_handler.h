// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace ash::full_restore {

// The FullRestoreDataHandler class observes AppRegistryCache to remove the app
// launching and app windows when the app is removed.
class FullRestoreDataHandler : public apps::AppRegistryCache::Observer {
 public:
  explicit FullRestoreDataHandler(Profile* profile);
  ~FullRestoreDataHandler() override;

  FullRestoreDataHandler(const FullRestoreDataHandler&) = delete;
  FullRestoreDataHandler& operator=(const FullRestoreDataHandler&) = delete;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<FullRestoreDataHandler> weak_ptr_factory_{this};
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_
