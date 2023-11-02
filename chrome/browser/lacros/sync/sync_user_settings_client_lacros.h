// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/driver/sync_service_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace syncer {
class SyncService;
}  // namespace syncer

// Once created, observes changes in Ash SyncUserSettings via Crosapi
// (currently, only apps toggle state) and populates them to Lacros
// SyncUserSettings. Stops working upon Lacros SyncService Shutdown().
class SyncUserSettingsClientLacros
    : public crosapi::mojom::SyncUserSettingsClientObserver,
      public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null. |sync_service_remote| must not be null and
  // must be bound.
  SyncUserSettingsClientLacros(
      syncer::SyncService* sync_service,
      mojo::Remote<crosapi::mojom::SyncService>* sync_service_remote);
  SyncUserSettingsClientLacros(const SyncUserSettingsClientLacros& other) =
      delete;
  SyncUserSettingsClientLacros& operator=(
      const SyncUserSettingsClientLacros& other) = delete;
  ~SyncUserSettingsClientLacros() override;

  // crosapi::mojom::SyncUserSettingsClientObserver overrides.
  void OnAppsSyncEnabledChanged(bool is_apps_sync_enabled) override;

  // SyncServiceObserver overrides.
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

 private:
  void OnIsAppsSyncEnabledFetched(bool is_apps_sync_enabled);

  base::raw_ptr<syncer::SyncService> sync_service_;
  mojo::Receiver<crosapi::mojom::SyncUserSettingsClientObserver>
      observer_receiver_{this};
  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> client_remote_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_
