// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_window_watcher_item_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/types/display_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Returns the window's shelf item type property value.
ShelfItemType GetShelfItemType(aura::Window* window) {
  return static_cast<ShelfItemType>(window->GetProperty(kShelfItemTypeKey));
}

// Returns the window's shelf id property value.
ShelfID GetShelfID(aura::Window* window) {
  return ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
}

// Update the ShelfItem from relevant window properties.
void UpdateShelfItemForWindow(ShelfItem* item, aura::Window* window) {
  DCHECK(item->id.IsNull() || item->id == GetShelfID(window));
  item->id = GetShelfID(window);
  item->type = GetShelfItemType(window);
  item->title = window->GetTitle();

  // Active windows don't draw attention because the user is looking at them.
  if (window->GetProperty(aura::client::kDrawAttentionKey) &&
      !wm::IsActiveWindow(window)) {
    item->status = STATUS_ATTENTION;
  } else {
    item->status = STATUS_RUNNING;
  }

  // Prefer app icons over window icons, they're typically larger.
  gfx::ImageSkia* image = window->GetProperty(aura::client::kAppIconKey);
  if (!image || image->isNull())
    image = window->GetProperty(aura::client::kWindowIconKey);
  if (!image || image->isNull()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    item->image = rb.GetImageNamed(IDR_DEFAULT_FAVICON_32).AsImageSkia();
  } else {
    item->image = *image;
  }
}

}  // namespace

ShelfWindowWatcher::ContainerWindowObserver::ContainerWindowObserver(
    ShelfWindowWatcher* window_watcher)
    : window_watcher_(window_watcher) {}

ShelfWindowWatcher::ContainerWindowObserver::~ContainerWindowObserver() =
    default;

void ShelfWindowWatcher::ContainerWindowObserver::OnWindowAdded(
    aura::Window* new_window) {
  DCHECK(new_window);
  DCHECK(new_window->parent());
  DCHECK(desks_util::IsDeskContainer(new_window->parent()));
  window_watcher_->OnUserWindowAdded(new_window);
}

void ShelfWindowWatcher::ContainerWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  window_watcher_->OnContainerWindowDestroying(window);
}

////////////////////////////////////////////////////////////////////////////////

ShelfWindowWatcher::UserWindowObserver::UserWindowObserver(
    ShelfWindowWatcher* window_watcher)
    : window_watcher_(window_watcher) {}

ShelfWindowWatcher::UserWindowObserver::~UserWindowObserver() = default;

void ShelfWindowWatcher::UserWindowObserver::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == kShelfIDKey && window == window_util::GetActiveWindow()) {
    window_watcher_->model_->SetActiveShelfID(
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey)));
  }

  if (key == aura::client::kAppIconKey || key == aura::client::kWindowIconKey ||
      key == aura::client::kDrawAttentionKey || key == kShelfItemTypeKey ||
      key == kShelfIDKey) {
    window_watcher_->OnUserWindowPropertyChanged(window);
  }
}

void ShelfWindowWatcher::UserWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  window_watcher_->OnUserWindowDestroying(window);
}

void ShelfWindowWatcher::UserWindowObserver::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  // This is also called for descendants; check that the window is observed.
  if (window_watcher_->observed_user_windows_.IsObservingSource(window))
    window_watcher_->OnUserWindowPropertyChanged(window);
}

void ShelfWindowWatcher::UserWindowObserver::OnWindowTitleChanged(
    aura::Window* window) {
  window_watcher_->OnUserWindowPropertyChanged(window);
}

////////////////////////////////////////////////////////////////////////////////

ShelfWindowWatcher::ShelfWindowWatcher(ShelfModel* model)
    : model_(model),
      observed_container_windows_(&container_window_observer_),
      observed_user_windows_(&user_window_observer_) {
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  for (aura::Window* window : Shell::GetAllRootWindows())
    OnRootWindowAdded(window);
}

ShelfWindowWatcher::~ShelfWindowWatcher() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void ShelfWindowWatcher::AddShelfItem(aura::Window* window) {
  user_windows_with_items_.insert(window);
  ShelfItem item;
  UpdateShelfItemForWindow(&item, window);

  model_->AddAt(
      model_->item_count(), item,
      std::make_unique<ShelfWindowWatcherItemDelegate>(item.id, window));
}

void ShelfWindowWatcher::RemoveShelfItem(aura::Window* window) {
  user_windows_with_items_.erase(window);
  const ShelfID shelf_id = GetShelfID(window);
  DCHECK(!shelf_id.IsNull());
  const int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  model_->RemoveItemAt(index);
}

void ShelfWindowWatcher::OnContainerWindowDestroying(aura::Window* container) {
  observed_container_windows_.RemoveObservation(container);
}

void ShelfWindowWatcher::OnUserWindowAdded(aura::Window* window) {
  // The window may already be tracked from a prior display or parent container.
  if (observed_user_windows_.IsObservingSource(window))
    return;

  observed_user_windows_.AddObservation(window);

  // Add, update, or remove a ShelfItem for |window|, as needed.
  OnUserWindowPropertyChanged(window);
}

void ShelfWindowWatcher::OnUserWindowDestroying(aura::Window* window) {
  if (observed_user_windows_.IsObservingSource(window))
    observed_user_windows_.RemoveObservation(window);

  if (user_windows_with_items_.count(window) > 0)
    RemoveShelfItem(window);
  DCHECK_EQ(0u, user_windows_with_items_.count(window));
}

void ShelfWindowWatcher::OnUserWindowPropertyChanged(aura::Window* window) {
  // ShelfWindowWatcher only handles dialogs for now, all other shelf item
  // types are handled by ChromeShelfController.
  const ShelfItemType item_type = GetShelfItemType(window);
  if (item_type != TYPE_DIALOG || GetShelfID(window).IsNull()) {
    // Remove |window|'s ShelfItem if it was added by ShelfWindowWatcher.
    if (user_windows_with_items_.count(window) > 0)
      RemoveShelfItem(window);
    return;
  }

  // Update an existing ShelfWindowWatcher item when a window property changes.
  int index = model_->ItemIndexByID(GetShelfID(window));
  if (index >= 0 && user_windows_with_items_.count(window) > 0) {
    ShelfItem item = model_->items()[index];
    UpdateShelfItemForWindow(&item, window);
    model_->Set(index, item);
    return;
  }

  // Create a new item for |window|, if it is visible.
  if (index < 0 && window->IsVisible())
    AddShelfItem(window);
}

void ShelfWindowWatcher::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  if (gained_active && user_windows_with_items_.count(gained_active) > 0)
    OnUserWindowPropertyChanged(gained_active);
  if (lost_active && user_windows_with_items_.count(lost_active) > 0)
    OnUserWindowPropertyChanged(lost_active);

  model_->SetActiveShelfID(gained_active ? GetShelfID(gained_active)
                                         : ShelfID());
}

void ShelfWindowWatcher::OnRootWindowAdded(aura::Window* root_window) {
  for (aura::Window* container : desks_util::GetDesksContainers(root_window)) {
    for (aura::Window* window : container->children())
      OnUserWindowAdded(window);
    observed_container_windows_.AddObservation(container);
  }
}

}  // namespace ash
