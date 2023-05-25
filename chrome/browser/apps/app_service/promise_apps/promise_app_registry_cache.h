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
#include "chrome/browser/apps/app_service/package_id.h"

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
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // The apps::PromiseAppUpdate argument shouldn't be accessed after
    // OnPromiseAppUpdate returns.
    virtual void OnPromiseAppUpdate(const PromiseAppUpdate& update) {}

    // Called when the PromiseAppRegistryCache object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "cache->RemoveObserver(this)", whether directly or indirectly
    // (e.g. via base::ScopedObservation::Remove or via Observe(nullptr)).
    virtual void OnPromiseAppRegistryCacheWillBeDestroyed(
        PromiseAppRegistryCache* cache) = 0;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // PromiseAppRegistryCache for its entire lifetime, or until the observee
    // (the PromiseAppRegistryCache) is destroyed, whichever comes first.
    explicit Observer(PromiseAppRegistryCache* cache);

    // Use this constructor when the observer |this| wants to observe a
    // PromiseAppRegistryCache for part of its lifetime. It can then call
    // Observe() to start and stop observing.
    Observer();

    ~Observer() override;

    void Observe(PromiseAppRegistryCache* cache);

   private:
    raw_ptr<PromiseAppRegistryCache> cache_ = nullptr;
  };

  PromiseAppRegistryCache();

  PromiseAppRegistryCache(const PromiseAppRegistryCache&) = delete;
  PromiseAppRegistryCache& operator=(const PromiseAppRegistryCache&) = delete;

  ~PromiseAppRegistryCache();

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

  // For testing only. Retrieve a read-only pointer to the promise app with the
  // specified package_id. Returns nullptr if the promise app does not exist. Do
  // not store the pointer as the promise app may be destroyed at any time.
  const PromiseApp* GetPromiseAppForTesting(const PackageId& package_id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_
