// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell/window_watcher_shelf_item_delegate.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

namespace {

}  // namespace

class WindowWatcher::WorkspaceWindowWatcher : public aura::WindowObserver {
 public:
  explicit WorkspaceWindowWatcher(WindowWatcher* watcher) : watcher_(watcher) {}

  ~WorkspaceWindowWatcher() override = default;

  void OnWindowAdded(aura::Window* new_window) override {
    new_window->AddObserver(watcher_);
  }

  void OnWillRemoveWindow(aura::Window* window) override {
    DCHECK(window->children().empty());
    window->RemoveObserver(watcher_);
  }

  void RootWindowAdded(aura::Window* root) {
    // The shelf is globally observing all active and inactive desks containers.
    for (aura::Window* container : desks_util::GetDesksContainers(root)) {
      container->AddObserver(watcher_);
      for (aura::Window* window : container->children())
        watcher_->OnWindowAdded(window);
    }
  }

  void RootWindowRemoved(aura::Window* root) {
    for (aura::Window* container : desks_util::GetDesksContainers(root)) {
      container->RemoveObserver(watcher_);
      for (aura::Window* window : container->children())
        watcher_->OnWillRemoveWindow(window);
    }
  }

 private:
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowWatcher);
};

WindowWatcher::WindowWatcher() {
  Shell::Get()->AddShellObserver(this);
  workspace_window_watcher_ = std::make_unique<WorkspaceWindowWatcher>(this);
  for (aura::Window* root : Shell::GetAllRootWindows())
    workspace_window_watcher_->RootWindowAdded(root);
}

WindowWatcher::~WindowWatcher() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    workspace_window_watcher_->RootWindowRemoved(root);
  Shell::Get()->RemoveShellObserver(this);
}

aura::Window* WindowWatcher::GetWindowByID(const ShelfID& id) {
  IDToWindow::const_iterator i = id_to_window_.find(id);
  return i != id_to_window_.end() ? i->second : NULL;
}

// aura::WindowObserver overrides:
void WindowWatcher::OnWindowAdded(aura::Window* new_window) {
  if (!window_util::IsWindowUserPositionable(new_window))
    return;

  ShelfModel* model = ShelfModel::Get();
  ShelfItem item;
  item.type = TYPE_APP;
  static int shelf_id = 0;
  item.id = ShelfID(base::NumberToString(shelf_id++));
  id_to_window_[item.id] = new_window;

  SkBitmap icon_bitmap;
  icon_bitmap.allocN32Pixels(16, 16);
  constexpr SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  icon_bitmap.eraseColor(colors[shelf_id % 3]);
  item.image = gfx::ImageSkia(gfx::ImageSkiaRep(icon_bitmap, 1.0f));
  item.title = base::NumberToString16(shelf_id);
  model->Add(item);

  model->SetShelfItemDelegate(
      item.id, std::make_unique<WindowWatcherShelfItemDelegate>(item.id, this));
  new_window->SetProperty(kShelfIDKey, item.id.Serialize());
}

void WindowWatcher::OnWillRemoveWindow(aura::Window* window) {
  for (IDToWindow::iterator i = id_to_window_.begin(); i != id_to_window_.end();
       ++i) {
    if (i->second == window) {
      ShelfModel* model = ShelfModel::Get();
      int index = model->ItemIndexByID(i->first);
      DCHECK_NE(-1, index);
      model->RemoveItemAt(index);
      id_to_window_.erase(i);
      break;
    }
  }
}

void WindowWatcher::OnRootWindowAdded(aura::Window* root_window) {
  workspace_window_watcher_->RootWindowAdded(root_window);
}

}  // namespace shell
}  // namespace ash
