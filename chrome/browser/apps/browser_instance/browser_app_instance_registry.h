// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_map.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace aura {
class Window;
}

namespace apps {

// Hosted in ash-chrome. Aggregates app events received from two
// |BrowserAppInstanceTracker| objects: one in ash-chrome for SWAs, one in
// lacros-chrome for PWAs (via |BrowserAppInstanceForwarder|).
class BrowserAppInstanceRegistry
    : public BrowserAppInstanceObserver,
      public crosapi::mojom::BrowserAppInstanceRegistry,
      public aura::EnvObserver,
      public aura::WindowObserver,
      public wm::ActivationChangeObserver {
 public:
  explicit BrowserAppInstanceRegistry(
      BrowserAppInstanceTracker& ash_instance_tracker);
  ~BrowserAppInstanceRegistry() override;

  // Get a single app instance by ID (Ash or Lacros).
  const BrowserAppInstance* GetAppInstanceById(base::UnguessableToken id) const;

  // Get a single browser window instance by ID (Ash or Lacros).
  const BrowserWindowInstance* GetBrowserWindowInstanceById(
      base::UnguessableToken id) const;

  // Get all instances of apps that are hosted on |window| (Ash or Lacros).
  const std::set<const BrowserAppInstance*> GetBrowserAppInstancesByWindow(
      const aura::Window* window) const;

  // Get a single instance that corresponds to |window|  (Ash or Lacros).
  const BrowserWindowInstance* GetBrowserWindowInstanceByWindow(
      const aura::Window* window) const;

  // Get a single window by instance ID (Ash or Lacros). Returns a nullptr if
  // instance identified by |id| does not exist.
  aura::Window* GetWindowByInstanceId(const base::UnguessableToken& id) const;

  // Get all instances of lacros browser window instances.
  std::set<const BrowserWindowInstance*> GetLacrosBrowserWindowInstances()
      const;

  template <typename PredicateT>
  const BrowserAppInstance* FindAppInstanceIf(PredicateT predicate) const {
    const BrowserAppInstance* instance =
        FindInstanceIf(lacros_app_instances_, predicate);
    if (instance) {
      return instance;
    }
    instance =
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
    SelectInstances(result, lacros_app_instances_, predicate);
    SelectInstances(result, ash_instance_tracker_->app_tab_instances_,
                    predicate);
    SelectInstances(result, ash_instance_tracker_->app_window_instances_,
                    predicate);
    return result;
  }

  template <typename PredicateT>
  const BrowserWindowInstance* FindWindowInstanceIf(
      PredicateT predicate) const {
    const BrowserWindowInstance* instance =
        FindInstanceIf(lacros_window_instances_, predicate);
    if (instance) {
      return instance;
    }
    return FindInstanceIf(ash_instance_tracker_->window_instances_, predicate);
  }

  template <typename PredicateT>
  std::set<const BrowserWindowInstance*> SelectWindowInstances(
      PredicateT predicate) const {
    std::set<const BrowserWindowInstance*> result;
    SelectInstances(result, lacros_window_instances_, predicate);
    SelectInstances(result, ash_instance_tracker_->window_instances_,
                    predicate);
    return result;
  }

  bool IsAppRunning(const std::string& app_id) const;
  bool IsAshBrowserRunning() const;
  bool IsLacrosBrowserRunning() const;

  // Activate the given instance within its tabstrip (in Ash or Lacros). If the
  // instance lives in its own window, this will have no effect.
  void ActivateTabInstance(const base::UnguessableToken& id);

  // Activate an app or a browser window instance (Ash or Lacros). Does nothing
  // if the instance identified by |id| does not exist.
  void ActivateInstance(const base::UnguessableToken& id);

  // Minimize the window of an app or a browser window instance (Ash or Lacros).
  // Does nothing if the instance identified by |id| does not exist.
  void MinimizeInstance(const base::UnguessableToken& id);

  // Check if an app or a browser window instance is active (Ash or Lacros).
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

  void BindReceiver(
      crosapi::CrosapiId id,
      mojo::PendingReceiver<crosapi::mojom::BrowserAppInstanceRegistry>
          receiver);

  // BrowserAppInstanceObserver overrides (events from Ash):
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override;
  void RegisterController(
      mojo::PendingRemote<crosapi::mojom::BrowserAppInstanceController>
          controller) override;

  // crosapi::mojom::BrowserAppInstanceRegistry overrides (events from Lacros):
  void OnBrowserWindowAdded(apps::BrowserWindowInstanceUpdate update) override;
  void OnBrowserWindowUpdated(
      apps::BrowserWindowInstanceUpdate update) override;
  void OnBrowserWindowRemoved(
      apps::BrowserWindowInstanceUpdate update) override;
  void OnBrowserAppAdded(apps::BrowserAppInstanceUpdate update) override;
  void OnBrowserAppUpdated(apps::BrowserAppInstanceUpdate update) override;
  void OnBrowserAppRemoved(apps::BrowserAppInstanceUpdate update) override;

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  // Buffered Lacros instance events for windows that weren't available yet
  // when events arrived.
  struct WindowEventList;

  // Run the action immediately if the window matching |window_id| is
  // available, otherwise buffer the event until it is.
  void RunOrEnqueueEventForWindow(
      const std::string& window_id,
      base::OnceCallback<void(aura::Window*)> event);

  // Helpers processing of buffered lacros instance events.
  void LacrosWindowInstanceAdded(apps::BrowserWindowInstanceUpdate update,
                                 aura::Window* window);
  void LacrosWindowInstanceUpdated(apps::BrowserWindowInstanceUpdate update,
                                   aura::Window* window);
  void LacrosWindowInstanceRemoved(apps::BrowserWindowInstanceUpdate update,
                                   aura::Window* window);
  void LacrosAppInstanceAddedOrUpdated(apps::BrowserAppInstanceUpdate update,
                                       aura::Window* window);
  void LacrosAppInstanceRemoved(apps::BrowserAppInstanceUpdate update,
                                aura::Window* window);

  void OnControllerDisconnected();

  void MaybeStartActivationObservation(aura::Window* window);

  const raw_ref<BrowserAppInstanceTracker> ash_instance_tracker_;

  // Lacros app instances.
  BrowserAppInstanceMap<base::UnguessableToken, BrowserAppInstance>
      lacros_app_instances_;

  // Lacros browser window instances.
  BrowserAppInstanceMap<base::UnguessableToken, BrowserWindowInstance>
      lacros_window_instances_;

  bool is_activation_observed_ = false;

  // Track all Aura windows belonging to Lacros. This is necessary to
  // synchronise crosapi events and Aura windows matching these events being
  // available.
  std::map<std::string, WindowEventList> window_id_to_event_list_;

  mojo::ReceiverSet<crosapi::mojom::BrowserAppInstanceRegistry,
                    crosapi::CrosapiId>
      receiver_set_;
  mojo::Remote<crosapi::mojom::BrowserAppInstanceController> controller_;

  base::ObserverList<BrowserAppInstanceObserver, true>::Unchecked observers_{
      base::ObserverListPolicy::EXISTING_ONLY};

  base::ScopedObservation<BrowserAppInstanceTracker, BrowserAppInstanceObserver>
      tracker_observation_{this};
  base::ScopedObservation<aura::Env, aura::EnvObserver> aura_env_observation_{
      this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      lacros_window_observations_{this};

  base::WeakPtrFactory<BrowserAppInstanceRegistry> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_REGISTRY_H_
