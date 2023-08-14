// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include <utility>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/shell.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"

namespace crosapi {

namespace {
std::vector<crosapi::mojom::ClipboardHistoryItemDescriptorPtr>
GetCurrentDescriptors() {
  std::vector<crosapi::mojom::ClipboardHistoryItemDescriptorPtr>
      descriptor_ptrs;
  for (const auto& descriptor :
       chromeos::clipboard_history::QueryItemDescriptors()) {
    descriptor_ptrs.push_back(descriptor.Clone());
  }
  return descriptor_ptrs;
}
}  // namespace

// ClipboardHistoryAsh::ClientUpdater ------------------------------------------

// Updates the cached descriptors of the associated client. Used only when the
// clipboard history refresh feature is enabled.
class ClipboardHistoryAsh::ClientUpdater
    : public ash::ClipboardHistoryController::Observer {
 public:
  explicit ClientUpdater(mojom::ClipboardHistoryClient* client)
      : client_(client) {
    CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
    observation_.Observe(ash::ClipboardHistoryController::Get());
  }
  ClientUpdater(const ClientUpdater&) = delete;
  ClientUpdater& operator=(const ClientUpdater&) = delete;
  ~ClientUpdater() override = default;

  // ash::ClipboardHistoryController::Observer:
  void OnClipboardHistoryItemsUpdated() override {
    client_->SetClipboardHistoryItemDescriptors(GetCurrentDescriptors());
  }

 private:
  // The associated client.
  const raw_ptr<mojom::ClipboardHistoryClient> client_;

  // The observation on the clipboard history controller.
  base::ScopedObservation<ash::ClipboardHistoryController,
                          ash::ClipboardHistoryController::Observer>
      observation_{this};
};

// ClipboardHistoryAsh ---------------------------------------------------------

ClipboardHistoryAsh::ClipboardHistoryAsh() {
  // `ash::Shell` may not exist in tests.
  if (ash::Shell::HasInstance()) {
    shell_observation_.Observe(ash::Shell::Get());
  }
}

ClipboardHistoryAsh::~ClipboardHistoryAsh() = default;

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

  // In testing, `remote_client_` can be already bound when multiple Lacros
  // tests run in parallel. This does not happen on real devices.
  // TODO(http://b/294617428): Make `ClipboardHistoryAsh` support multiple
  // clients then remove this code.
  if (remote_client_.is_bound()) {
    return;
  }

  remote_client_.Bind(std::move(client));

  // `remote_client_` is a class member so it is safe to use `this` here.
  remote_client_.set_disconnect_handler(base::BindOnce(
      &ClipboardHistoryAsh::OnRemoteDisconnected, base::Unretained(this)));

  // If there are clipboard history item descriptors, send the descriptors to
  // `remote_client_` when connection is built.
  if (auto descriptors = GetCurrentDescriptors(); !descriptors.empty()) {
    remote_client_->SetClipboardHistoryItemDescriptors(std::move(descriptors));
  }

  client_updater_ = std::make_unique<ClientUpdater>(remote_client_.get());
}

void ClipboardHistoryAsh::UpdateRemoteDescriptorsForTesting() {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
  CHECK(remote_client_.is_bound());
  CHECK(client_updater_);

  client_updater_->OnClipboardHistoryItemsUpdated();
}

void ClipboardHistoryAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ClipboardHistory> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ClipboardHistoryAsh::FlushForTesting() {
  remote_client_.FlushForTesting();  // IN-TEST
}

void ClipboardHistoryAsh::OnShellDestroying() {
  shell_observation_.Reset();
  ClearRemoteConnection();
}

void ClipboardHistoryAsh::OnRemoteDisconnected() {
  ClearRemoteConnection();
}

void ClipboardHistoryAsh::ClearRemoteConnection() {
  client_updater_.reset();
  remote_client_.reset();
}

}  // namespace crosapi
