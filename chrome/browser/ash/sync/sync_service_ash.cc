// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_service_ash.h"

namespace ash {

SyncServiceAsh::SyncServiceAsh(syncer::SyncService* sync_service)
    : explicit_passphrase_client_(
          std::make_unique<SyncExplicitPassphraseClientAsh>(sync_service)) {}

SyncServiceAsh::~SyncServiceAsh() = default;

void SyncServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncServiceAsh::Shutdown() {
  receivers_.Clear();
  explicit_passphrase_client_ = nullptr;
}

void SyncServiceAsh::BindExplicitPassphraseClient(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  // This method must not be called after shutdown.
  DCHECK(explicit_passphrase_client_);
  explicit_passphrase_client_->BindReceiver(std::move(receiver));
}

}  // namespace ash
