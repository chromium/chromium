// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item_delegate.h"

#include "ui/base/models/simple_menu_model.h"

namespace ash {

ShelfItemDelegate::ShelfItemDelegate(const ShelfID& shelf_id)
    : shelf_id_(shelf_id) {}

ShelfItemDelegate::~ShelfItemDelegate() = default;

void ShelfItemDelegate::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  std::move(callback).Run(SHELF_ACTION_NONE, {});
}

ShelfItemDelegate::AppMenuItems ShelfItemDelegate::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  return {};
}

void ShelfItemDelegate::GetContextMenu(int64_t display_id,
                                       GetContextMenuCallback callback) {
  // Supplying null will cause ShelfView to show a default context menu.
  std::move(callback).Run(nullptr);
}

AppWindowShelfItemController*
ShelfItemDelegate::AsAppWindowShelfItemController() {
  return nullptr;
}

}  // namespace ash
