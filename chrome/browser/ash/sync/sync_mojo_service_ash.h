// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_ASH_H_

#include <memory>

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace syncer {
class SyncService;
}

namespace ash {

class SyncExplicitPassphraseClientAsh;
class SyncUserSettingsClientAsh;
class SyncedSessionClientAsh;

// Implements Crosapi SyncService interface, that allows interaction of Lacros
// and Ash SyncServices.
class SyncMojoServiceAsh : public KeyedService,
                           public crosapi::mojom::SyncService {
 public:
  // |sync_service| must not be null. |this| should depend on |sync_service| and
  // be shutted down before it.
  explicit SyncMojoServiceAsh(syncer::SyncService* sync_service);
  SyncMojoServiceAsh(const SyncMojoServiceAsh& other) = delete;
  SyncMojoServiceAsh& operator=(const SyncMojoServiceAsh& other) = delete;
  ~SyncMojoServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncService> receiver);

  // KeyedService implementation.
  void Shutdown() override;

  // crosapi::mojom::SyncService implementation.
  void BindExplicitPassphraseClient(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver) override;

  void BindUserSettingsClient(
      mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver)
      override;

  // TODO(b/260599791): Remove in M-114.
  void DEPRECATED_BindSyncedSessionClient(
      mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver)
      override;

  void CreateSyncedSessionClient(
      CreateSyncedSessionClientCallback callback) override;

  // Returns null if kChromeOSSyncedSessionClient is disabled.
  SyncedSessionClientAsh* GetSyncedSessionClientAsh() {
    return synced_session_client_.get();
  }

 private:
  // Members below destroyed after Shutdown().

  // |explicit_passphrase_client_| is null if
  // kSyncChromeOSExplicitPassphraseSharing is disabled.
  std::unique_ptr<SyncExplicitPassphraseClientAsh> explicit_passphrase_client_;

  // |user_settings_client_| is null if kSyncChromeOSAppsToggleSharing is
  // disabled.
  std::unique_ptr<SyncUserSettingsClientAsh> user_settings_client_;

  // |synced_session_client_| is null if kChromeOSSyncedSessionClient is
  // disabled.
  std::unique_ptr<SyncedSessionClientAsh> synced_session_client_;

  mojo::ReceiverSet<crosapi::mojom::SyncService> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_ASH_H_
