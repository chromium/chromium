// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"

#include "base/feature_list.h"
#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"
#include "chrome/browser/ash/sync/sync_user_settings_client_ash.h"
#include "chrome/browser/ash/sync/synced_session_client_ash.h"
#include "components/sync/base/features.h"

namespace ash {

SyncMojoServiceAsh::SyncMojoServiceAsh(syncer::SyncService* sync_service) {
  if (base::FeatureList::IsEnabled(
          syncer::kSyncChromeOSExplicitPassphraseSharing)) {
    explicit_passphrase_client_ =
        std::make_unique<SyncExplicitPassphraseClientAsh>(sync_service);
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    user_settings_client_ =
        std::make_unique<SyncUserSettingsClientAsh>(sync_service);
  }
  if (base::FeatureList::IsEnabled(syncer::kChromeOSSyncedSessionSharing)) {
    synced_session_client_ = std::make_unique<SyncedSessionClientAsh>();
  }
}

SyncMojoServiceAsh::~SyncMojoServiceAsh() = default;

void SyncMojoServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncMojoServiceAsh::Shutdown() {
  receivers_.Clear();
  user_settings_client_ = nullptr;
  explicit_passphrase_client_ = nullptr;
  synced_session_client_ = nullptr;
}

void SyncMojoServiceAsh::BindExplicitPassphraseClient(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  // Null if feature is disabled.
  if (explicit_passphrase_client_) {
    explicit_passphrase_client_->BindReceiver(std::move(receiver));
  }
}

void SyncMojoServiceAsh::BindUserSettingsClient(
    mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver) {
  // Null if feature is disabled.
  if (user_settings_client_) {
    user_settings_client_->BindReceiver(std::move(receiver));
  }
}

void SyncMojoServiceAsh::DEPRECATED_BindSyncedSessionClient(
    mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver) {
  NOTIMPLEMENTED();
}

void SyncMojoServiceAsh::CreateSyncedSessionClient(
    CreateSyncedSessionClientCallback callback) {
  if (!base::FeatureList::IsEnabled(syncer::kChromeOSSyncedSessionSharing)) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  std::move(callback).Run(synced_session_client_->CreateRemote());
}

}  // namespace ash
