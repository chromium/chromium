// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_map.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace apps {

// Aggregates app events for SWAs received from the |BrowserAppInstanceTracker|.
class BrowserAppInstanceRegistry
    : public BrowserAppInstanceObserver,
      public wm::ActivationChangeObserver {
 public:
  explicit BrowserAppInstanceRegistry(
      BrowserAppInstanceTracker& ash_instance_tracker);
  ~BrowserAppInstanceRegistry() override;

  // Get a single app instance by ID.
  const BrowserAppInstance* GetAppInstanceById(base::UnguessableToken id) const;

  // Get a single browser window instance by ID.
  const BrowserWindowInstance* GetBrowserWindowInstanceById(
      base::UnguessableToken id) const;

  // Get all instances of apps that are hosted on |window|.
  const std::set<const BrowserAppInstance*> GetBrowserAppInstancesByWindow(
      const aura::Window* window) const;

  // Get a single instance that corresponds to |window|.
  const BrowserWindowInstance* GetBrowserWindowInstanceByWindow(
      const aura::Window* window) const;

  // Get a single window by instance ID.
  // Returns a nullptr if instance identified by |id| does not exist.
  aura::Window* GetWindowByInstanceId(const base::UnguessableToken& id) const;

  template <typename PredicateT>
  const BrowserAppInstance* FindAppInstanceIf(PredicateT predicate) const {
    const BrowserAppInstance* instance =
        FindInstanceIf(ash_instance_tracker_->app_tab_instances_, predicate);
    if (instance) {
      return instance;
    }
    return FindInstanceIf(ash_instance_tracker_->app_window_instances_,
                          predicate);
  }

  template <typename PredicateT>
  std::set<const BrowserAppInstance*> SelectAppInstances(
      PredicateT predicate) const {
    std::set<const BrowserAppInstance*> result;
    SelectInstances(result, ash_instance_tracker_->app_tab_instances_,
                    predicate);
    SelectInstances(result, ash_instance_tracker_->app_window_instances_,
                    predicate);
    return result;
  }

  template <typename PredicateT>
  const BrowserWindowInstance* FindWindowInstanceIf(
      PredicateT predicate) const {
    return FindInstanceIf(ash_instance_tracker_->window_instances_, predicate);
  }

  template <typename PredicateT>
  std::set<const BrowserWindowInstance*> SelectWindowInstances(
      PredicateT predicate) const {
    std::set<const BrowserWindowInstance*> result;
    SelectInstances(result, ash_instance_tracker_->window_instances_,
                    predicate);
    return result;
  }

  bool IsAppRunning(const std::string& app_id) const;
  bool IsAshBrowserRunning() const;

  // Activate the given instance within its tabstrip.
  // If the instance lives in its own window, this will have no effect.
  void ActivateTabInstance(const base::UnguessableToken& id);

  // Activate an app or a browser window instance.
  // Does nothing if the instance identified by |id| does not exist.
  void ActivateInstance(const base::UnguessableToken& id);

  // Minimize the window of an app or a browser window instance.
  // Does nothing if the instance identified by |id| does not exist.
  void MinimizeInstance(const base::UnguessableToken& id);

  // Check if an app or a browser window instance is active.
  // Returns false if the instance identified by |id| does not exist.
  bool IsInstanceActive(const base::UnguessableToken& id) const;

  void AddObserver(BrowserAppInstanceObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BrowserAppInstanceObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Runs notifications for all existing instances.
  void NotifyExistingInstances(BrowserAppInstanceObserver* observer);

  // BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  void MaybeStartActivationObservation(aura::Window* window);

  const raw_ref<BrowserAppInstanceTracker> ash_instance_tracker_;

  bool is_activation_observed_ = false;

  base::ObserverList<BrowserAppInstanceObserver, true>::Unchecked observers_{
      base::ObserverListPolicy::EXISTING_ONLY};

  base::ScopedObservation<BrowserAppInstanceTracker, BrowserAppInstanceObserver>
      tracker_observation_{this};

  base::WeakPtrFactory<BrowserAppInstanceRegistry> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_
