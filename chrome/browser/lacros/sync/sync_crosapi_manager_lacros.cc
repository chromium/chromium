// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/lacros/sync/crosapi_session_sync_notifier.h"
#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"
#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Version of crosapi that is guaranteed to have SyncUserSettingsClient API (
// exposed by SyncService crosapi).
const uint32_t kMinCrosapiVersionWithSyncUserSettingsClient = 80;

// Creates SyncExplicitPassphraseClientLacros if preconditions are met, returns
// nullptr otherwise. Preconditions are:
// 1. Sync passphrase sharing feature is enabled.
// 2. SyncService crosapi is available.
// `lacros_service` and `sync_service` must not be null.
std::unique_ptr<SyncExplicitPassphraseClientLacros>
MaybeCreateSyncExplicitPassphraseClient(chromeos::LacrosService* lacros_service,
                                        syncer::SyncService* sync_service) {
  DCHECK(lacros_service);
  DCHECK(sync_service);

  if (!base::FeatureList::IsEnabled(
          syncer::kSyncChromeOSExplicitPassphraseSharing)) {
    return nullptr;
  }

  if (!lacros_service->IsAvailable<crosapi::mojom::SyncService>()) {
    return nullptr;
  }

  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> client_remote;
  lacros_service->GetRemote<crosapi::mojom::SyncService>()
      ->BindExplicitPassphraseClient(
          client_remote.BindNewPipeAndPassReceiver());
  return std::make_unique<SyncExplicitPassphraseClientLacros>(
      std::move(client_remote), sync_service);
}

// Creates SyncUserSettingsClientLacros if preconditions are met, returns
// nullptr otherwise. Preconditions are:
// 1. Sync apps toggle sharing feature is enabled.
// 2. SyncService crosapi is available.
// 3. SyncService crosapi exposes SyncUserSettingsClient (e.g. Ash-side crosapi
// has sufficient version).
// `lacros_service` and `sync_service` must not be null.
std::unique_ptr<SyncUserSettingsClientLacros> MaybeCreateSyncUserSettingsClient(
    chromeos::LacrosService* lacros_service,
    syncer::SyncUserSettings* sync_user_settings) {
  DCHECK(lacros_service);
  DCHECK(sync_user_settings);

  if (!base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    return nullptr;
  }

  if (!lacros_service->IsAvailable<crosapi::mojom::SyncService>()) {
    return nullptr;
  }

  if (chromeos::BrowserParamsProxy::Get()->CrosapiVersion() <
      kMinCrosapiVersionWithSyncUserSettingsClient) {
    return nullptr;
  }

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> client_remote;
  lacros_service->GetRemote<crosapi::mojom::SyncService>()
      ->BindUserSettingsClient(client_remote.BindNewPipeAndPassReceiver());
  return std::make_unique<SyncUserSettingsClientLacros>(
      std::move(client_remote), sync_user_settings);
}

}  // namespace

SyncCrosapiManagerLacros::SyncCrosapiManagerLacros() = default;

SyncCrosapiManagerLacros::~SyncCrosapiManagerLacros() = default;

void SyncCrosapiManagerLacros::PostProfileInit(Profile* profile) {
  if (!profile->IsMainProfile()) {
    return;
  }

  DCHECK(!profile_);
  profile_ = profile;
  auto* lacros_service = chromeos::LacrosService::Get();
  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  if (!lacros_service || !sync_service) {
    return;
  }
  sync_service->AddObserver(this);

  DCHECK(!crosapi_session_sync_notifier_);
  profile->AddObserver(this);
  MaybeCreateCrosapiSessionSyncNotifier();

  DCHECK(!sync_explicit_passphrase_client_);
  sync_explicit_passphrase_client_ =
      MaybeCreateSyncExplicitPassphraseClient(lacros_service, sync_service);

  DCHECK(!sync_user_settings_client_);
  sync_user_settings_client_ = MaybeCreateSyncUserSettingsClient(
      lacros_service, sync_service->GetUserSettings());
}

void SyncCrosapiManagerLacros::OnProfileWillBeDestroyed(Profile* profile) {
  profile_ = nullptr;
  crosapi_session_sync_notifier_.reset();
  profile->RemoveObserver(this);
}

void SyncCrosapiManagerLacros::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  sync_explicit_passphrase_client_.reset();
  sync_user_settings_client_.reset();
  sync_service->RemoveObserver(this);
}

void SyncCrosapiManagerLacros::MaybeCreateCrosapiSessionSyncNotifier() {
  if (!base::FeatureList::IsEnabled(syncer::kChromeOSSyncedSessionSharing)) {
    return;
  }

  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->GetInterfaceVersion<crosapi::mojom::SyncService>() <
      static_cast<int>(
          crosapi::mojom::SyncService::kCreateSyncedSessionClientMinVersion)) {
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::SyncService>()
      ->CreateSyncedSessionClient(
          base::BindOnce(&SyncCrosapiManagerLacros::OnCreateSyncedSessionClient,
                         weak_ptr_factory_.GetWeakPtr()));
}

void SyncCrosapiManagerLacros::OnCreateSyncedSessionClient(
    mojo::PendingRemote<crosapi::mojom::SyncedSessionClient> pending_remote) {
  if (!pending_remote || !profile_) {
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (!sync_service) {
    return;
  }

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  if (!session_sync_service) {
    return;
  }

  favicon::HistoryUiFaviconRequestHandler* favicon_request_handler =
      HistoryUiFaviconRequestHandlerFactory::GetInstance()
          ->GetForBrowserContext(profile_);

  DCHECK(!crosapi_session_sync_notifier_);
  crosapi_session_sync_notifier_ = std::make_unique<CrosapiSessionSyncNotifier>(
      session_sync_service, std::move(pending_remote), sync_service,
      favicon_request_handler);
}
