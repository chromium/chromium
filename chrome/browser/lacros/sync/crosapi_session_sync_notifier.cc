// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/crosapi_session_sync_notifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/sync_sessions/session_sync_service.h"

CrosapiSessionSyncNotifier::CrosapiSessionSyncNotifier(
    sync_sessions::SessionSyncService* session_sync_service,
    mojo::Remote<crosapi::mojom::SyncedSessionClient> synced_session_client)
    : session_sync_service_(session_sync_service),
      synced_session_client_(std::move(synced_session_client)) {
  session_updated_subscription_ =
      session_sync_service->SubscribeToForeignSessionsChanged(
          base::BindRepeating(
              &CrosapiSessionSyncNotifier::OnForeignSyncedSessionsUpdated,
              base::Unretained(this)));
}

CrosapiSessionSyncNotifier::~CrosapiSessionSyncNotifier() = default;

void CrosapiSessionSyncNotifier::OnForeignSyncedSessionsUpdated() {
  // TODO(b/260599791): In a follow-up CL we will fetch a list of sessions using
  // OpenTabsUIDelegate() and notify ash via `synced_session_client_`.
  NOTIMPLEMENTED();
}
