// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILES_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILES_APP_LAUNCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace crosapi {

// Launches files.app. This can be used to handle initialization timing.
// Also, this handles switching files.app to SWA.
class FilesAppLauncher : public apps::AppRegistryCache::Observer,
                         public apps::InstanceRegistry::Observer {
 public:
  explicit FilesAppLauncher(apps::AppServiceProxy* proxy);
  FilesAppLauncher(const FilesAppLauncher&) = delete;
  FilesAppLauncher& operator=(const FilesAppLauncher&) = delete;
  ~FilesAppLauncher() override;

  // Launches the files app if necessary.
  // If it is already launched, |callback| is immediately called.
  // This should not be called, if there was another invocation of Launch(),
  // and it is not yet completed.
  void Launch(base::OnceClosure callback);

 private:
  // Triggers to launch files.app.
  void LaunchInternal();

  // apps::AppRegistryCache::Observer override.
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // apps::InstanceRegistryCache::Observer override.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  const raw_ptr<apps::AppServiceProxy> proxy_;

  base::OnceClosure callback_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<FilesAppLauncher> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILES_APP_LAUNCHER_H_
