// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_user_settings_client_ash.h"

#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace ash {

namespace {

bool IsAppsSyncEnabledForSyncService(const syncer::SyncService& sync_service) {
  return sync_service.GetUserSettings()->GetSelectedOsTypes().Has(
      syncer::UserSelectableOsType::kOsApps);
}

}  // namespace

SyncUserSettingsClientAsh::SyncUserSettingsClientAsh(
    syncer::SyncService* sync_service)
    : sync_service_(sync_service),
      is_apps_sync_enabled_(IsAppsSyncEnabledForSyncService(*sync_service)) {
  sync_service_->AddObserver(this);
}

SyncUserSettingsClientAsh::~SyncUserSettingsClientAsh() {
  sync_service_->RemoveObserver(this);
}

void SyncUserSettingsClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncUserSettingsClientAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::SyncUserSettingsClientObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void SyncUserSettingsClientAsh::IsAppsSyncEnabled(
    IsAppsSyncEnabledCallback callback) {
  std::move(callback).Run(is_apps_sync_enabled_);
}

void SyncUserSettingsClientAsh::OnStateChanged(
    syncer::SyncService* sync_service) {
  bool new_is_apps_sync_enabled =
      IsAppsSyncEnabledForSyncService(*sync_service_);
  if (new_is_apps_sync_enabled == is_apps_sync_enabled_) {
    return;
  }

  is_apps_sync_enabled_ = new_is_apps_sync_enabled;
  for (auto& observer : observers_) {
    observer->OnAppsSyncEnabledChanged(is_apps_sync_enabled_);
  }
}

void SyncUserSettingsClientAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
