// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"

#include "base/callback.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

SyncUserSettingsClientLacros::SyncUserSettingsClientLacros(
    syncer::SyncService* sync_service,
    mojo::Remote<crosapi::mojom::SyncService>* sync_service_remote)
    : sync_service_(sync_service) {
  DCHECK(sync_service_);
  DCHECK(sync_service_remote);
  DCHECK(sync_service_remote->is_bound());

  (*sync_service_remote)
      ->BindUserSettingsClient(client_remote_.BindNewPipeAndPassReceiver());
  client_remote_.get()->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());
  sync_service_->AddObserver(this);

  client_remote_.get()->IsAppsSyncEnabled(
      base::BindOnce(&SyncUserSettingsClientLacros::OnIsAppsSyncEnabledFetched,
                     base::Unretained(this)));
}

SyncUserSettingsClientLacros::~SyncUserSettingsClientLacros() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void SyncUserSettingsClientLacros::OnAppsSyncEnabledChanged(
    bool is_apps_sync_enabled) {
  DCHECK(sync_service_);
  sync_service_->GetUserSettings()->SetAppsSyncEnabledByOs(
      is_apps_sync_enabled);
}

void SyncUserSettingsClientLacros::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  sync_service_->RemoveObserver(this);
  client_remote_.reset();
  observer_receiver_.reset();
  sync_service_ = nullptr;
}

void SyncUserSettingsClientLacros::OnIsAppsSyncEnabledFetched(
    bool is_apps_sync_enabled) {
  DCHECK(sync_service_);
  sync_service_->GetUserSettings()->SetAppsSyncEnabledByOs(
      is_apps_sync_enabled);
}
