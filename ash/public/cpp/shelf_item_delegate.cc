// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item_delegate.h"

#include "base/bind.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

ShelfItemDelegate::ShelfItemDelegate(const ShelfID& shelf_id)
    : shelf_id_(shelf_id) {}

ShelfItemDelegate::~ShelfItemDelegate() = default;

void ShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                     int64_t display_id,
                                     ShelfLaunchSource source,
                                     ItemSelectedCallback callback) {
  std::move(callback).Run(SHELF_ACTION_NONE, {});
}

ShelfItemDelegate::AppMenuItems ShelfItemDelegate::GetAppMenuItems(
    int event_flags) {
  return {};
}

void ShelfItemDelegate::GetContextMenu(int64_t display_id,
                                       GetContextMenuCallback callback) {
  // Supplying null will cause ShelfView to show a default context menu.
  std::move(callback).Run(nullptr);
}

AppWindowLauncherItemController*
ShelfItemDelegate::AsAppWindowLauncherItemController() {
  return nullptr;
}

bool ShelfItemDelegate::ExecuteContextMenuCommand(int64_t command_id,
                                                  int32_t event_flags) {
  DCHECK(context_menu_);
  // Help subclasses execute context menu items, which may be on a sub-menu.
  ui::MenuModel* model = context_menu_.get();
  int index = -1;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index))
    return false;

  model->ActivatedAt(index, event_flags);
  return true;
}

}  // namespace ash
