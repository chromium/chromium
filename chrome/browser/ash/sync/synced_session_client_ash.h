// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_

#include <vector>

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// Implements the SyncedSessionClient mojo interface to receive foreign session
// updates.
class SyncedSessionClientAsh final
    : public crosapi::mojom::SyncedSessionClient {
 public:
  SyncedSessionClientAsh();
  SyncedSessionClientAsh(const SyncedSessionClientAsh&) = delete;
  SyncedSessionClientAsh& operator=(const SyncedSessionClientAsh&) = delete;
  ~SyncedSessionClientAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver);

  // crosapi::mojom::SyncedSessionClient
  void OnForeignSyncedPhoneSessionsUpdated(
      std::vector<crosapi::mojom::SyncedSessionPtr> sessions) override;

 private:
  mojo::ReceiverSet<crosapi::mojom::SyncedSessionClient> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_
