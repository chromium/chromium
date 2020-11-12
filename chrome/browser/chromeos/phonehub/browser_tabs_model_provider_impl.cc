// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/phonehub/browser_tabs_model_provider_impl.h"

#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/components/phonehub/browser_tabs_model.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace chromeos {
namespace phonehub {

BrowserTabsModelProviderImpl::BrowserTabsModelProviderImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    sync_sessions::SessionSyncService* session_sync_service,
    std::unique_ptr<BrowserTabsMetadataFetcher> browser_tabs_metadata_fetcher)
    : multidevice_setup_client_(multidevice_setup_client),
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

base::Optional<std::string> BrowserTabsModelProviderImpl::GetSessionName()
    const {
  const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
      host_device_with_status = multidevice_setup_client_->GetHostStatus();
  if (!host_device_with_status.second)
    return base::nullopt;
  // The pii_free_name field of the device matches the session name for
  // sync.
  return host_device_with_status.second->pii_free_name();
}

void BrowserTabsModelProviderImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  AttemptBrowserTabsModelUpdate();
}

void BrowserTabsModelProviderImpl::AttemptBrowserTabsModelUpdate() {
  base::Optional<std::string> session_name = GetSessionName();
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
    if (session->session_name != *session_name)
      continue;

    if (!phone_session ||
        phone_session->modified_time < session->modified_time) {
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
  BrowserTabsModelProvider::NotifyBrowserTabsUpdated(
      /*is_tab_sync_enabled=*/is_tab_sync_enabled, {});
}

void BrowserTabsModelProviderImpl::OnMetadataFetched(
    base::Optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
        metadata) {
  // The operation to fetch metadata was cancelled.
  if (!metadata)
    return;
  BrowserTabsModelProvider::NotifyBrowserTabsUpdated(
      /*is_tab_sync_enabled=*/true, *metadata);
}

}  // namespace phonehub
}  // namespace chromeos
