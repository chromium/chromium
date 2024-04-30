// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_INSTANCE_REGISTRY_UPDATER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_INSTANCE_REGISTRY_UPDATER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace apps {

class InstanceRegistry;

// Created in ash-chrome. Listens to |BrowserAppInstanceRegistry| events and
// aura::Windows associated with them and updates |InstanceRegistry|.
class InstanceRegistryUpdater : public BrowserAppInstanceObserver,
                                public aura::EnvObserver,
                                public aura::WindowObserver {
 public:
  InstanceRegistryUpdater(
      BrowserAppInstanceRegistry& browser_app_instance_registry,
      InstanceRegistry& instance_registry);
  InstanceRegistryUpdater(const InstanceRegistryUpdater&) = delete;
  InstanceRegistryUpdater& operator=(const InstanceRegistryUpdater&) = delete;
  ~InstanceRegistryUpdater() override;

  // BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(const BrowserWindowInstance& instance) override;
  void OnBrowserWindowUpdated(const BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(const BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const BrowserAppInstance& instance) override;

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  const raw_ref<BrowserAppInstanceRegistry> browser_app_instance_registry_;
  const raw_ref<InstanceRegistry> instance_registry_;

  void OnInstance(const base::UnguessableToken& instance_id,
                  const std::string& app_id,
                  aura::Window* window,
                  InstanceState state);

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  base::ScopedObservation<BrowserAppInstanceRegistry,
                          BrowserAppInstanceObserver>
      browser_app_instance_registry_observation_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_INSTANCE_REGISTRY_UPDATER_H_
