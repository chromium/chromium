// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/crosapi/lacros_shelf_item_tracker.h"

#include <map>
#include <set>
#include <string>

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/lacros_shelf_item_controller.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "components/exo/shell_surface_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/env.h"

namespace crosapi {

LacrosShelfItemTracker::LacrosShelfItemTracker() {
  env_observation_.Observe(aura::Env::GetInstance());
}

LacrosShelfItemTracker::~LacrosShelfItemTracker() = default;

void LacrosShelfItemTracker::BindReceiver(
    mojo::PendingReceiver<mojom::LacrosShelfItemTracker> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void LacrosShelfItemTracker::AddOrUpdateWindow(
    mojom::WindowDataPtr window_data) {
  std::string window_id = window_data->window_id;
  auto it = lacros_prefixed_windows_.find(window_id);
  if (it != lacros_prefixed_windows_.end() &&
      it->second.window_data.has_value()) {
    CHECK(window_data->item_id == it->second.window_data.value()->item_id)
        << "The window has already been added under a different shelf item.";
  }
  lacros_prefixed_windows_[window_id].window_data = std::move(window_data);
  MaybeAddToShelf(window_id);
}

void LacrosShelfItemTracker::OnWindowDestroying(aura::Window* window) {
  lacros_prefixed_windows_observations_.RemoveObservation(window);

  std::string window_id = *exo::GetShellApplicationId(window);

  auto lacros_prefixed_windows_it = lacros_prefixed_windows_.find(window_id);
  CHECK(lacros_prefixed_windows_it != lacros_prefixed_windows_.end());

  // If |window_data| is non-null, then we should delete the window from
  // |shelf_tem_windows_| as well. If it's the last window of the Shelf item,
  // remove the item from Shelf.
  if (lacros_prefixed_windows_it->second.window_data.has_value()) {
    std::string item_id =
        lacros_prefixed_windows_it->second.window_data.value()->item_id;
    auto shelf_item_windows_it = shelf_item_windows_.find(item_id);
    CHECK(shelf_item_windows_it != shelf_item_windows_.end());
    std::set<std::string>& windows_under_item = shelf_item_windows_it->second;

    auto window_it = windows_under_item.find(window_id);
    CHECK(window_it != windows_under_item.end());
    windows_under_item.erase(window_it);
    if (windows_under_item.empty()) {
      RemoveFromShelf(ash::ShelfID(item_id));
    }
  }

  lacros_prefixed_windows_.erase(window_id);
}

void LacrosShelfItemTracker::OnWindowInitialized(aura::Window* window) {
  if (!crosapi::browser_util::IsLacrosWindow(window)) {
    return;
  }

  // If |IsLacrosWindow()| is true, then the ID cannot be null.
  std::string window_id = *exo::GetShellApplicationId(window);
  lacros_prefixed_windows_[window_id].window = window;
  lacros_prefixed_windows_observations_.AddObservation(window);

  MaybeAddToShelf(window_id);
}

ash::ShelfItemDelegate*
LacrosShelfItemTracker::AddOrUpdateShelfItemAndReturnDelegate(
    mojom::WindowDataPtr window_data) {
  std::string item_id = window_data->item_id;
  ash::ShelfID shelf_id(item_id);
  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();

  int index = ash::ShelfModel::Get()->ItemIndexByID(shelf_id);

  if (index == -1) {
    // If there is no existing item by the ID in the Shelf, we add a new item.
    mojom::InstanceType instance_type = window_data->instance_type;
    std::unique_ptr<ash::ShelfItemDelegate> created_delegate =
        CreateDelegateByInstanceType(shelf_id, instance_type);

    ash::ShelfItem item;
    item.id = shelf_id;
    item.title = static_cast<LacrosShelfItemController*>(created_delegate.get())
                     ->GetTitle();
    CHECK(!item.title.empty());
    item.status = ash::STATUS_RUNNING;
    item.type = ash::TYPE_APP;
    if (!window_data->icon.isNull()) {
      item.image = window_data->icon;
    }

    shelf_model->Add(item, std::move(created_delegate));
  } else {
    // If the item already exists in the Shelf, we update.
    const ash::ShelfItem* existing_item = shelf_model->ItemByID(shelf_id);
    CHECK(existing_item);

    ash::ShelfItem item = *existing_item;
    // Icon is the only thing we update for now.
    if (!window_data->icon.isNull()) {
      item.image = window_data->icon;
    }

    ash::ShelfModel::Get()->Set(index, item);
  }
  return shelf_model->GetShelfItemDelegate(shelf_id);
}

void LacrosShelfItemTracker::RemoveFromShelf(const ash::ShelfID& shelf_id) {
  ash::ShelfModel::Get()->RemoveItemAndTakeShelfItemDelegate(shelf_id);
}

void LacrosShelfItemTracker::MaybeAddToShelf(const std::string& window_id) {
  auto lacros_prefixed_windows_it = lacros_prefixed_windows_.find(window_id);
  CHECK(lacros_prefixed_windows_it != lacros_prefixed_windows_.end());

  if (!lacros_prefixed_windows_it->second.window ||
      !lacros_prefixed_windows_it->second.window_data.has_value()) {
    return;
  }

  mojom::WindowDataPtr window_data =
      lacros_prefixed_windows_it->second.window_data.value().Clone();
  std::string item_id = window_data->item_id;

  ash::ShelfItemDelegate* delegate =
      AddOrUpdateShelfItemAndReturnDelegate(std::move(window_data));

  // |delegate| must be non-null and a child class of
  // |LacrosShelfItemController|.
  static_cast<LacrosShelfItemController*>(delegate)->AddWindow(
      lacros_prefixed_windows_it->second.window);

  shelf_item_windows_[item_id].insert(window_id);
}

// Returned delegate must be a child of |LacrosShelfItemController|.
std::unique_ptr<ash::ShelfItemDelegate>
LacrosShelfItemTracker::CreateDelegateByInstanceType(
    const ash::ShelfID& shelf_id,
    mojom::InstanceType instance_type) {
  switch (instance_type) {
    case mojom::InstanceType::kIsolatedWebAppInstaller: {
      return std::make_unique<IsolatedWebAppInstallerShelfItemController>(
          shelf_id);
    }
  }
  return nullptr;
}

LacrosShelfItemTracker::WindowInfo::WindowInfo() = default;
LacrosShelfItemTracker::WindowInfo::~WindowInfo() = default;
}  // namespace crosapi
