// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class SyncExplicitPassphraseClientLacros;
class SyncUserSettingsClientLacros;
class CrosapiSessionSyncNotifier;

namespace syncer {
class SyncService;
}  // namespace syncer

// Controls lifetime of sync-related Crosapi clients.
class SyncCrosapiManagerLacros : public syncer::SyncServiceObserver,
                                 public ProfileObserver {
 public:
  SyncCrosapiManagerLacros();
  ~SyncCrosapiManagerLacros() override;

  void PostProfileInit(Profile* profile);

  // SyncServiceObserver implementation.
  // Note: |this| observes only SyncService from the main profile.
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

 private:
  // ProfileObserver implementation.
  // Note: |this| observes only the main profile.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Creates a CrosapiSessionSyncNotifier after asynchronously acquiring a
  // PendingRemote from Ash. Exits early if the:
  //  - ChromeOS Synced Session Sharing is disabled.
  //  - Crosapi version used is not high enough to include the necessary updates
  //  made.
  //  - session sync service for the user's profile cannot be found.
  void MaybeCreateCrosapiSessionSyncNotifier();
  void OnCreateSyncedSessionClient(
      mojo::PendingRemote<crosapi::mojom::SyncedSessionClient> pending_remote);

  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  // The objects below are created for main profile PostProfileInit() and
  // destroyed upon main profile SyncService shutdown.
  std::unique_ptr<SyncExplicitPassphraseClientLacros>
      sync_explicit_passphrase_client_;
  std::unique_ptr<SyncUserSettingsClientLacros> sync_user_settings_client_;

  // This object will be destroyed on `OnProfileWillBeDestroyed()` call.
  std::unique_ptr<CrosapiSessionSyncNotifier> crosapi_session_sync_notifier_;

  base::WeakPtrFactory<SyncCrosapiManagerLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
