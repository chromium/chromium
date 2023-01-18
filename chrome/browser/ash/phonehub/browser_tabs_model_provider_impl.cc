// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h"

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace ash {
namespace phonehub {

BrowserTabsModelProviderImpl::BrowserTabsModelProviderImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    syncer::SyncService* sync_service,
    sync_sessions::SessionSyncService* session_sync_service,
    std::unique_ptr<BrowserTabsMetadataFetcher> browser_tabs_metadata_fetcher)
    : multidevice_setup_client_(multidevice_setup_client),
      sync_service_(sync_service),
      session_sync_service_(session_sync_service),
      browser_tabs_metadata_fetcher_(std::move(browser_tabs_metadata_fetcher)) {
  multidevice_setup_client_->AddObserver(this);
  session_updated_subscription_ =
      session_sync_service->SubscribeToForeignSessionsChanged(
          base::BindRepeating(
              &BrowserTabsModelProviderImpl::AttemptBrowserTabsModelUpdate,
              base::Unretained(this)));
  AttemptBrowserTabsModelUpdate();
}

BrowserTabsModelProviderImpl::~BrowserTabsModelProviderImpl() {
  multidevice_setup_client_->RemoveObserver(this);
}

absl::optional<std::string> BrowserTabsModelProviderImpl::GetSessionName()
    const {
  const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
      host_device_with_status = multidevice_setup_client_->GetHostStatus();
  if (!host_device_with_status.second)
    return absl::nullopt;
  // The pii_free_name field of the device matches the session name for
  // sync.
  return host_device_with_status.second->pii_free_name();
}

void BrowserTabsModelProviderImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  AttemptBrowserTabsModelUpdate();
}

void BrowserTabsModelProviderImpl::TriggerRefresh() {
  // crbug/1158480: Currently (January 2021), updates to synced sessions
  // sometimes take a long time to arrive. As a workaround,
  // SyncService::TriggerRefresh() is used, which bypasses some of the potential
  // sources of latency (e.g. for delivering an invalidation), but not others
  // (e.g. backend replication delay). I.e SyncService::TriggerRefresh() will
  // not guarantee an immediate update.
  sync_service_->TriggerRefresh({syncer::SESSIONS});
}

void BrowserTabsModelProviderImpl::AttemptBrowserTabsModelUpdate() {
  absl::optional<std::string> session_name = GetSessionName();
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service_->GetOpenTabsUIDelegate();
  // Tab sync is disabled or no valid |pii_free_name_|.
  if (!open_tabs || !session_name) {
    InvalidateWeakPtrsAndClearTabMetadata(/*is_tab_sync_enabled=*/false);
    return;
  }

  std::vector<const sync_sessions::SyncedSession*> sessions;
  bool was_fetch_successful = open_tabs->GetAllForeignSessions(&sessions);
  // No tabs were found, clear all tab metadata.
  if (!was_fetch_successful) {
    InvalidateWeakPtrsAndClearTabMetadata(/*is_tab_sync_enabled=*/true);
    return;
  }

  // |phone_session| should have the same |session_name| as the
  // |pii_free_name|. If there is more than one session with the same
  // |session_name| as the |pii_free_name|, as would be the case if a user has
  // multiple phones of the same type, |phone_session| will have the latest
  // |modified_time|.
  const sync_sessions::SyncedSession* phone_session = nullptr;
  for (const auto* session : sessions) {
    if (session->GetSessionName() != *session_name) {
      continue;
    }

    if (!phone_session ||
        phone_session->GetModifiedTime() < session->GetModifiedTime()) {
      phone_session = session;
    }
  }

  // No session with the same name as |pii_free_name_| was found, clear all
  // tab metadata.
  if (!phone_session) {
    InvalidateWeakPtrsAndClearTabMetadata(/*is_tab_sync_enabled=*/true);
    return;
  }

  browser_tabs_metadata_fetcher_->Fetch(
      phone_session,
      base::BindOnce(&BrowserTabsModelProviderImpl::OnMetadataFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserTabsModelProviderImpl::InvalidateWeakPtrsAndClearTabMetadata(
    bool is_tab_sync_enabled) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  NotifyBrowserTabsUpdated(
      /*is_tab_sync_enabled=*/is_tab_sync_enabled, {});
}

void BrowserTabsModelProviderImpl::OnMetadataFetched(
    absl::optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
        metadata) {
  // The operation to fetch metadata was cancelled.
  if (!metadata)
    return;
  NotifyBrowserTabsUpdated(
      /*is_tab_sync_enabled=*/true, *metadata);
}

}  // namespace phonehub
}  // namespace ash
