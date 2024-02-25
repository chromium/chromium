// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
#define ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace ash {

// ShelfItemDelegate for the items created by ShelfWindowWatcher, for example:
// The Chrome OS settings window, task manager window, and panel windows.
class ShelfWindowWatcherItemDelegate : public ShelfItemDelegate {
 public:
  ShelfWindowWatcherItemDelegate(const ShelfID& id, aura::Window* window);

  ShelfWindowWatcherItemDelegate(const ShelfWindowWatcherItemDelegate&) =
      delete;
  ShelfWindowWatcherItemDelegate& operator=(
      const ShelfWindowWatcherItemDelegate&) = delete;

  ~ShelfWindowWatcherItemDelegate() override;

 private:
  // ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // The window associated with this item. Not owned.
  raw_ptr<aura::Window> window_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
