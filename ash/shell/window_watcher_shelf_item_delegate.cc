// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher_shelf_item_delegate.h"

#include <utility>

#include "ash/shell/window_watcher.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

WindowWatcherShelfItemDelegate::WindowWatcherShelfItemDelegate(
    ShelfID id,
    WindowWatcher* watcher)
    : ShelfItemDelegate(id), watcher_(watcher) {
  DCHECK(!id.IsNull());
  DCHECK(watcher_);
}

WindowWatcherShelfItemDelegate::~WindowWatcherShelfItemDelegate() = default;

void WindowWatcherShelfItemDelegate::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  aura::Window* window = watcher_->GetWindowByID(shelf_id());
  window->Show();
  wm::ActivateWindow(window);
  std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
}

void WindowWatcherShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                                    int64_t command_id,
                                                    int32_t event_flags,
                                                    int64_t display_id) {
  // This delegate does not show custom context or application menu items.
  NOTIMPLEMENTED();
}

void WindowWatcherShelfItemDelegate::Close() {}

}  // namespace shell
}  // namespace ash
