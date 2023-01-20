// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_

#include <memory>

#include "components/sync/driver/sync_service_observer.h"

class Profile;
class SyncExplicitPassphraseClientLacros;
class SyncUserSettingsClientLacros;
class CrosapiSessionSyncNotifier;

namespace syncer {
class SyncService;
}

// Controls lifetime of sync-related Crosapi clients.
class SyncCrosapiManagerLacros : public syncer::SyncServiceObserver {
 public:
  SyncCrosapiManagerLacros();
  ~SyncCrosapiManagerLacros() override;

  void PostProfileInit(Profile* profile);

  // SyncServiceObserver implementation.
  // Note: |this| observes only SyncService from the main profile.
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

 private:
  // The objects below are created for main profile PostProfileInit() and
  // destroyed upon main profile SyncService shutdown.
  std::unique_ptr<SyncExplicitPassphraseClientLacros>
      sync_explicit_passphrase_client_;
  std::unique_ptr<SyncUserSettingsClientLacros> sync_user_settings_client_;
  std::unique_ptr<CrosapiSessionSyncNotifier> crosapi_session_sync_notifier_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
