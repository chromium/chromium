// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_

#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
class Shell;
}  // namespace ash

namespace base {
class UnguessableToken;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace crosapi {

// Ash-Chrome implementation of `mojom::ClipboardHistory`. Handles
// communications with Lacros.
class ClipboardHistoryAsh : public mojom::ClipboardHistory,
                            public ash::ShellObserver {
 public:
  ClipboardHistoryAsh();
  ClipboardHistoryAsh(const ClipboardHistoryAsh&) = delete;
  ClipboardHistoryAsh& operator=(const ClipboardHistoryAsh&) = delete;
  ~ClipboardHistoryAsh() override;

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

  void BindReceiver(mojo::PendingReceiver<mojom::ClipboardHistory> receiver);

  // Updates the cached descriptors on `remote_client_` with the current
  // clipboard history.
  void UpdateRemoteDescriptorsForTesting();

  // Flushes the calls on `remotes_`.
  void FlushForTesting();

 private:
  class ClientUpdater;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  // Called when the remote client disconnects.
  void OnRemoteDisconnected();

  // Clears the connection with `remote_client`. Called when `remote_client`
  // disconnects or Ash is being destroyed.
  void ClearRemoteConnection();

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  // TODO(http://b/281766341): Use `mojo::Receiver` here.
  mojo::ReceiverSet<mojom::ClipboardHistory> receivers_;

  // There is only one client connection.
  mojo::Remote<mojom::ClipboardHistoryClient> remote_client_;

  // Responsible for updating the cached descriptors on `remote_client_`.
  // Constructed when the remote connection is built; destroyed when the
  // connection with remote is cleaned.
  std::unique_ptr<ClientUpdater> client_updater_;

  // The observation on `ash::Shell`.
  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_
