// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/unguessable_token.h"

namespace crosapi {

ClipboardHistoryAsh::ClipboardHistoryAsh() = default;
ClipboardHistoryAsh::~ClipboardHistoryAsh() = default;

void ClipboardHistoryAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ClipboardHistory> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ClipboardHistoryAsh::ShowClipboard(
    const gfx::Rect& anchor_point,
    ui::MenuSourceType menu_source_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  if (auto* clipboard_history_controller =
          ash::ClipboardHistoryController::Get()) {
    clipboard_history_controller->ShowMenu(anchor_point, menu_source_type,
                                           show_source);
  }
}

void ClipboardHistoryAsh::PasteClipboardItemById(
    const base::UnguessableToken& item_id,
    int event_flags,
    mojom::ClipboardHistoryControllerShowSource paste_source) {
  if (auto* clipboard_history_controller =
          ash::ClipboardHistoryController::Get()) {
    clipboard_history_controller->PasteClipboardItemById(
        item_id.ToString(), event_flags, paste_source);
  }
}

}  // namespace crosapi
