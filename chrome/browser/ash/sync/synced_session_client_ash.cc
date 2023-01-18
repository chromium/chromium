// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/synced_session_client_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

SyncedSessionClientAsh::SyncedSessionClientAsh() = default;
SyncedSessionClientAsh::~SyncedSessionClientAsh() = default;

void SyncedSessionClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncedSessionClientAsh::OnForeignSyncedPhoneSessionsUpdated(
    std::vector<crosapi::mojom::SyncedSessionPtr> sessions) {
  // TODO(b/260599791): implement handling of SyncedSessions upon update.
  NOTIMPLEMENTED();
}

}  // namespace ash
