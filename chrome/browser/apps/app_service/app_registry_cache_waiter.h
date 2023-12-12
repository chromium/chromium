// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_

#include <string>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "url/gurl.h"

class Profile;

namespace apps {

// Waits for a given AppType to be initialized in App Service.
class AppTypeInitializationWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppTypeInitializationWaiter(Profile* profile, apps::AppType app_type);
  ~AppTypeInitializationWaiter() override;

  // Waits for the app type to be initialized, returns immediately if it is
  // already initialized.
  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const apps::AppType app_type_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

// Waits for an app in the App Registry Cache to match an arbitrary condition.
// See below for specializations for common conditions.
class AppUpdateWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppUpdateWaiter(
      Profile* profile,
      const std::string& app_id,
      base::RepeatingCallback<bool(const apps::AppUpdate&)> condition);
  ~AppUpdateWaiter() override;

  // Waits for the condition to match. Returns immediately if it already
  // matches.
  void Await(const base::Location& location = base::Location::Current());

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  const std::string app_id_;
  base::RepeatingCallback<bool(const apps::AppUpdate&)> condition_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

// Waits for the app's Readiness in the App Service app cache to match the
// expected value.
class AppReadinessWaiter : public AppUpdateWaiter {
 public:
  AppReadinessWaiter(
      Profile* profile,
      const std::string& app_id,
      base::RepeatingCallback<bool(apps::Readiness)> readiness_condition);
  AppReadinessWaiter(Profile* profile,
                     const std::string& app_id,
                     apps::Readiness readiness = apps::Readiness::kReady);
};

// Waits for the web app's scope in the App Service app cache to match the
// expected |scope|.
class WebAppScopeWaiter : public AppUpdateWaiter {
 public:
  WebAppScopeWaiter(Profile* profile, const std::string& app_id, GURL scope);
};

// Waits for the app's window mode in the App Service app cache to match the
// expected |window_mode|.
class AppWindowModeWaiter : public AppUpdateWaiter {
 public:
  AppWindowModeWaiter(Profile* profile,
                      const std::string& app_id,
                      apps::WindowMode window_mode);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_
