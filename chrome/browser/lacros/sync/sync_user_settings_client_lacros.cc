// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

SyncUserSettingsClientLacros::SyncUserSettingsClientLacros(
    mojo::Remote<crosapi::mojom::SyncUserSettingsClient> remote,
    syncer::SyncService* sync_service)
    : sync_service_(sync_service), remote_(std::move(remote)) {
  DCHECK(remote_.is_bound());

  remote_.get()->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  sync_service_->AddObserver(this);

  remote_.get()->IsAppsSyncEnabled(
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
  remote_.reset();
  observer_receiver_.reset();
  sync_service_ = nullptr;
}

void SyncUserSettingsClientLacros::OnIsAppsSyncEnabledFetched(
    bool is_apps_sync_enabled) {
  DCHECK(sync_service_);
  sync_service_->GetUserSettings()->SetAppsSyncEnabledByOs(
      is_apps_sync_enabled);
}
