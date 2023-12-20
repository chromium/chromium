// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/crosapi_session_sync_notifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/lacros/sync/crosapi_session_sync_favicon_delegate.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_session.h"
#include "url/gurl.h"

namespace {
// Constructs a list of type crosapi::mojom::SyncedSessionTab from type
// std::unique_ptr<session::SessionTab>.
std::vector<crosapi::mojom::SyncedSessionTabPtr> ConstructSyncedPhoneTabs(
    const std::vector<std::unique_ptr<sessions::SessionTab>>& tabs) {
  std::vector<crosapi::mojom::SyncedSessionTabPtr> crosapi_synced_phone_tabs;
  for (const std::unique_ptr<sessions::SessionTab>& tab : tabs) {
    if (tab->navigations.empty()) {
      continue;
    }

    int selected_index = tab->normalized_navigation_index();
    const sessions::SerializedNavigationEntry& navigation =
        tab->navigations[selected_index];
    GURL tab_url = navigation.virtual_url();

    // URLs whose schemes are not http:// or https:// should be ignored
    // because they may be platform specific (e.g., chrome:// URLs) or may
    // refer to local media on the phone (e.g., content:// URLs).
    if (!tab_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    // If the url is incorrectly formatted, is empty, or has a
    // scheme that should be omitted, do not proceed with storing its
    // metadata.
    if (!tab_url.is_valid()) {
      continue;
    }

    crosapi_synced_phone_tabs.push_back(crosapi::mojom::SyncedSessionTab::New(
        tab_url, navigation.title(), tab->timestamp));
  }
  return crosapi_synced_phone_tabs;
}

// Constructs a list of type crosapi::mojom::SyncedSessionWindow from type
// sync_sessions::SyncedSessionWindow*.
std::vector<crosapi::mojom::SyncedSessionWindowPtr> ConstructSyncedPhoneWindows(
    const std::vector<sync_sessions::SyncedSessionWindow*>& windows) {
  std::vector<crosapi::mojom::SyncedSessionWindowPtr>
      crosapi_synced_phone_windows;
  for (const sync_sessions::SyncedSessionWindow* window : windows) {
    crosapi_synced_phone_windows.push_back(
        crosapi::mojom::SyncedSessionWindow::New(
            ConstructSyncedPhoneTabs(window->wrapped_window.tabs)));
  }
  return crosapi_synced_phone_windows;
}

// Constructs a list of type crosapi::mojom::SyncedSession from a list of type
// sync_sessions::SyncedSession for sessions with FormFactor kPhone from the
// latter list.
std::vector<crosapi::mojom::SyncedSessionPtr> ConstructSyncedPhoneSessions(
    const std::vector<raw_ptr<const sync_sessions::SyncedSession,
                              VectorExperimental>>& sessions) {
  std::vector<crosapi::mojom::SyncedSessionPtr> crosapi_synced_phone_sessions;
  for (const sync_sessions::SyncedSession* session : sessions) {
    if (session->GetDeviceFormFactor() !=
        syncer::DeviceInfo::FormFactor::kPhone) {
      continue;
    }
    std::vector<sync_sessions::SyncedSessionWindow*> windows;
    for (const auto& [window_id, window] : session->windows) {
      windows.push_back(window.get());
    }
    crosapi_synced_phone_sessions.push_back(crosapi::mojom::SyncedSession::New(
        session->GetSessionName(), session->GetModifiedTime(),
        ConstructSyncedPhoneWindows(windows)));
  }
  return crosapi_synced_phone_sessions;
}

bool IsTabSyncEnabled(syncer::SyncService* sync_service) {
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs);
}

}  // namespace

CrosapiSessionSyncNotifier::CrosapiSessionSyncNotifier(
    sync_sessions::SessionSyncService* session_sync_service,
    mojo::PendingRemote<crosapi::mojom::SyncedSessionClient>
        synced_session_client,
    syncer::SyncService* sync_service,
    favicon::HistoryUiFaviconRequestHandler* favicon_request_handler)
    : is_tab_sync_enabled_(IsTabSyncEnabled(sync_service)),
      session_sync_service_(session_sync_service),
      synced_session_client_(std::move(synced_session_client)),
      favicon_delegate_(favicon_request_handler) {
  if (synced_session_client_.version() >=
      static_cast<int>(crosapi::mojom::SyncedSessionClient::
                           kOnForeignSyncedPhoneSessionsUpdatedMinVersion)) {
    session_updated_subscription_ =
        session_sync_service->SubscribeToForeignSessionsChanged(
            base::BindRepeating(
                &CrosapiSessionSyncNotifier::OnForeignSyncedSessionsUpdated,
                base::Unretained(this)));
  }

  if (synced_session_client_.version() >=
      static_cast<int>(crosapi::mojom::SyncedSessionClient::
                           kOnSessionSyncEnabledChangedMinVersion)) {
    sync_service_observation_.Observe(sync_service);

    // Broadcast the initial value for |is_tab_sync_enabled_|.
    NotifySyncEnabledChanged();
  }

  if (synced_session_client_.version() >=
      static_cast<int>(
          crosapi::mojom::SyncedSessionClient::kSetFaviconDelegateMinVersion)) {
    synced_session_client_->SetFaviconDelegate(
        favicon_delegate_.CreateRemote());
  }
}

CrosapiSessionSyncNotifier::~CrosapiSessionSyncNotifier() = default;

void CrosapiSessionSyncNotifier::OnStateChanged(
    syncer::SyncService* sync_service) {
  bool is_tab_sync_enabled = IsTabSyncEnabled(sync_service);
  if (is_tab_sync_enabled == is_tab_sync_enabled_) {
    return;
  }

  is_tab_sync_enabled_ = is_tab_sync_enabled;
  NotifySyncEnabledChanged();
}

void CrosapiSessionSyncNotifier::NotifySyncEnabledChanged() {
  synced_session_client_->OnSessionSyncEnabledChanged(is_tab_sync_enabled_);
}

void CrosapiSessionSyncNotifier::OnForeignSyncedSessionsUpdated() {
  // Fetch sessions. Ensure |open_tabs| is not null since
  // GetOpenTabsUIDelegate() can return null if session sync is not running.
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    return;
  }

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      synced_sessions;
  open_tabs->GetAllForeignSessions(&synced_sessions);
  std::vector<crosapi::mojom::SyncedSessionPtr> crosapi_synced_phone_sessions =
      ConstructSyncedPhoneSessions(synced_sessions);

  synced_session_client_->OnForeignSyncedPhoneSessionsUpdated(
      std::move(crosapi_synced_phone_sessions));
}
