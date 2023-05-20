// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

class SyncExplicitPassphraseClientAsh
    : public crosapi::mojom::SyncExplicitPassphraseClient,
      public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null. |this| must be destroyed before
  // |sync_service| shutdown.
  explicit SyncExplicitPassphraseClientAsh(syncer::SyncService* sync_service);
  SyncExplicitPassphraseClientAsh(
      const SyncExplicitPassphraseClientAsh& other) = delete;
  SyncExplicitPassphraseClientAsh& operator=(
      const SyncExplicitPassphraseClientAsh& other) = delete;
  ~SyncExplicitPassphraseClientAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver);

  // crosapi::mojom::AshSyncExplicitPassphraseClient implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
          observer) override;
  void GetDecryptionNigoriKey(crosapi::mojom::AccountKeyPtr mojo_account_key,
                              GetDecryptionNigoriKeyCallback callback) override;
  void SetDecryptionNigoriKey(
      crosapi::mojom::AccountKeyPtr mojo_account_key,
      crosapi::mojom::NigoriKeyPtr mojo_nigori_key) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync_service) override;

  void FlushMojoForTesting();

 private:
  bool ValidateAccountKey(
      const crosapi::mojom::AccountKeyPtr& mojo_account_key) const;

  const raw_ptr<syncer::SyncService> sync_service_;

  bool is_passphrase_required_;
  bool is_passphrase_available_;

  // Don't add new members below this. `receivers_` and `observers_` should be
  // destroyed as soon as `this` is getting destroyed so that we don't deal
  // with message handling on a partially destroyed object.
  mojo::ReceiverSet<crosapi::mojom::SyncExplicitPassphraseClient> receivers_;
  mojo::RemoteSet<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_
