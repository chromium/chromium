// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include <utility>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"

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

void ClipboardHistoryAsh::RegisterClient(
    mojo::PendingRemote<mojom::ClipboardHistoryClient> client) {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());

  // `remote_client_` should be unbounded. Because `remote_client_` is reset in
  // the cleaning function when disconnected.
  CHECK(!remote_client_.is_bound());

  remote_client_.Bind(std::move(client));

  // `remote_client_` is a class member so it is safe to use `this` here.
  remote_client_.set_disconnect_handler(base::BindOnce(
      &ClipboardHistoryAsh::OnRemoteDisconnected, base::Unretained(this)));
}

void ClipboardHistoryAsh::UpdateRemoteDescriptorsForTesting() {
  // TODO(http://b/278916298): Implement client update in subsequent CLs.

  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
  const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>
      descriptors = chromeos::clipboard_history::QueryItemDescriptors();
  std::vector<crosapi::mojom::ClipboardHistoryItemDescriptorPtr>
      descriptor_ptrs;
  for (const auto& descriptor : descriptors) {
    descriptor_ptrs.push_back(descriptor.Clone());
  }

  remote_client_->SetClipboardHistoryItemDescriptors(
      std::move(descriptor_ptrs));
}

void ClipboardHistoryAsh::FlushForTesting() {
  remote_client_.FlushForTesting();  // IN-TEST
}

void ClipboardHistoryAsh::OnRemoteDisconnected() {
  remote_client_.reset();
}

}  // namespace crosapi
