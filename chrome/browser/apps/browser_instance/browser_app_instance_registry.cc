// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_map.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/browser_window.h"
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

struct BrowserAppInstanceRegistry::WindowEventList {
  raw_ptr<aura::Window> window{nullptr};
  std::vector<base::OnceCallback<void(aura::Window*)>> events;
};

BrowserAppInstanceRegistry::BrowserAppInstanceRegistry(
    BrowserAppInstanceTracker& ash_instance_tracker)
    : ash_instance_tracker_(ash_instance_tracker) {
  tracker_observation_.Observe(&*ash_instance_tracker_);
  aura_env_observation_.Observe(aura::Env::GetInstance());
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

std::set<const BrowserWindowInstance*>
BrowserAppInstanceRegistry::GetLacrosBrowserWindowInstances() const {
  std::set<const BrowserWindowInstance*> result;
  for (const auto& pair : lacros_window_instances_) {
    result.insert(pair.second.get());
  }
  return result;
}

bool BrowserAppInstanceRegistry::IsAppRunning(const std::string& app_id) const {
  return FindAppInstanceIf([&app_id](const BrowserAppInstance& instance) {
           return instance.app_id == app_id;
         }) != nullptr;
}

bool BrowserAppInstanceRegistry::IsAshBrowserRunning() const {
  return ash_instance_tracker_->window_instances_.size() > 0;
}

bool BrowserAppInstanceRegistry::IsLacrosBrowserRunning() const {
  return lacros_window_instances_.size() > 0;
}

void BrowserAppInstanceRegistry::ActivateTabInstance(
    const base::UnguessableToken& id) {
  if (lacros_app_instances_.find(id) != lacros_app_instances_.end()) {
    if (controller_.is_bound()) {
      controller_->ActivateTabInstance(id);
    }
  } else {
    ash_instance_tracker_->ActivateTabInstance(id);
  }
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
  for (const auto& pair : lacros_window_instances_) {
    observer->OnBrowserWindowAdded(*pair.second);
  }
  for (const auto& pair : lacros_app_instances_) {
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

void BrowserAppInstanceRegistry::BindReceiver(
    crosapi::CrosapiId id,
    mojo::PendingReceiver<crosapi::mojom::BrowserAppInstanceRegistry>
        receiver) {
  receiver_set_.Add(this, std::move(receiver), id);
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

void BrowserAppInstanceRegistry::RegisterController(
    mojo::PendingRemote<crosapi::mojom::BrowserAppInstanceController>
        controller) {
  // At the moment only a single controller is supported.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (controller_.is_bound()) {
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(
      base::BindOnce(&BrowserAppInstanceRegistry::OnControllerDisconnected,
                     base::Unretained(this)));
}

void BrowserAppInstanceRegistry::OnBrowserWindowAdded(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceAdded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserWindowUpdated(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserWindowRemoved(
    apps::BrowserWindowInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosWindowInstanceRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppAdded(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(
          &BrowserAppInstanceRegistry::LacrosAppInstanceAddedOrUpdated,
          weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppUpdated(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(
          &BrowserAppInstanceRegistry::LacrosAppInstanceAddedOrUpdated,
          weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnBrowserAppRemoved(
    apps::BrowserAppInstanceUpdate update) {
  auto window_id = update.window_id;
  RunOrEnqueueEventForWindow(
      window_id,
      base::BindOnce(&BrowserAppInstanceRegistry::LacrosAppInstanceRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void BrowserAppInstanceRegistry::OnWindowInitialized(aura::Window* window) {
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

void BrowserAppInstanceRegistry::OnWindowVisibilityChanged(aura::Window* window,
                                                           bool visible) {
  if (visible) {
    // When `LacrosWindowInstanceAdded()` or
    // `LacrosAppInstanceAddedOrUpdated()` is called, the `window` is not
    // attached to the tree yet and will not be able to get `ActivationClient`
    // from it. For this reason, for Lacros window, we delay the start of
    // activation to when it becomes visible.
    MaybeStartActivationObservation(window);
  }
}

void BrowserAppInstanceRegistry::OnWindowDestroying(aura::Window* window) {
  lacros_window_observations_.RemoveObservation(window);
  const std::string* id = exo::GetShellApplicationId(window);
  DCHECK(id);
  DCHECK(base::Contains(window_id_to_event_list_, *id));
  window_id_to_event_list_.erase(*id);

  for (auto it = std::begin(lacros_app_instances_);
       it != std::end(lacros_app_instances_);) {
    if (it->second->window == window) {
      auto instance = std::move(it->second);
      it = lacros_app_instances_.erase(it);
      for (auto& observer : observers_) {
        observer.OnBrowserAppRemoved(*instance);
      }
    } else {
      it++;
    }
  }
  for (auto it = std::begin(lacros_window_instances_);
       it != std::end(lacros_window_instances_);) {
    if (it->second->window == window) {
      auto instance = std::move(it->second);
      it = lacros_window_instances_.erase(it);
      for (auto& observer : observers_) {
        observer.OnBrowserWindowRemoved(*instance);
      }
    } else {
      it++;
    }
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

// Run the action immediately if the window matching |window_id| is
// available, otherwise buffer the event until it is.
void BrowserAppInstanceRegistry::RunOrEnqueueEventForWindow(
    const std::string& window_id,
    base::OnceCallback<void(aura::Window*)> event) {
  auto& event_list = window_id_to_event_list_[window_id];
  if (event_list.window) {
    std::move(event).Run(event_list.window.get());
  } else {
    event_list.events.push_back(std::move(event));
  }
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
  if (instance && instance->MaybeUpdate(update.is_active)) {
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
  if (instance) {
    for (auto& observer : observers_) {
      observer.OnBrowserWindowRemoved(*instance);
    }
  }
}

void BrowserAppInstanceRegistry::LacrosAppInstanceAddedOrUpdated(
    apps::BrowserAppInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  // Create instance if it does not already eixsts, update it if exists.
  //
  // In some cases this may result in the removal of an instance and then
  // immediate recreation of it with the same ID, but it's necessary to maintain
  // app instances with a valid window.
  //
  // For example, if the last tab is dragged from browser A into browser B, the
  // tab will get reparented into a different window and browser A's window is
  // destroyed. However app instance messages and window destruction events may
  // arrive out of order because they originate from different sources now
  // (crosapi and wayland). If a window is destroyed first, it leaves the app
  // instance with an invalid window for a fraction of time. Rather than making
  // the window pointer nullable, we remove the instance and then re-add it when
  // an instance update message reparenting the instance into a new window
  // arrives.
  BrowserAppInstance* instance = GetInstance(lacros_app_instances_, update.id);
  if (instance) {
    if (instance->MaybeUpdate(window, update.title, update.is_browser_active,
                              update.is_web_contents_active,
                              update.browser_session_id,
                              update.restored_browser_session_id)) {
      for (auto& observer : observers_) {
        observer.OnBrowserAppUpdated(*instance);
      }
    }
  } else {
    auto id = update.id;
    auto& new_instance = AddInstance(
        lacros_app_instances_, id,
        std::make_unique<BrowserAppInstance>(std::move(update), window));
    for (auto& observer : observers_) {
      observer.OnBrowserAppAdded(new_instance);
    }
  }
}

void BrowserAppInstanceRegistry::LacrosAppInstanceRemoved(
    apps::BrowserAppInstanceUpdate update,
    aura::Window* window) {
  DCHECK(window);
  auto instance = PopInstanceIfExists(lacros_app_instances_, update.id);
  if (instance) {
    for (auto& observer : observers_) {
      observer.OnBrowserAppRemoved(*instance);
    }
  }
}

void BrowserAppInstanceRegistry::OnControllerDisconnected() {
  controller_.reset();
}

}  // namespace apps
