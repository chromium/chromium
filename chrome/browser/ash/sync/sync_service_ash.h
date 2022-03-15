// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_ASH_H_

#include <memory>

#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace syncer {
class SyncService;
}

namespace ash {

// Implements Crosapi SyncService interface, that allows interaction of Lacros
// and Ash SyncServices.
// TODO(crbug.com/1233545): Consider renaming to something more distinguishable
// from syncer::SyncService (e.g. SyncMojoService), especially if there will be
// a need to create ash-specific SyncService implementation or code around its
// initialization (e.g. SyncClient or SyncServiceFactory).
class SyncServiceAsh : public KeyedService, public crosapi::mojom::SyncService {
 public:
  // |sync_service| must not be null. |this| should depend on |sync_service| and
  // be shutted down before it.
  explicit SyncServiceAsh(syncer::SyncService* sync_service);
  SyncServiceAsh(const SyncServiceAsh& other) = delete;
  SyncServiceAsh& operator=(const SyncServiceAsh& other) = delete;
  ~SyncServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncService> receiver);

  // KeyedService implementation.
  void Shutdown() override;

  // crosapi::mojom::SyncService implementation.
  void BindExplicitPassphraseClient(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver) override;

 private:
  // Members below destroyed after Shutdown().
  // |explicit_passphrase_client_| is null if
  // kSyncChromeOSExplicitPassphraseSharing is disabled.
  std::unique_ptr<SyncExplicitPassphraseClientAsh> explicit_passphrase_client_;
  mojo::ReceiverSet<crosapi::mojom::SyncService> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_ASH_H_
