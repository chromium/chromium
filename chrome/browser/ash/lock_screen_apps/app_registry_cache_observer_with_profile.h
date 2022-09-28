// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_REGISTRY_CACHE_OBSERVER_WITH_PROFILE_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_REGISTRY_CACHE_OBSERVER_WITH_PROFILE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace apps {

class AppUpdate;

// Observes an AppRegistryCache and holds an associated profile to pass on to
// the delegate on observed calls, so the delegate can differentiate calls from
// multiple profiles.
class AppRegistryCacheObserverWithProfile
    : public apps::AppRegistryCache::Observer {
 public:
  class Delegate {
   public:
    // Forwards calls from `apps::AppRegistryCache::Observer` with an additional
    // `profile`.
    virtual void OnAppUpdate(const apps::AppUpdate& update,
                             Profile* profile) = 0;
  };

  // Delegate must outlive this.
  explicit AppRegistryCacheObserverWithProfile(Delegate* observer,
                                               Profile* profile);
  ~AppRegistryCacheObserverWithProfile() override;

  AppRegistryCacheObserverWithProfile(
      const AppRegistryCacheObserverWithProfile&) = delete;
  AppRegistryCacheObserverWithProfile& operator=(
      const AppRegistryCacheObserverWithProfile&) = delete;

  // apps::AppRegistryCache::Observer
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  const base::raw_ptr<Delegate> delegate_;
  const base::raw_ptr<Profile> profile_;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_observation_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_REGISTRY_CACHE_OBSERVER_WITH_PROFILE_H_
