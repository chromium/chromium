// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_
#define CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lacros/sync/crosapi_session_sync_favicon_delegate.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace favicon {
class HistoryUiFaviconRequestHandler;
}  // namespace favicon

// This class is responsible for sending browser window data to Ash upon changes
// to foreign browser sessions.
class CrosapiSessionSyncNotifier : public syncer::SyncServiceObserver {
 public:
  // |session_sync_service| should not be null and should outlive |this|.
  // |sync_service| should not be null and should outlive |this|.
  // |favicon_request_handler| can be null but must outlive |this| if provided.
  CrosapiSessionSyncNotifier(
      sync_sessions::SessionSyncService* session_sync_service,
      mojo::PendingRemote<crosapi::mojom::SyncedSessionClient>
          synced_session_client,
      syncer::SyncService* sync_service,
      favicon::HistoryUiFaviconRequestHandler* favicon_request_handler);
  CrosapiSessionSyncNotifier(const CrosapiSessionSyncNotifier&) = delete;
  CrosapiSessionSyncNotifier& operator=(const CrosapiSessionSyncNotifier&) =
      delete;
  ~CrosapiSessionSyncNotifier() override;

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;

  void NotifySyncEnabledChanged();
  void OnForeignSyncedSessionsUpdated();

  bool is_tab_sync_enabled_ = false;
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  mojo::Remote<crosapi::mojom::SyncedSessionClient> synced_session_client_;
  base::CallbackListSubscription session_updated_subscription_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  CrosapiSessionSyncFaviconDelegate favicon_delegate_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_
