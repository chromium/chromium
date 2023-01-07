// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/shell.h"

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
  auto* clipboard_history_controller =
      ash::Shell::Get()->clipboard_history_controller();
  if (!clipboard_history_controller)
    return;

  clipboard_history_controller->ShowMenu(anchor_point, menu_source_type,
                                         show_source);
}

}  // namespace crosapi
