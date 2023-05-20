// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/service/sync_user_settings.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

SyncUserSettingsClientLacros::SyncUserSettingsClientLacros(
    mojo::Remote<crosapi::mojom::SyncUserSettingsClient> remote,
    syncer::SyncUserSettings* sync_user_settings)
    : sync_user_settings_(sync_user_settings), remote_(std::move(remote)) {
  DCHECK(remote_.is_bound());
  DCHECK(sync_user_settings_);

  remote_.get()->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());

  remote_.get()->IsAppsSyncEnabled(
      base::BindOnce(&SyncUserSettingsClientLacros::OnIsAppsSyncEnabledFetched,
                     base::Unretained(this)));
}

SyncUserSettingsClientLacros::~SyncUserSettingsClientLacros() = default;

void SyncUserSettingsClientLacros::OnAppsSyncEnabledChanged(
    bool is_apps_sync_enabled) {
  sync_user_settings_->SetAppsSyncEnabledByOs(is_apps_sync_enabled);
}

void SyncUserSettingsClientLacros::OnIsAppsSyncEnabledFetched(
    bool is_apps_sync_enabled) {
  sync_user_settings_->SetAppsSyncEnabledByOs(is_apps_sync_enabled);
}
