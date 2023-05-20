// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace crosapi {

// Ash-Chrome implementation of `mojom::ClipboardHistory`. Handles
// communications with Lacros.
class ClipboardHistoryAsh : public mojom::ClipboardHistory {
 public:
  ClipboardHistoryAsh();
  ClipboardHistoryAsh(const ClipboardHistoryAsh&) = delete;
  ClipboardHistoryAsh& operator=(const ClipboardHistoryAsh&) = delete;
  ~ClipboardHistoryAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ClipboardHistory> receiver);

  // mojom::ClipboardHistory:
  void ShowClipboard(
      const gfx::Rect& anchor_point,
      ui::MenuSourceType menu_source_type,
      mojom::ClipboardHistoryControllerShowSource show_source) override;
  void PasteClipboardItemById(
      const base::UnguessableToken& item_id,
      int event_flags,
      mojom::ClipboardHistoryControllerShowSource paste_source) override;
  void RegisterClient(
      mojo::PendingRemote<mojom::ClipboardHistoryClient> client) override;

  // Updates the cached descriptors on `remote_client_` with the current
  // clipboard history.
  void UpdateRemoteDescriptorsForTesting();

  // Flushes the calls on `remotes_`.
  void FlushForTesting();

 private:
  // Called when the remote client is disconnected.
  void OnRemoteDisconnected();

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  // TODO(http://b/281766341): Use `mojo::Receiver` here.
  mojo::ReceiverSet<mojom::ClipboardHistory> receivers_;

  // There is only one client connection.
  mojo::Remote<mojom::ClipboardHistoryClient> remote_client_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_
