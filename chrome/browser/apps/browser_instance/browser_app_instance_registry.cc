// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_map.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace apps {

namespace {

void ActivateWindow(aura::Window* window) {
  wm::GetActivationClient(window->GetRootWindow())->ActivateWindow(window);
}

void MinimizeWindow(aura::Window* window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  widget->Minimize();
}

}  // namespace

BrowserAppInstanceRegistry::BrowserAppInstanceRegistry(
    BrowserAppInstanceTracker& ash_instance_tracker)
    : ash_instance_tracker_(ash_instance_tracker) {
  tracker_observation_.Observe(&*ash_instance_tracker_);
}

BrowserAppInstanceRegistry::~BrowserAppInstanceRegistry() = default;

const BrowserAppInstance* BrowserAppInstanceRegistry::GetAppInstanceById(
    base::UnguessableToken id) const {
  return FindAppInstanceIf(
      [&id](const BrowserAppInstance& instance) { return instance.id == id; });
}

const BrowserWindowInstance*
BrowserAppInstanceRegistry::GetBrowserWindowInstanceById(
    base::UnguessableToken id) const {
  return FindWindowInstanceIf([&id](const BrowserWindowInstance& instance) {
    return instance.id == id;
  });
}

const std::set<const BrowserAppInstance*>
BrowserAppInstanceRegistry::GetBrowserAppInstancesByWindow(
    const aura::Window* window) const {
  return SelectAppInstances([&window](const BrowserAppInstance& instance) {
    return instance.window == window;
  });
}

const BrowserWindowInstance*
BrowserAppInstanceRegistry::GetBrowserWindowInstanceByWindow(
    const aura::Window* window) const {
  return FindWindowInstanceIf([&window](const BrowserWindowInstance& instance) {
    return instance.window == window;
  });
}

aura::Window* BrowserAppInstanceRegistry::GetWindowByInstanceId(
    const base::UnguessableToken& id) const {
  if (const BrowserAppInstance* instance = GetAppInstanceById(id)) {
    return instance->window;
  }

  if (const BrowserWindowInstance* instance =
          GetBrowserWindowInstanceById(id)) {
    return instance->window;
  }

  return nullptr;
}

bool BrowserAppInstanceRegistry::IsAppRunning(const std::string& app_id) const {
  return FindAppInstanceIf([&app_id](const BrowserAppInstance& instance) {
           return instance.app_id == app_id;
         }) != nullptr;
}

bool BrowserAppInstanceRegistry::IsAshBrowserRunning() const {
  return ash_instance_tracker_->window_instances_.size() > 0;
}

void BrowserAppInstanceRegistry::ActivateTabInstance(
    const base::UnguessableToken& id) {
  ash_instance_tracker_->ActivateTabInstance(id);
}

void BrowserAppInstanceRegistry::ActivateInstance(
    const base::UnguessableToken& id) {
  if (const BrowserAppInstance* instance = GetAppInstanceById(id)) {
    ActivateWindow(instance->window);
    ActivateTabInstance(id);
    return;
  }

  if (const BrowserWindowInstance* instance =
          GetBrowserWindowInstanceById(id)) {
    ActivateWindow(instance->window);
  }
}

void BrowserAppInstanceRegistry::MinimizeInstance(
    const base::UnguessableToken& id) {
  if (aura::Window* window = GetWindowByInstanceId(id)) {
    MinimizeWindow(window);
  }
}

bool BrowserAppInstanceRegistry::IsInstanceActive(
    const base::UnguessableToken& id) const {
  if (const BrowserAppInstance* instance = GetAppInstanceById(id)) {
    return instance->is_browser_active() && instance->is_web_contents_active;
  }

  if (const BrowserWindowInstance* instance =
          GetBrowserWindowInstanceById(id)) {
    return instance->is_active();
  }
  return false;
}

void BrowserAppInstanceRegistry::NotifyExistingInstances(
    BrowserAppInstanceObserver* observer) {
  for (const auto& pair : ash_instance_tracker_->window_instances_) {
    observer->OnBrowserWindowAdded(*pair.second);
  }
  for (const auto& pair : ash_instance_tracker_->app_tab_instances_) {
    observer->OnBrowserAppAdded(*pair.second);
  }
  for (const auto& pair : ash_instance_tracker_->app_window_instances_) {
    observer->OnBrowserAppAdded(*pair.second);
  }
}

void BrowserAppInstanceRegistry::MaybeStartActivationObservation(
    aura::Window* window) {
  if (is_activation_observed_) {
    return;
  }

  is_activation_observed_ = true;
  // On Ash Chrome, there is only one `ActivationClient` so as long as `window`
  // is attached to the tree, it serves the purpose of getting the
  // `ActivationClient`.
  // Since `ActivationClient` is destroyed before `BrowserAppInstanceRegistry`,
  // `BrowserAppInstanceRegistry` does not need to remove itself upon
  // destruction.
  wm::ActivationClient* activation_client =
      wm::GetActivationClient(window->GetRootWindow());
  CHECK(activation_client);
  activation_client->AddObserver(this);
}

void BrowserAppInstanceRegistry::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  MaybeStartActivationObservation(instance.window);
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
  MaybeStartActivationObservation(instance.window);
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

void BrowserAppInstanceRegistry::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  std::set<const BrowserAppInstance*> instances =
      GetBrowserAppInstancesByWindow(gained_active);
  for (const auto* instance : instances) {
    OnBrowserAppUpdated(*instance);
  }

  instances = GetBrowserAppInstancesByWindow(lost_active);
  for (const auto* instance : instances) {
    OnBrowserAppUpdated(*instance);
  }

  if (const BrowserWindowInstance* instance =
          GetBrowserWindowInstanceByWindow(gained_active)) {
    OnBrowserWindowUpdated(*instance);
  }

  if (const BrowserWindowInstance* instance =
          GetBrowserWindowInstanceByWindow(lost_active)) {
    OnBrowserWindowUpdated(*instance);
  }
}

}  // namespace apps
