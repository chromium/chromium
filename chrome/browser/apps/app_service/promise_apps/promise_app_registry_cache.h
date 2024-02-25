// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace apps {

struct PromiseApp;
class PromiseAppUpdate;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;
using PromiseAppCacheMap = std::map<PackageId, PromiseAppPtr>;

// A cache that manages and keeps track of all promise apps on the
// system.
class PromiseAppRegistryCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Triggered when a new promise app is registered or an existing promise app
    // is updated in the observed Promise App Registry Cache. `Update` contains
    // information on which promise app has been updated and what changes have
    // been made.
    virtual void OnPromiseAppUpdate(const PromiseAppUpdate& update) {}

    // Called after a promise app gets removed from the cache. It's generally
    // preceded by an app update with a "completed" promise app status.
    // `id` - the promise app ID.
    virtual void OnPromiseAppRemoved(const PackageId& id) {}

    // Called when the PromiseAppRegistryCache object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "cache->RemoveObserver(this)", whether directly or indirectly
    // (e.g. via base::ScopedObservation::Reset)
    virtual void OnPromiseAppRegistryCacheWillBeDestroyed(
        PromiseAppRegistryCache* cache) = 0;
  };

  PromiseAppRegistryCache();

  PromiseAppRegistryCache(const PromiseAppRegistryCache&) = delete;
  PromiseAppRegistryCache& operator=(const PromiseAppRegistryCache&) = delete;

  ~PromiseAppRegistryCache();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Find the promise app with the same package_id as the delta and apply
  // the changes for the fields specified by the delta object. If there is no
  // promise app with a matching package_id, then create a new promise app.
  // This method should only be called publicly through Promise App Service.
  // Alternatively, this may be used to add a promise app to the cache directly
  // in unit tests and bypass any API calls that would have been triggered by
  // Promise App Service.
  void OnPromiseApp(PromiseAppPtr delta);

  // Retrieve a copy of all the registered promise apps.
  std::vector<PromiseAppPtr> GetAllPromiseApps() const;

  // Check that a promise app with `package_id` is registered in the cache.
  bool HasPromiseApp(const PackageId& package_id);

  // Retrieve a read-only pointer to the promise app with the specified
  // package_id. Returns nullptr if the promise app does not exist. Do not store
  // the pointer as the promise app may be destroyed at any time.
  const PromiseApp* GetPromiseApp(const PackageId& package_id) const;

  // Retrieve a read-only pointer to the promise app with the specified
  // string_package_id. Returns nullptr if the promise app does not exist or if
  // `string_package_id` cannot be converted into a legitimate package ID. Do
  // not store the pointer as the promise app may be destroyed at any time.
  const PromiseApp* GetPromiseAppForStringPackageId(
      const std::string& string_package_id) const;

 private:
  // Retrieve the registered promise app with the specified package_id. Returns
  // nullptr if the promise app does not exist.
  PromiseApp* FindPromiseApp(const PackageId& package_id) const;

  apps::PromiseAppCacheMap promise_app_map_;

  base::ObserverList<Observer> observers_;

  // Flag to check whether an update to a promise app is already in progress. We
  // shouldn't have more than one concurrent update to a package_id, e.g. if
  // OnPromiseApp notifies observers and triggers them to call OnPromiseApp
  // again (before the first call to OnPromiseApp completes), we want to prevent
  // overwriting fields.
  bool update_in_progress_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_
