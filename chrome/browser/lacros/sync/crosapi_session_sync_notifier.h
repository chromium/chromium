// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_
#define CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// This class is responsible for sending browser window data to Ash upon changes
// to foreign browser sessions.
class CrosapiSessionSyncNotifier {
 public:
  // `session_sync_service` should not be null and should outlive `this`.
  CrosapiSessionSyncNotifier(
      sync_sessions::SessionSyncService* session_sync_service,
      mojo::Remote<crosapi::mojom::SyncedSessionClient> synced_session_client);
  CrosapiSessionSyncNotifier(const CrosapiSessionSyncNotifier&) = delete;
  CrosapiSessionSyncNotifier& operator=(const CrosapiSessionSyncNotifier&) =
      delete;
  ~CrosapiSessionSyncNotifier();

 private:
  void OnForeignSyncedSessionsUpdated();

  base::raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  mojo::Remote<crosapi::mojom::SyncedSessionClient> synced_session_client_;
  base::CallbackListSubscription session_updated_subscription_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_