// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/remote_shelf_item_delegate.h"

#include <memory>

namespace ash {

RemoteShelfItemDelegate::RemoteShelfItemDelegate(
    const ShelfID& shelf_id,
    mojom::ShelfItemDelegatePtr delegate)
    : ShelfItemDelegate(shelf_id), delegate_(std::move(delegate)) {}

RemoteShelfItemDelegate::~RemoteShelfItemDelegate() = default;

void RemoteShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                           int64_t display_id,
                                           ShelfLaunchSource source,
                                           ItemSelectedCallback callback) {
  delegate_->ItemSelected(std::move(event), display_id, source,
                          std::move(callback));
}

void RemoteShelfItemDelegate::GetContextMenuItems(
    int64_t display_id,
    GetContextMenuItemsCallback callback) {
  delegate_->GetContextMenuItems(display_id, std::move(callback));
}

void RemoteShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                             int64_t command_id,
                                             int32_t event_flags,
                                             int64_t display_id) {
  delegate_->ExecuteCommand(from_context_menu, command_id, event_flags,
                            display_id);
}

void RemoteShelfItemDelegate::Close() {
  delegate_->Close();
}

}  // namespace ash
