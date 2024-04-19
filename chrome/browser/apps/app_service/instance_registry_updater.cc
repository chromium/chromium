// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/instance_registry_updater.h"

#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "ui/aura/window.h"

namespace apps {

namespace {

static InstanceState GetState(bool visible, bool active) {
  auto state = static_cast<InstanceState>(InstanceState::kStarted |
                                          InstanceState::kRunning);
  if (visible) {
    state = static_cast<InstanceState>(state | InstanceState::kVisible);
  }
  if (active) {
    // If the app is active, it should be started, running, and visible.
    state = static_cast<InstanceState>(state | InstanceState::kVisible |
                                       InstanceState::kActive);
  }
  return state;
}

bool IsAshBrowserWindow(aura::Window* aura_window) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    BrowserWindow* window = browser->window();
    if (window && window->GetNativeWindow() == aura_window) {
      return true;
    }
  }
  return false;
}

}  // namespace

InstanceRegistryUpdater::InstanceRegistryUpdater(
    BrowserAppInstanceRegistry& browser_app_instance_registry,
    InstanceRegistry& instance_registry)
    : browser_app_instance_registry_(browser_app_instance_registry),
      instance_registry_(instance_registry) {
  if (aura::Env::HasInstance()) {
    env_observer_.Observe(aura::Env::GetInstance());
  }
  browser_app_instance_registry_observation_.Observe(
      &*browser_app_instance_registry_);
}

InstanceRegistryUpdater::~InstanceRegistryUpdater() = default;

void InstanceRegistryUpdater::OnBrowserWindowAdded(
    const BrowserWindowInstance& instance) {
  OnBrowserWindowUpdated(instance);
}

void InstanceRegistryUpdater::OnBrowserWindowUpdated(
    const BrowserWindowInstance& instance) {
  InstanceState state =
      GetState(instance.window->IsVisible(), instance.is_active());
  OnInstance(instance.id, instance.GetAppId(), instance.window, state);
}

void InstanceRegistryUpdater::OnBrowserWindowRemoved(
    const BrowserWindowInstance& instance) {
  OnInstance(instance.id, instance.GetAppId(), instance.window,
             InstanceState::kDestroyed);
}

void InstanceRegistryUpdater::OnBrowserAppAdded(
    const BrowserAppInstance& instance) {
  OnBrowserAppUpdated(instance);
}

void InstanceRegistryUpdater::OnBrowserAppUpdated(
    const BrowserAppInstance& instance) {
  InstanceState state =
      GetState(instance.window->IsVisible(),
               instance.is_browser_active() && instance.is_web_contents_active);
  OnInstance(instance.id, instance.app_id, instance.window, state);
}

void InstanceRegistryUpdater::OnBrowserAppRemoved(
    const BrowserAppInstance& instance) {
  OnInstance(instance.id, instance.app_id, instance.window,
             InstanceState::kDestroyed);
}

void InstanceRegistryUpdater::OnWindowInitialized(aura::Window* window) {
  window_observations_.AddObservation(window);
}

void InstanceRegistryUpdater::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  if (!crosapi::browser_util::IsLacrosWindow(window) &&
      !IsAshBrowserWindow(window)) {
    return;
  }
  for (const BrowserAppInstance* instance :
       browser_app_instance_registry_->SelectAppInstances(
           [window](const BrowserAppInstance& instance) {
             return instance.window == window;
           })) {
    OnBrowserAppUpdated(*instance);
  }
  for (const BrowserWindowInstance* instance :
       browser_app_instance_registry_->SelectWindowInstances(
           [window](const BrowserWindowInstance& instance) {
             return instance.window == window;
           })) {
    OnBrowserWindowUpdated(*instance);
  }
}

void InstanceRegistryUpdater::OnWindowDestroying(aura::Window* window) {
  window_observations_.RemoveObservation(window);
}

void InstanceRegistryUpdater::OnInstance(
    const base::UnguessableToken& instance_id,
    const std::string& app_id,
    aura::Window* window,
    InstanceState state) {
  auto instance = std::make_unique<apps::Instance>(app_id, instance_id, window);
  instance->UpdateState(state, base::Time::Now());
  instance_registry_->OnInstance(std::move(instance));
}

}  // namespace apps
