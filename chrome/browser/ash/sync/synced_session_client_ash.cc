// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/synced_session_client_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/sync_sessions/synced_session.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

ForeignSyncedSessionAsh::ForeignSyncedSessionAsh() = default;
ForeignSyncedSessionAsh::ForeignSyncedSessionAsh(
    const ForeignSyncedSessionAsh&) = default;
ForeignSyncedSessionAsh::ForeignSyncedSessionAsh(ForeignSyncedSessionAsh&&) =
    default;
ForeignSyncedSessionAsh& ForeignSyncedSessionAsh::operator=(
    const ForeignSyncedSessionAsh&) = default;
ForeignSyncedSessionAsh& ForeignSyncedSessionAsh::operator=(
    ForeignSyncedSessionAsh&&) = default;
ForeignSyncedSessionAsh::~ForeignSyncedSessionAsh() = default;

ForeignSyncedSessionWindowAsh::ForeignSyncedSessionWindowAsh() = default;
ForeignSyncedSessionWindowAsh::ForeignSyncedSessionWindowAsh(
    const ForeignSyncedSessionWindowAsh&) = default;
ForeignSyncedSessionWindowAsh::ForeignSyncedSessionWindowAsh(
    ForeignSyncedSessionWindowAsh&&) = default;
ForeignSyncedSessionWindowAsh& ForeignSyncedSessionWindowAsh::operator=(
    const ForeignSyncedSessionWindowAsh&) = default;
ForeignSyncedSessionWindowAsh& ForeignSyncedSessionWindowAsh::operator=(
    ForeignSyncedSessionWindowAsh&&) = default;
ForeignSyncedSessionWindowAsh::~ForeignSyncedSessionWindowAsh() = default;

ForeignSyncedSessionTabAsh::ForeignSyncedSessionTabAsh() = default;
ForeignSyncedSessionTabAsh::ForeignSyncedSessionTabAsh(
    const ForeignSyncedSessionTabAsh&) = default;
ForeignSyncedSessionTabAsh::ForeignSyncedSessionTabAsh(
    ForeignSyncedSessionTabAsh&&) = default;
ForeignSyncedSessionTabAsh& ForeignSyncedSessionTabAsh::operator=(
    const ForeignSyncedSessionTabAsh&) = default;
ForeignSyncedSessionTabAsh& ForeignSyncedSessionTabAsh::operator=(
    ForeignSyncedSessionTabAsh&&) = default;
ForeignSyncedSessionTabAsh::~ForeignSyncedSessionTabAsh() = default;

SyncedSessionClientAsh::SyncedSessionClientAsh() = default;
SyncedSessionClientAsh::~SyncedSessionClientAsh() = default;

mojo::PendingRemote<crosapi::mojom::SyncedSessionClient>
SyncedSessionClientAsh::CreateRemote() {
  mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> pending_receiver;
  mojo::PendingRemote<crosapi::mojom::SyncedSessionClient> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();
  receivers_.Add(this, std::move(pending_receiver));
  return {pending_remote.PassPipe(),
          crosapi::mojom::SyncedSessionClient::Version_};
}

void SyncedSessionClientAsh::OnForeignSyncedPhoneSessionsUpdated(
    std::vector<crosapi::mojom::SyncedSessionPtr> sessions) {
  // TODO(b/260599791): Implement the deserialization as a Mojom StructTrait as
  // a fast follow after initial prototype.
  last_foreign_synced_phone_sessions_.clear();
  for (const crosapi::mojom::SyncedSessionPtr& session : sessions) {
    ForeignSyncedSessionAsh current_session;
    current_session.session_name = session->session_name;
    current_session.modified_time = session->modified_time;
    for (const crosapi::mojom::SyncedSessionWindowPtr& window :
         session->windows) {
      ForeignSyncedSessionWindowAsh current_window;
      for (const crosapi::mojom::SyncedSessionTabPtr& tab : window->tabs) {
        ForeignSyncedSessionTabAsh current_tab;
        current_tab.current_navigation_title = tab->current_navigation_title;
        current_tab.last_modified_timestamp = tab->last_modified_timestamp;
        current_tab.current_navigation_url = tab->current_navigation_url;
        current_window.tabs.push_back(std::move(current_tab));
      }
      current_session.windows.push_back(std::move(current_window));
    }
    last_foreign_synced_phone_sessions_.push_back(std::move(current_session));
  }
  for (auto& observer : observers_) {
    observer.OnForeignSyncedPhoneSessionsUpdated(
        last_foreign_synced_phone_sessions_);
  }
}

void SyncedSessionClientAsh::OnSessionSyncEnabledChanged(bool enabled) {
  is_session_sync_enabled_ = enabled;
  for (auto& observer : observers_) {
    observer.OnSessionSyncEnabledChanged(is_session_sync_enabled_);
  }
}

void SyncedSessionClientAsh::SetFaviconDelegate(
    mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
        delegate) {
  if (favicon_delegate_.is_bound()) {
    favicon_delegate_.reset();
  }

  favicon_delegate_.Bind(std::move(delegate));
}

void SyncedSessionClientAsh::GetFaviconImageForPageURL(
    const GURL& url,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  if (!favicon_delegate_.is_bound()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  favicon_delegate_->GetFaviconImageForPageURL(url, std::move(callback));
}

void SyncedSessionClientAsh::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncedSessionClientAsh::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
