// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/chrome_app_window_tracker_ash.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/exo/shell_surface_util.h"

namespace crosapi {

ChromeAppWindowTrackerAsh::ChromeAppWindowTrackerAsh() {
  env_observation_.Observe(aura::Env::GetInstance());
}

ChromeAppWindowTrackerAsh::~ChromeAppWindowTrackerAsh() = default;

void ChromeAppWindowTrackerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::AppWindowTracker> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ChromeAppWindowTrackerAsh::OnAppWindowAdded(const std::string& app_id,
                                                 const std::string& window_id) {
  pending_window_ids_[window_id].app_id = app_id;
  CheckWindowNoLongerPending(window_id);
  full_restore::OnLacrosChromeAppWindowAdded(app_id, window_id);
}

void ChromeAppWindowTrackerAsh::OnAppWindowRemoved(
    const std::string& app_id,
    const std::string& window_id) {
  pending_window_ids_.erase(window_id);
  full_restore::OnLacrosChromeAppWindowRemoved(app_id, window_id);
}

void ChromeAppWindowTrackerAsh::OnWindowInitialized(aura::Window* window) {
  if (!crosapi::browser_util::IsLacrosWindow(window)) {
    return;
  }

  std::string window_id = *exo::GetShellApplicationId(window);

  // All Lacros windows get tracked, including non-app windows. Only app windows
  // will become StandaloneBrowserExtensionAppShelfItemControllers.
  pending_window_ids_[window_id].window = window;
  window_observations_.AddObservation(window);

  CheckWindowNoLongerPending(window_id);
}

void ChromeAppWindowTrackerAsh::OnWindowDestroying(aura::Window* window) {
  auto it = pending_window_ids_.find(*exo::GetShellApplicationId(window));
  if (it == pending_window_ids_.end())
    return;

  DCHECK_EQ(it->second.window, window);
  window_observations_.RemoveObservation(window);
  pending_window_ids_.erase(it);
}

void ChromeAppWindowTrackerAsh::UpdateShelf(const std::string& app_id,
                                            aura::Window* window) {
  ash::ShelfID shelf_id(app_id);
  ash::ShelfItemDelegate* existing_delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(shelf_id);
  if (existing_delegate) {
    // If there's already a delegate in the shelf then it must be an instance of
    // StandaloneBrowserExtensionAppShelfItemController.
    auto* controller =
        static_cast<StandaloneBrowserExtensionAppShelfItemController*>(
            existing_delegate);
    controller->StartTrackingInstance(window);
  } else {
    // Since there's no delegate or item in the shelf, we know the item isn't
    // pinned. Therefore, the type must be TYPE_APP.
    ash::ShelfItem item;
    item.id = shelf_id;
    item.type = ash::TYPE_APP;
    auto delegate =
        std::make_unique<StandaloneBrowserExtensionAppShelfItemController>(
            shelf_id, window);
    ash::ShelfModel::Get()->Add(item, std::move(delegate));
  }
}

void ChromeAppWindowTrackerAsh::CheckWindowNoLongerPending(
    const std::string& window_id) {
  auto pending_window = pending_window_ids_.find(window_id);
  if (pending_window == pending_window_ids_.end())
    return;

  // The window is still pending
  if (pending_window->second.app_id.empty() || !pending_window->second.window)
    return;


  std::string app_id = std::move(pending_window->second.app_id);
  aura::Window* window = pending_window->second.window;

  // Now that both pieces of metadata are available, we can stop tracking the
  // window.
  pending_window_ids_.erase(pending_window);
  window_observations_.RemoveObservation(window);

  UpdateShelf(app_id, window);
}

}  // namespace crosapi
