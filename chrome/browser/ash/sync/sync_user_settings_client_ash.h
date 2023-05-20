// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_USER_SETTINGS_CLIENT_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_USER_SETTINGS_CLIENT_ASH_H_

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

class SyncUserSettingsClientAsh : public crosapi::mojom::SyncUserSettingsClient,
                                  public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null. |this| must be destroyed before
  // |sync_service| shutdown.
  explicit SyncUserSettingsClientAsh(syncer::SyncService* sync_service);
  SyncUserSettingsClientAsh(const SyncUserSettingsClientAsh& other) = delete;
  SyncUserSettingsClientAsh& operator=(const SyncUserSettingsClientAsh& other) =
      delete;
  ~SyncUserSettingsClientAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver);

  // crosapi::mojom::SyncUserSettingsClient implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SyncUserSettingsClientObserver>
          observer) override;
  void IsAppsSyncEnabled(IsAppsSyncEnabledCallback callback) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync_service) override;

  void FlushMojoForTesting();

 private:
  const raw_ptr<syncer::SyncService> sync_service_;

  bool is_apps_sync_enabled_;

  // Don't add new members below this. `receivers_` and `observers_` should be
  // destroyed as soon as `this` is getting destroyed so that we don't deal
  // with message handling on a partially destroyed object.
  mojo::ReceiverSet<crosapi::mojom::SyncUserSettingsClient> receivers_;
  mojo::RemoteSet<crosapi::mojom::SyncUserSettingsClientObserver> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_USER_SETTINGS_CLIENT_ASH_H_
