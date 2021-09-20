// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"

#include <utility>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_map.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "components/exo/shell_surface_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"

namespace apps {

// Helper class to track all Aura windows belonging to Lacros. This is necessary
// to synchronise crosapi events and Aura windows matching these events being
// available.
class BrowserAppInstanceRegistry::LacrosWindowObserver
    : public aura::EnvObserver,
      public aura::WindowObserver {
 public:
  LacrosWindowObserver() {
    aura_env_observation_.Observe(aura::Env::GetInstance());
  }

  // Run the action immediately if the window matching |window_id| is
  // available, otherwise buffer the event until it is.
  void RunOrEnqueueEventForWindow(
      const std::string& window_id,
      base::OnceCallback<void(aura::Window*)> event) {
    auto& event_list = window_id_to_event_list_[window_id];
    if (event_list.window) {
      std::move(event).Run(event_list.window);
    } else {
      event_list.events.push_back(std::move(event));
    }
  }

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override {
    if (!crosapi::browser_util::IsLacrosWindow(window)) {
      return;
    }
    lacros_window_observations_.AddObservation(window);
    const std::string* id = exo::GetShellApplicationId(window);
    DCHECK(id);
    auto& event_list = window_id_to_event_list_[*id];
    event_list.window = window;
    // Flush any pending events for the new window.
    for (auto& callback : event_list.events) {
      std::move(callback).Run(window);
    }
    event_list.events.clear();
  }

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override {
    if (!crosapi::browser_util::IsLacrosWindow(window)) {
      return;
    }
    lacros_window_observations_.RemoveObservation(window);
    const std::string* id = exo::GetShellApplicationId(window);
    DCHECK(id);
    DCHECK(base::Contains(window_id_to_event_list_, *id));
    window_id_to_event_list_.erase(*id);
  }

 private:
  // Buffered Lacros instance events for windows that weren't available yet
  // when events arrived.
  struct WindowEventList {
    aura::Window* window{nullptr};
    std::vector<base::OnceCallback<void(aura::Window*)>> events;
  };
  std::map<std::string, WindowEventList> window_id_to_event_list_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> aura_env_observation_{
      this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      lacros_window_observations_{this};
};

BrowserAppInstanceRegistry::BrowserAppInstanceRegistry(
    BrowserAppInstanceTracker& ash_instance_tracker)
    : ash_instance_tracker_(ash_instance_tracker),
      lacros_window_observer_(std::make_unique<LacrosWindowObserver>()) {
  tracker_observation_.Observe(&ash_instance_tracker_);
}

BrowserAppInstanceRegistry::~BrowserAppInstanceRegistry() = default;

std::unique_ptr<BrowserAppInstanceRegistry> BrowserAppInstanceRegistry::Create(
    BrowserAppInstanceTracker* ash_instance_tracker) {
  if (!base::FeatureList::IsEnabled(features::kBrowserAppInstanceTracking)) {
    return nullptr;
  }
  return std::make_unique<BrowserAppInstanceRegistry>(*ash_instance_tracker);
}

std::set<const BrowserAppInstance*>
BrowserAppInstanceRegistry::GetAppInstancesByAppId(
    const std::string& app_id) const {
  auto result = ash_instance_tracker_.GetAppInstancesByAppId(app_id);
  if (result.size() > 0) {
    // Ash and Lacros apps don't share IDs, so return now.
    return result;
  }
  return SelectInstances(lacros_app_instances_,
                         [&app_id](const BrowserAppInstance& instance) {
                           return instance.app_id == app_id;
                         });
}

const BrowserAppInstance*
BrowserAppInstanceRegistry::GetActiveAppInstanceForWindow(
    aura::Window* window) {
  const BrowserAppInstance* instance = FindInstanceIf(
      lacros_app_instances_, [window](const BrowserAppInstance& instance) {
        return instance.window == window && instance.is_web_contents_active;
      });
  if (instance) {
    return instance;
  }
  return ash_instance_tracker_.GetActiveAppInstanceForWindow(window);
}

bool BrowserAppInstanceRegistry::IsAppRunning(const std::string& app_id) const {
  return ash_instance_tracker_.IsAppRunning(app_id) ||
         FindInstanceIf(lacros_app_instances_,
                        [&app_id](const BrowserAppInstance& instance) {
                          return instance.app_id == app_id;
                        }) != nullptr;
}

void BrowserAppInstanceRegistry::BindReceiver(
    crosapi::CrosapiId id,
    mojo::PendingReceiver<crosapi::mojom::BrowserAppInstanceRegistry>
        receiver) {
  receiver_set_.Add(this, std::move(receiver), id);
}

void BrowserAppInstanceRegistry::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserWindowAdded(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserWindowUpdated(
    const apps::BrowserWindowInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserWindowUpdated(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserWindowRemoved(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserAppAdded(
    const BrowserAppInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserAppAdded(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserAppUpdated(
    const BrowserAppInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserAppUpdated(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserAppRemoved(
    const BrowserAppInstance& instance) {
  for (auto& observer : observers_) {
    observer.OnBrowserAppRemoved(instance);
  }
}

void BrowserAppInstanceRegistry::OnBrowserWindowAdded(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceAdded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserWindowUpdated(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserWindowRemoved(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppAdded(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosAppInstanceAdded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppUpdated(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosAppInstanceUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppRemoved(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  lacros_window_observer_->RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosAppInstanceRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::LacrosWindowInstanceAdded(
    apps::BrowserWindowInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto instance_id = update.id;
  auto& instance = AddInstance(
      lacros_window_instances_, instance_id,
      std::make_unique<BrowserWindowInstance>(std::move(update), window));
  for (auto& observer : observers_) {
    observer.OnBrowserWindowAdded(instance);
  }
}

void BrowserAppInstanceRegistry::LacrosWindowInstanceUpdated(
    apps::BrowserWindowInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto* instance = GetInstance(lacros_window_instances_, update.id);
  DCHECK(instance);
  if (instance->MaybeUpdate(update.is_active)) {
    for (auto& observer : observers_) {
      observer.OnBrowserWindowUpdated(*instance);
    }
  }
}

void BrowserAppInstanceRegistry::LacrosWindowInstanceRemoved(
    apps::BrowserWindowInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto instance = PopInstanceIfExists(lacros_window_instances_, update.id);
  DCHECK(instance);
  for (auto& observer : observers_) {
    observer.OnBrowserWindowRemoved(*instance);
  }
}

void BrowserAppInstanceRegistry::LacrosAppInstanceAdded(
    apps::BrowserAppInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto& instance = AddInstance(
      lacros_app_instances_, update.id,
      std::make_unique<BrowserAppInstance>(std::move(update), window));
  for (auto& observer : observers_) {
    observer.OnBrowserAppAdded(instance);
  }
}

void BrowserAppInstanceRegistry::LacrosAppInstanceUpdated(
    apps::BrowserAppInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  BrowserAppInstance* instance = GetInstance(lacros_app_instances_, update.id);
  DCHECK(instance);
  if (instance->MaybeUpdate(window, update.title, update.is_browser_active,
                            update.is_web_contents_active)) {
    for (auto& observer : observers_) {
      observer.OnBrowserAppUpdated(*instance);
    }
  }
}

void BrowserAppInstanceRegistry::LacrosAppInstanceRemoved(
    apps::BrowserAppInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto instance = PopInstanceIfExists(lacros_app_instances_, update.id);
  DCHECK(instance);
  for (auto& observer : observers_) {
    observer.OnBrowserAppRemoved(*instance);
  }
}

}  // namespace apps
